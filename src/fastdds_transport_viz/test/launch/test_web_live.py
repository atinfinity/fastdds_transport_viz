# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""transport_viz_web with the real transport_viz: /latest.json and the first SSE document."""
import json
import os
import re
import subprocess
import sys
import time
import urllib.request

import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, topic  # noqa: E402


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker'),
        node_action('demo_nodes_cpp', 'listener', 'listener'),
    ]), {}


class TestWebLive(Base):

    def test_live_stream(self):
        proc = subprocess.Popen(
            ['ros2', 'run', 'fastdds_transport_viz', 'transport_viz_web', '--port', '0', '--interval', '1', '--timeout', '3'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            line = proc.stdout.readline()
            m = re.search(r'listening on (http://[^/]+)/', line)
            self.assertTrue(m, line)
            base = m.group(1)
            # The demo nodes may still be starting when the first frame is observed
            # (3 s), so read frames until the pair shows up.
            deadline = time.monotonic() + 40
            doc = None
            with urllib.request.urlopen(f'{base}/events', timeout=30) as resp:
                for raw in resp:
                    line = raw.decode().rstrip('\n')
                    if not line.startswith('data: '):
                        continue
                    doc = json.loads(line[6:])
                    self.assertEqual(doc['schema_version'], 1)
                    chatter = next((t for t in doc['topics'] if t['topic'] == '/chatter'), None)
                    if chatter and len(chatter['pairs']) == 1 or time.monotonic() > deadline:
                        break
            self.assertIsNotNone(doc)
            chatter = topic(doc, '/chatter')
            self.assertEqual(len(chatter['pairs']), 1, chatter)
            with urllib.request.urlopen(f'{base}/latest.json', timeout=10) as resp:
                self.assertEqual(json.load(resp)['schema_version'], 1)
        finally:
            proc.terminate()
            proc.wait(timeout=10)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
