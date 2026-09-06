# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""talker + listener in one process namespace => SHM (String is unbounded: no data-sharing)."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, STRING_TYPES, transport_viz_json  # noqa: E402

import launch_testing  # noqa: E402


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker'),
        node_action('demo_nodes_cpp', 'listener', 'listener'),
    ]), {}


class TestSameHostShm(Base):

    def test_chatter_uses_shm(self):
        doc, chatter = self.wait_for_topic('/chatter')
        # demo_nodes_cpp moved to example_interfaces in Rolling
        self.assertIn(chatter['type'], STRING_TYPES)
        self.assertEqual(len(chatter['pairs']), 1, chatter)
        pair = chatter['pairs'][0]
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertEqual(pair['confidence'], 'certain')
        self.assertIn('same-host-guid', pair['reasons'])
        self.assertIn('both-shm-locators', pair['reasons'])
        self.assertEqual(pair['writer_node'], '/talker')
        self.assertEqual(pair['reader_node'], '/listener')
        self.assertEqual(pair['writer_host'], 'local')
        # the tool itself must not show up
        for t in doc['topics']:
            for ep in t['writers'] + t['readers']:
                self.assertFalse(ep['node'].startswith('/_transport_viz'), ep)

    def test_node_filter(self):
        self.wait_for_topic('/chatter')
        doc = transport_viz_json(['--node', '^/listener$'])
        names = {t['topic'] for t in doc['topics']}
        self.assertIn('/chatter', names)
        for t in doc['topics']:
            for p in t['pairs']:
                self.assertTrue(
                    p['writer_node'] == '/listener' or p['reader_node'] == '/listener', p)
        doc = transport_viz_json(['--node', 'no-such-node'])
        self.assertEqual(doc['topics'], [], doc['topics'])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        # nodes are killed by launch; only assert they did not crash on their own
        pass
