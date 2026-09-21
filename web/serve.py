#!/usr/bin/env python3
# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""transport_viz_web: serve the web viewer and stream live transport_viz output.

Runs ``transport_viz --watch --json`` (JSON Lines) as a single subprocess, keeps
the latest document, and serves:

  /              -> redirect to /index.html?live=1
  /index.html …  -> the static viewer (web/ directory next to this script, or
                    share/fastdds_transport_viz/web when installed)
  /latest.json   -> the most recent document
  /metrics       -> the most recent document in the Prometheus text format (#83): a gauge
                    per measured value of every writer -> reader pair, info series for its
                    predicted and measured transports and its warnings, the /dev/shm usage
                    of this host, and the state of the stream
  /events        -> Server-Sent Events: a retry interval, the latest document on
                    connect, then every new document ("document" events, each
                    with ``id:`` its number in this server's stream: a client
                    that fell behind receives only the newest, and the viewer's
                    live history counts the skipped ones by the gaps, #218); a
                    "status" event when the stream ends

With ``--record FILE`` every line transport_viz prints is also written to FILE as it
arrives, which is what ``transport_viz --watch --json > FILE`` would have written: a
recording the viewer replays with a timeline (#82).

Standard library only. Unknown command-line options are forwarded verbatim to
transport_viz (e.g. --stats, --interval 1, --domain 3, --all, --topic REGEX).
"""
import argparse
import datetime
import http.server
import json
import os
import shutil
import signal
import subprocess
import sys
import threading
import time
import urllib.parse


class Stream:
    """Latest document plus a condition variable for SSE subscribers."""

    def __init__(self):
        self.latest = None
        self.seq = 0
        self.ended = None          # message once the producer has stopped
        self.cond = threading.Condition()

    def push(self, doc):
        with self.cond:
            self.latest = doc
            self.seq += 1
            self.cond.notify_all()

    def end(self, message):
        with self.cond:
            self.ended = message
            self.cond.notify_all()

    def wait(self, seen_seq, timeout):
        """Block until a newer document exists or the stream ended; return (seq, latest, ended)."""
        with self.cond:
            self.cond.wait_for(lambda: self.seq != seen_seq or self.ended is not None, timeout)
            return self.seq, self.latest, self.ended


def pump(proc, stream, verbose, record=None):
    """Read JSON Lines from the subprocess into the stream (and the recording)."""
    for raw in proc.stdout:
        line = raw.strip()
        if not line:
            continue
        if record is not None:
            # as it came, unparsable lines included: the file is the one a shell
            # redirection would have written, flushed per line so an interrupted
            # recording loses at most the line being written
            record.write(raw if raw.endswith('\n') else raw + '\n')
            record.flush()
        try:
            doc = json.loads(line)
        except json.JSONDecodeError as e:
            print(f'transport_viz_web: ignoring unparsable line: {e}', file=sys.stderr)
            continue
        stream.push(doc)
        if verbose:
            print(f'transport_viz_web: document #{stream.seq} ({len(line)} bytes)', file=sys.stderr)
    rc = proc.wait()
    stream.end(f'transport_viz exited with code {rc}')


# One label set per pair: the GUIDs keep two pairs of the same nodes (or of nodes without a
# name) apart, and the transport is not among them, so a transport change does not cut the
# value series; it is told by the info series instead (#83).
PAIR_LABELS = ('topic', 'writer_node', 'reader_node', 'writer_host', 'reader_host',
               'writer_guid', 'reader_guid')

# name, type, HELP text; every family is a gauge except documents_total (the document values are
# windows or observations the producer can restart, not counters)
METRICS = (
    ('up', 'gauge', '1 while transport_viz is running, 0 once it has exited.'),
    ('documents_total', 'counter', 'Documents received from transport_viz.'),
    ('info', 'gauge', 'The domain and schema version of the latest document (always 1).'),
    ('last_document_timestamp_seconds', 'gauge',
     'observed_at of the latest document, seconds since the epoch.'),
    ('stats_enabled', 'gauge', '1 when transport_viz runs with --stats.'),
    ('pair_transport', 'gauge', 'The transport predicted for the pair (always 1).'),
    ('pair_measured_transport', 'gauge',
     'A transport that carried packets of the pair (always 1; --stats).'),
    ('pair_warning', 'gauge', 'A warning code of the pair (always 1).'),
    ('pair_packets', 'gauge', 'RTPS packets sent to the reader during the observation.'),
    ('pair_bytes', 'gauge', 'RTPS bytes sent to the reader during the observation.'),
    ('pair_delivered_per_second', 'gauge',
     'Samples delivered per second inside the rate window (a lower bound when the '
     "reader's statistics lost samples)."),
    ('pair_latency_seconds', 'gauge', 'Write-to-notification latency of the pair.'),
    ('pair_lost_packets', 'gauge',
     "RTPS_LOST packets the reader's participant missed from the writer's participant."),
    ('pair_resent_datas', 'gauge', 'DATA submessages the writer sent again.'),
    ('shm_total_bytes', 'gauge', 'Size of the shared-memory file system of this host.'),
    ('shm_used_bytes', 'gauge', 'Used bytes of the shared-memory file system.'),
    ('shm_free_bytes', 'gauge', 'Free bytes of the shared-memory file system.'),
    ('shm_fastdds_bytes', 'gauge', 'Bytes of the Fast DDS files in it.'),
    ('shm_segments', 'gauge', 'Fast DDS shared-memory segments.'),
    ('shm_ports', 'gauge', 'Fast DDS shared-memory ports.'),
    ('shm_stale_segments', 'gauge', 'Segments no live process holds.'),
    ('shm_stale_ports', 'gauge', 'Ports no live process holds.'),
)


def _label_value(value):
    return str(value).replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')


def _number(value):
    if isinstance(value, bool):
        return '1' if value else '0'
    return repr(float(value)) if isinstance(value, float) else str(value)


def _timestamp(observed_at):
    try:
        at = datetime.datetime.fromisoformat(str(observed_at).replace('Z', '+00:00'))
    except ValueError:
        return None
    if at.tzinfo is None:
        return None
    return at.timestamp()


def _local_host(doc):
    """The label the document gives the tool's own host, as the pair labels spell it."""
    for participant in doc.get('participants') or []:
        if participant.get('host_id') == doc.get('local_host_id') and participant.get('host'):
            return participant['host']
    return 'local'


def metrics_text(doc, documents, ended):
    """The Prometheus text format of the latest document; a null value has no series."""
    samples = {name: [] for name, _, _ in METRICS}

    def add(name, labels, value):
        if value is not None:
            samples[name].append((labels, value))

    add('up', {}, 0 if ended else 1)
    add('documents_total', {}, documents)
    if doc is not None:
        add('info', {'domain': doc.get('domain'), 'schema_version': doc.get('schema_version')}, 1)
        add('last_document_timestamp_seconds', {}, _timestamp(doc.get('observed_at')))
        add('stats_enabled', {}, bool((doc.get('stats') or {}).get('enabled')))
        for topic in doc.get('topics') or []:
            for pair in topic.get('pairs') or []:
                base = {'topic': topic.get('topic')}
                base.update((k, pair.get(k)) for k in PAIR_LABELS[1:])
                add('pair_transport', {**base, 'transport': pair.get('transport')}, 1)
                for code in pair.get('warnings') or []:
                    add('pair_warning', {**base, 'code': code}, 1)
                m = pair.get('measured') or {}
                if not m.get('available'):
                    continue
                for transport in m.get('transports') or []:
                    add('pair_measured_transport', {**base, 'transport': transport}, 1)
                add('pair_packets', base, m.get('packets'))
                add('pair_bytes', base, m.get('bytes'))
                add('pair_delivered_per_second', base, m.get('delivered_per_s'))
                for stat, value in (m.get('latency_s') or {}).items():
                    if stat != 'samples':
                        add('pair_latency_seconds', {**base, 'stat': stat}, value)
                reliability = m.get('reliability') or {}
                add('pair_lost_packets', base, reliability.get('lost_packets'))
                add('pair_resent_datas', base, reliability.get('resent_datas'))
        shm = doc.get('shm') or {}
        if shm.get('available'):
            for key in ('total_bytes', 'used_bytes', 'free_bytes', 'fastdds_bytes', 'segments',
                        'ports', 'stale_segments', 'stale_ports'):
                add('shm_' + key, {'host': _local_host(doc)}, shm.get(key))
    lines = []
    for name, kind, help_text in METRICS:
        if not samples[name]:
            continue
        full = 'transport_viz_' + name
        lines.append(f'# HELP {full} {help_text}')
        lines.append(f'# TYPE {full} {kind}')
        for labels, value in samples[name]:
            text = ','.join(f'{k}="{_label_value(v)}"' for k, v in labels.items())
            lines.append(f'{full}{{{text}}} {_number(value)}' if text else
                         f'{full} {_number(value)}')
    return '\n'.join(lines) + '\n'


def make_handler(web_dir, stream, verbose):
    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=web_dir, **kwargs)

        def log_message(self, fmt, *args):
            if verbose:
                super().log_message(fmt, *args)

        def do_GET(self):
            path = urllib.parse.urlsplit(self.path).path
            if path == '/':
                self.send_response(302)
                self.send_header('Location', '/index.html?live=1')
                self.end_headers()
            elif path == '/latest.json':
                self.serve_latest()
            elif path == '/events':
                self.serve_events()
            elif path == '/metrics':
                self.serve_metrics()
            else:
                super().do_GET()

        def serve_latest(self):
            _, latest, _ = stream.wait(stream.seq, 0)
            if latest is None:
                self.send_error(503, 'no document received from transport_viz yet')
                return
            body = json.dumps(latest).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(body)

        def serve_metrics(self):
            # 200 before the first document too: the state series say why nothing else is
            # there, and a scrape that fails would read as the server being down
            seq, latest, ended = stream.wait(stream.seq, 0)
            body = metrics_text(latest, seq, ended).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'text/plain; version=0.0.4; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(body)

        def sse(self, event, data, event_id=None):
            head = f'id: {event_id}\n' if event_id is not None else ''
            self.wfile.write(f'{head}event: {event}\ndata: {json.dumps(data)}\n\n'.encode())
            self.wfile.flush()

        def serve_events(self):
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Connection', 'keep-alive')
            self.end_headers()
            # Reconnect a second after a lost connection instead of the browser's default
            # (3 s in Chrome): the viewer sits next to the system it watches, and the
            # banner is worth clearing quickly.
            self.wfile.write(b'retry: 1000\n\n')
            self.wfile.flush()
            seen = 0
            try:
                while True:
                    seq, latest, ended = stream.wait(seen, timeout=15)
                    if seq != seen and latest is not None:
                        seen = seq
                        # the id is the document's number in this server's stream: the
                        # viewer's history counts the documents a slow client skipped
                        # by its gaps, and a restarted server by its going down (#218)
                        self.sse('document', latest, seq)
                    elif ended is None:
                        self.wfile.write(b': keep-alive\n\n')   # comment line keeps proxies awake
                        self.wfile.flush()
                    if ended is not None:
                        self.sse('status', {'state': 'ended', 'message': ended})
                        return
            except (BrokenPipeError, ConnectionResetError):
                return   # client went away

    return Handler


def find_transport_viz(explicit):
    if explicit:
        return explicit
    here = os.path.dirname(os.path.abspath(__file__))
    sibling = os.path.join(here, 'transport_viz')      # installed: lib/<pkg>/transport_viz
    if os.access(sibling, os.X_OK):
        return sibling
    found = shutil.which('transport_viz')
    if found:
        return found
    sys.exit('transport_viz_web: transport_viz not found; source the workspace or pass --transport-viz PATH')


def find_web_dir():
    here = os.path.dirname(os.path.abspath(__file__))
    for candidate in (here,                                                    # repo: web/serve.py
                      os.path.join(here, '..', '..', 'share', 'fastdds_transport_viz', 'web')):  # installed
        if os.path.isfile(os.path.join(candidate, 'index.html')):
            return os.path.abspath(candidate)
    sys.exit('transport_viz_web: web/index.html not found next to this script')


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog='transport_viz_web',
        description='Serve the fastdds_transport_viz web viewer with a live transport_viz stream.',
        epilog='Any other option is forwarded to transport_viz (e.g. --stats --interval 1 --domain 3).')
    parser.add_argument('--bind', default='127.0.0.1', help='address to listen on (default: 127.0.0.1; use 0.0.0.0 for remote browsers)')
    parser.add_argument('--port', type=int, default=8765, help='port to listen on (default: 8765; 0 = any free port)')
    parser.add_argument('--transport-viz', metavar='PATH', help='transport_viz executable (default: next to this script, then $PATH)')
    parser.add_argument('--record', metavar='FILE', help='also write every document to FILE (JSON Lines, overwritten) for replay in the viewer')
    parser.add_argument('--verbose', action='store_true', help='log HTTP requests and received documents')
    args, forward = parser.parse_known_args(argv)

    binary = find_transport_viz(args.transport_viz)
    web_dir = find_web_dir()
    record = None
    if args.record:
        try:
            record = open(args.record, 'w', encoding='utf-8')
        except OSError as e:
            sys.exit(f'transport_viz_web: cannot write the recording: {e}')
    cmd = [binary, '--watch', '--json', *forward]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True, bufsize=1)
    stream = Stream()
    pumper = threading.Thread(target=pump, args=(proc, stream, args.verbose, record), daemon=True)
    pumper.start()

    server = http.server.ThreadingHTTPServer((args.bind, args.port), make_handler(web_dir, stream, args.verbose))
    server.daemon_threads = True
    host, port = server.server_address[:2]
    print(f'transport_viz_web: running {" ".join(cmd)}', file=sys.stderr)
    if record is not None:
        print(f'transport_viz_web: recording to {args.record}', file=sys.stderr)
    print(f'transport_viz_web: listening on http://{host}:{port}/  (serving {web_dir})', flush=True)
    print(f'transport_viz_web: metrics: http://{host}:{port}/metrics', file=sys.stderr)

    def watch_producer():
        with stream.cond:
            stream.cond.wait_for(lambda: stream.ended is not None)
        time.sleep(0.5)   # let SSE handlers deliver the status event
        server.shutdown()

    threading.Thread(target=watch_producer, daemon=True).start()

    # SIGTERM and SIGHUP take the Ctrl-C path on purpose: without this they end the
    # process outright, the `finally` below never runs, and transport_viz is left behind
    # as a live DDS participant until its next frame hits the closed pipe (#195).
    for name in ('SIGTERM', 'SIGHUP'):
        signum = getattr(signal, name, None)
        if signum is not None:
            signal.signal(signum, signal.default_int_handler)

    rc = 0
    try:
        server.serve_forever()
        if stream.ended:
            print(f'transport_viz_web: {stream.ended}', file=sys.stderr)
            rc = 1 if proc.returncode else 0
    except KeyboardInterrupt:
        pass
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
        server.server_close()
        if record is not None:
            pumper.join(timeout=3)   # the last lines transport_viz wrote before it stopped
            record.close()
    return rc


if __name__ == '__main__':
    sys.exit(main())
