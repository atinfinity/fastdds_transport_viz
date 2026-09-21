# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""web/serve.py with a fake transport_viz: framing, /latest.json, SSE, /metrics, shutdown."""
import calendar
import json
import os
import pathlib
import re
import signal
import subprocess
import sys
import textwrap
import time
import urllib.request

import pytest

REPO = pathlib.Path(__file__).resolve().parents[3]
SERVE = REPO / 'web' / 'serve.py'
SAMPLE = REPO / 'web' / 'sample' / 'sample.json'
RECORDING = REPO / 'web' / 'sample' / 'recording.jsonl'

FAKE = textwrap.dedent("""\
    #!/usr/bin/env python3
    import json, sys, time
    assert sys.argv[1:3] == ['--watch', '--json'], sys.argv
    doc = json.load(open(sys.argv[3]))
    for i in range(3):
        doc['observed_at'] = f'frame-{i}'
        print(json.dumps(doc), flush=True)
        time.sleep(0.2)
    print('garbage that is not json', flush=True)
    sys.exit(0)
""")

# A producer that keeps running and stops writing: the real transport_viz would die of
# SIGPIPE on its next frame, which is what hid the missing cleanup in #195. It records its
# pid, and the signal it is asked to stop with, in the files given after the document.
FAKE_FOREVER = textwrap.dedent("""\
    #!/usr/bin/env python3
    import json, os, signal, sys, time
    assert sys.argv[1:3] == ['--watch', '--json'], sys.argv
    doc = json.load(open(sys.argv[3]))
    pid_file, marker = sys.argv[4], sys.argv[5]

    def bye(signum, frame):
        open(marker, 'w').write(str(signum))
        sys.exit(0)

    signal.signal(signal.SIGTERM, bye)
    doc['observed_at'] = 'frame-0'
    print(json.dumps(doc), flush=True)
    open(pid_file, 'w').write(str(os.getpid()))
    while True:
        time.sleep(0.05)
""")


# Holds its document back until the file given after it exists, then keeps running: /metrics
# is scraped before the first document and after it.
FAKE_GATED = textwrap.dedent("""\
    #!/usr/bin/env python3
    import json, os, sys, time
    assert sys.argv[1:3] == ['--watch', '--json'], sys.argv
    doc = json.load(open(sys.argv[3]))
    while not os.path.exists(sys.argv[4]):
        time.sleep(0.05)
    print(json.dumps(doc), flush=True)
    while True:
        time.sleep(0.05)
""")


def alive(pid):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    return True


