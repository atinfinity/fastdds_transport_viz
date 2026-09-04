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
            with urllib.request.urlopen(f'{base}/events', timeout=30) as resp:
                doc = None
                for raw in resp:
                    line = raw.decode().rstrip('\n')
                    if line.startswith('data: '):
                        doc = json.loads(line[6:])
                        break
            self.assertIsNotNone(doc)
            self.assertEqual(doc['schema_version'], 1)
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
