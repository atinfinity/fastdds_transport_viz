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
  /events        -> Server-Sent Events: the latest document on connect, then
                    every new document ("document" events); a "status" event
                    when the stream ends

Standard library only. Unknown command-line options are forwarded verbatim to
transport_viz (e.g. --stats, --interval 1, --domain 3, --all, --topic REGEX).
"""
import argparse
import http.server
import json
import os
import shutil
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


def pump(proc, stream, verbose):
    """Read JSON Lines from the subprocess into the stream."""
    for raw in proc.stdout:
        line = raw.strip()
        if not line:
            continue
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

        def sse(self, event, data):
            self.wfile.write(f'event: {event}\ndata: {json.dumps(data)}\n\n'.encode())
            self.wfile.flush()

        def serve_events(self):
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Connection', 'keep-alive')
            self.end_headers()
            seen = 0
            try:
                while True:
                    seq, latest, ended = stream.wait(seen, timeout=15)
                    if seq != seen and latest is not None:
                        seen = seq
                        self.sse('document', latest)
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
    parser.add_argument('--verbose', action='store_true', help='log HTTP requests and received documents')
    args, forward = parser.parse_known_args(argv)

    binary = find_transport_viz(args.transport_viz)
    web_dir = find_web_dir()
    cmd = [binary, '--watch', '--json', *forward]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True, bufsize=1)
    stream = Stream()
    threading.Thread(target=pump, args=(proc, stream, args.verbose), daemon=True).start()

    server = http.server.ThreadingHTTPServer((args.bind, args.port), make_handler(web_dir, stream, args.verbose))
    server.daemon_threads = True
    host, port = server.server_address[:2]
    print(f'transport_viz_web: running {" ".join(cmd)}', file=sys.stderr)
    print(f'transport_viz_web: listening on http://{host}:{port}/  (serving {web_dir})', flush=True)

    def watch_producer():
        with stream.cond:
            stream.cond.wait_for(lambda: stream.ended is not None)
        time.sleep(0.5)   # let SSE handlers deliver the status event
        server.shutdown()

    threading.Thread(target=watch_producer, daemon=True).start()

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
    return rc


if __name__ == '__main__':
    sys.exit(main())