def start(tmp_path, src=FAKE, extra=(), doc=SAMPLE):
    fake = tmp_path / 'fake_transport_viz.py'
    fake.write_text(src)
    fake.chmod(0o755)
    proc = subprocess.Popen(
        [sys.executable, str(SERVE), '--port', '0', '--transport-viz', str(fake),
         str(doc), *extra],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    line = proc.stdout.readline()
    m = re.search(r'listening on (http://[^/]+)/', line)
    assert m, line
    return proc, m.group(1)


def test_latest_and_events_then_shutdown(tmp_path):
    proc, base = start(tmp_path)
    try:
        # SSE: first event is a document, later ones follow, then a status event
        with urllib.request.urlopen(f'{base}/events', timeout=10) as resp:
            assert resp.headers['Content-Type'].startswith('text/event-stream')
            events = []
            event = {}
            retry = None
            for raw in resp:
                line = raw.decode().rstrip('\n')
                if line.startswith('retry: '):
                    retry = int(line[7:])
                elif line.startswith('id: '):
                    event['id'] = int(line[4:])
                elif line.startswith('event: '):
                    event['event'] = line[7:]
                elif line.startswith('data: '):
                    event['data'] = json.loads(line[6:])
                elif line == '' and event:
                    events.append(event)
                    event = {}
                    if events[-1]['event'] == 'status':
                        break
        # the stream asks the browser to reconnect after a second, not after its own
        # default of three (#191)
        assert retry == 1000, events
        docs = [e['data'] for e in events if e['event'] == 'document']
        assert docs, events
        assert docs[0]['schema_version'] == 1
        assert docs[-1]['observed_at'] == 'frame-2'
        assert events[-1]['data']['state'] == 'ended'
        assert 'exited with code 0' in events[-1]['data']['message']
        # every document carries its number in the stream, the unparsable line has none;
        # the viewer drops a repeated id and counts the gaps as skipped frames (#218)
        ids = [e['id'] for e in events if e['event'] == 'document']
        assert ids == sorted(set(ids)), events
        assert ids[-1] == 3, ids
        assert 'id' not in events[-1], events
        # producer finished => server shuts down by itself
        assert proc.wait(timeout=10) == 0
    finally:
        if proc.poll() is None:
            proc.kill()


def test_latest_json(tmp_path):
    proc, base = start(tmp_path)
    try:
        deadline = time.time() + 5
        doc = None
        while time.time() < deadline:
            try:
                with urllib.request.urlopen(f'{base}/latest.json', timeout=5) as resp:
                    doc = json.load(resp)
                    break
            except urllib.error.HTTPError as e:   # 503 until the first document arrives
                assert e.code == 503
                time.sleep(0.05)
        assert doc and doc['schema_version'] == 1
        with urllib.request.urlopen(f'{base}/', timeout=5) as resp:   # redirect to the live viewer
            assert resp.url.endswith('/index.html?live=1')
            assert b'<title>fastdds_transport_viz viewer</title>' in resp.read()
    finally:
        proc.kill()
        proc.wait()


@pytest.mark.parametrize('signame', ['SIGTERM', 'SIGHUP'])
def test_signal_reaps_transport_viz(tmp_path, signame):
    # Ctrl-C unwinds into the cleanup; SIGTERM and SIGHUP have to be made to do the same,
    # or transport_viz outlives the server that started it (#195).
    signum = getattr(signal, signame, None)
    if signum is None:
        pytest.skip(f'{signame} is not available on this platform')
    pid_file = tmp_path / 'child.pid'
    marker = tmp_path / 'child.signal'
    proc, _ = start(tmp_path, src=FAKE_FOREVER, extra=[str(pid_file), str(marker)])
    child = None
    try:
        deadline = time.time() + 10
        while time.time() < deadline and not pid_file.is_file():
            time.sleep(0.05)
        child = int(pid_file.read_text())
        assert alive(child)
        proc.send_signal(signum)
        assert proc.wait(timeout=10) == 0
        deadline = time.time() + 5
        while time.time() < deadline and alive(child):
            time.sleep(0.05)
        assert not alive(child), f'transport_viz ({child}) outlived transport_viz_web'
        # reaped with SIGTERM, not killed: the child ran its own shutdown
        assert marker.read_text() == str(int(signal.SIGTERM))
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        if child is not None and alive(child):
            os.kill(child, signal.SIGKILL)


def test_record_writes_every_line_as_it_came(tmp_path):
    # --record keeps what `transport_viz --watch --json > FILE` would have: every line,
    # the unparsable one included, in order (#82)
    rec = tmp_path / 'rec.jsonl'
    proc, _ = start(tmp_path, extra=['--record', str(rec)])
    try:
        assert proc.wait(timeout=10) == 0
        stderr = proc.stderr.read()
    finally:
        if proc.poll() is None:
            proc.kill()
    assert f'recording to {rec}' in stderr
    lines = rec.read_text().splitlines()
    assert len(lines) == 4, lines
    observed = [json.loads(line)['observed_at'] for line in lines[:3]]
    assert observed == ['frame-0', 'frame-1', 'frame-2']
    assert lines[3] == 'garbage that is not json'


def test_record_to_an_unwritable_path_fails_before_starting(tmp_path):
    proc = subprocess.run(
        [sys.executable, str(SERVE), '--port', '0', '--transport-viz', '/bin/false',
         '--record', str(tmp_path / 'missing' / 'rec.jsonl')],
        capture_output=True, text=True, timeout=10)
    assert proc.returncode == 1
    assert 'cannot write the recording' in proc.stderr
    assert 'listening' not in proc.stdout


SAMPLE_LINE = re.compile(r'^([a-zA-Z_:][a-zA-Z0-9_:]*)(?:\{(.*)\})? (\S+)$')
LABEL = re.compile(r'([a-zA-Z_][a-zA-Z0-9_]*)="((?:[^"\\]|\\.)*)",?')


def parse_metrics(text):
    """Check the Prometheus text format and return {(name, labels): value}."""
    assert text.endswith('\n'), text
    helped, typed, series = set(), {}, {}
    for line in text.splitlines():
        if line.startswith('# HELP '):
            helped.add(line.split(' ')[2])
        elif line.startswith('# TYPE '):
            _, _, name, kind = line.split(' ')
            assert kind in ('gauge', 'counter'), line
            assert name not in typed, f'{name} typed twice'
            typed[name] = kind
        else:
            m = SAMPLE_LINE.match(line)
            assert m, line
            name, body, value = m.groups()
            assert name in helped and name in typed, f'{name} without HELP/TYPE before it'
            labels = []
            rest = body or ''
            while rest:
                lm = LABEL.match(rest)
                assert lm, (line, rest)
                raw = lm.group(2)
                labels.append((lm.group(1), re.sub(
                    r'\\(.)', lambda e: '\n' if e.group(1) == 'n' else e.group(1), raw)))
                rest = rest[lm.end():]
            key = (name, tuple(sorted(labels)))
            assert key not in series, f'duplicate series {key}'
            series[key] = float(value)
    return series


def scrape(base):
    with urllib.request.urlopen(f'{base}/metrics', timeout=5) as resp:
        assert resp.status == 200
        assert resp.headers['Content-Type'].startswith('text/plain; version=0.0.4')
        return parse_metrics(resp.read().decode())


def values(series, name):
    return {dict(labels).get('stat') or dict(labels).get('transport') or
            dict(labels).get('code') or '': v
            for (n, labels), v in series.items() if n == name}


def test_metrics_before_and_after_the_first_document(tmp_path):
    # #83: a measured document (the recording's last frame) with a node name that needs
    # escaping, a second pair of the same nodes (only the GUIDs tell the two apart) and a
    # warning; scraped before transport_viz printed anything, and after
    doc = json.loads(RECORDING.read_text().splitlines()[-1])
    topic = next(t for t in doc['topics'] if t['pairs'] and t['pairs'][0]['measured']['available'])
    pair = topic['pairs'][0]
    pair['writer_node'] = '/odd"\\name\nx'
    pair['warnings'] = ['split-but-non-shm-traffic']
    twin = json.loads(json.dumps(pair))
    twin['reader_guid'] = twin['reader_guid'][:-1] + 'f'
    twin['measured']['latency_s'] = None
    topic['pairs'].append(twin)
    doc['shm'].update(available=True, used_bytes=40, stale_ports=2)
    doc_file = tmp_path / 'doc.json'
    doc_file.write_text(json.dumps(doc))
    gate = tmp_path / 'go'
    proc, base = start(tmp_path, src=FAKE_GATED, extra=[str(gate)], doc=doc_file)
    try:
        before = scrape(base)
        assert before == {('transport_viz_up', ()): 1.0,
                          ('transport_viz_documents_total', ()): 0.0}, before
        gate.write_text('')
        deadline = time.time() + 5
        while time.time() < deadline:
            series = scrape(base)
            if series[('transport_viz_documents_total', ())] == 1:
                break
            time.sleep(0.05)
        assert series[('transport_viz_documents_total', ())] == 1
        assert series[('transport_viz_stats_enabled', ())] == 1
        assert values(series, 'transport_viz_info') == {'': 1.0}
        info = next(dict(k[1]) for k in series if k[0] == 'transport_viz_info')
        assert info == {'domain': str(doc['domain']), 'schema_version': '1'}
        # the pair labels: the escaped node name reads back as it was
        latency = [dict(k[1]) for k in series if k[0] == 'transport_viz_pair_latency_seconds']
        assert {lb['stat'] for lb in latency} == {'mean', 'min', 'max', 'last'}
        assert {lb['writer_node'] for lb in latency} == {'/odd"\\name\nx'}
        assert set(latency[0]) == {'topic', 'writer_node', 'reader_node', 'writer_host',
                                   'reader_host', 'writer_guid', 'reader_guid', 'stat'}
        assert latency[0]['reader_guid'] == pair['reader_guid']   # the twin has no latency
        m = pair['measured']
        assert sorted(values(series, 'transport_viz_pair_latency_seconds').values()) == \
            sorted(m['latency_s'][k] for k in ('mean', 'min', 'max', 'last'))
        # both pairs: the value series, the predicted and measured transports, the warning
        for name in ('transport_viz_pair_packets', 'transport_viz_pair_bytes',
                     'transport_viz_pair_transport', 'transport_viz_pair_warning',
                     'transport_viz_pair_measured_transport'):
            assert len([k for k in series if k[0] == name]) == 2 * (
                len(m['transports']) if name.endswith('measured_transport') else 1), name
        assert values(series, 'transport_viz_pair_transport') == {pair['transport']: 1.0}
        assert values(series, 'transport_viz_pair_warning') == {'split-but-non-shm-traffic': 1.0}
        assert set(values(series, 'transport_viz_pair_packets').values()) == {m['packets']}
        # /dev/shm of this host, under the label the pairs give it
        shm = {k[0]: (dict(k[1]), v) for k, v in series.items() if k[0].startswith(
            'transport_viz_shm_')}
        assert len(shm) == 8, shm
        assert shm['transport_viz_shm_used_bytes'][1] == 40
        assert shm['transport_viz_shm_stale_ports'][1] == 2
        assert shm['transport_viz_shm_used_bytes'][0]['host'] == pair['reader_host']
        ts = series[('transport_viz_last_document_timestamp_seconds', ())]
        assert ts == calendar.timegm(time.strptime(doc['observed_at'], '%Y-%m-%dT%H:%M:%SZ'))
        assert series[('transport_viz_up', ())] == 1
    finally:
        proc.kill()
        proc.wait()


def test_metrics_without_statistics_and_after_the_end(tmp_path):
    # a document observed without --stats: no measured series at all; then the stream ends
    proc, base = start(tmp_path, doc=REPO / 'web' / 'sample' / 'diff_after.json')
    try:
        deadline = time.time() + 10
        series = {}
        while time.time() < deadline:
            try:
                series = scrape(base)
            except (urllib.error.URLError, ConnectionError):
                break   # the server went down after the producer exited
            if series[('transport_viz_up', ())] == 0:
                break
            time.sleep(0.05)
        assert series[('transport_viz_up', ())] == 0, series
        assert series[('transport_viz_documents_total', ())] == 3
        assert series[('transport_viz_stats_enabled', ())] == 0
        names = {k[0] for k in series}
        assert 'transport_viz_pair_transport' in names
        assert not {n for n in names if n.startswith('transport_viz_pair_') and
                    n not in ('transport_viz_pair_transport', 'transport_viz_pair_warning')}, names
    finally:
        if proc.poll() is None:
            proc.kill()
        proc.wait()
