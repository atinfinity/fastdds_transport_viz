# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""web/serve.py with a fake transport_viz: framing, /latest.json, SSE, shutdown on exit."""
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


def alive(pid):
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    return True


def start(tmp_path, src=FAKE, extra=()):
    fake = tmp_path / 'fake_transport_viz.py'
    fake.write_text(src)
    fake.chmod(0o755)
    proc = subprocess.Popen(
        [sys.executable, str(SERVE), '--port', '0', '--transport-viz', str(fake),
         str(SAMPLE), *extra],
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
