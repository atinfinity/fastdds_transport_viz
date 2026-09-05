# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Shared-memory report: talker + listener in the tool's IPC namespace => /dev/shm is
visible, their segments are not stale, the JSON `shm` object is complete."""
import os
import sys

import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action  # noqa: E402


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker'),
        node_action('demo_nodes_cpp', 'listener', 'listener'),
    ]), {}


class TestShm(Base):

    def test_shm_report(self):
        doc, chatter = self.wait_for_topic('/chatter')
        shm = doc['shm']
        self.assertTrue(shm['available'], shm)
        self.assertEqual(shm['path'], '/dev/shm')
        self.assertGreater(shm['total_bytes'], 0)
        self.assertEqual(shm['used_bytes'] + shm['free_bytes'], shm['total_bytes'])
        self.assertGreaterEqual(shm['fastdds_bytes'], 1)
        # talker and listener each own one segment and hold its lock
        self.assertGreaterEqual(shm['segments'] - shm['stale_segments'], 2, shm)
        self.assertGreaterEqual(shm['ports'] - shm['stale_ports'], 1, shm)
        # the nodes hold their SHM ports in our /dev/shm
        self.assertGreater(len(shm['checked_ports']), 0, shm)
        self.assertEqual(shm['missing_ports'], [], shm)
        self.assertEqual(shm['other_host_participants'], 0, shm)
        self.assertTrue(shm['nodes_visible'])
        self.assertNotIn('shm-not-visible', shm['warnings'])
        for w in shm['warnings']:
            self.assertIn(w, doc['reason_code_descriptions'])
        # String is unbounded: no data-sharing history for the talker
        writer = chatter['writers'][0]
        self.assertIsNone(writer['datasharing_history_bytes'], writer)

    def test_table_footer(self):
        import subprocess
        out = subprocess.run(
            ['ros2', 'run', 'fastdds_transport_viz', 'transport_viz', '--timeout', '3', '--quiet', '0'],
            check=True, capture_output=True, text=True, timeout=30).stdout
        self.assertIn('shared memory: /dev/shm', out)
        self.assertIn('segment(s)', out)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
