# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""web/serve.py with a fake transport_viz: framing, /latest.json, SSE, shutdown on producer exit."""
import json
import os
import pathlib
import re
import subprocess
import sys
import textwrap
import time
import urllib.request

REPO = pathlib.Path(__file__).resolve().parents[3]
SERVE = REPO / 'web' / 'serve.py'
SAMPLE = REPO / 'web' / 'sample' / 'sample.json'

FAKE = textwrap.dedent('''\
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
''')


def start(tmp_path):
    fake = tmp_path / 'fake_transport_viz.py'
    fake.write_text(FAKE)
    fake.chmod(0o755)
    proc = subprocess.Popen(
        [sys.executable, str(SERVE), '--port', '0', '--transport-viz', str(fake), str(SAMPLE)],
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
            for raw in resp:
                line = raw.decode().rstrip('\n')
                if line.startswith('event: '):
                    event['event'] = line[7:]
                elif line.startswith('data: '):
                    event['data'] = json.loads(line[6:])
                elif line == '' and event:
                    events.append(event)
                    event = {}
                    if events[-1]['event'] == 'status':
                        break
        docs = [e['data'] for e in events if e['event'] == 'document']
        assert docs, events
        assert docs[0]['schema_version'] == 1
        assert docs[-1]['observed_at'] == 'frame-2'
        assert events[-1]['data']['state'] == 'ended'
        assert 'exited with code 0' in events[-1]['data']['message']
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
