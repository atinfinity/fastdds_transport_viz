# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""FASTDDS_BUILTIN_TRANSPORTS=LARGE_DATA on one host: TCPv4 is announced, SHM still wins."""
import os
import sys

import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, pair_of, transport_viz_json, skip_without_builtin_transports  # noqa: E402

ENV = {'FASTDDS_BUILTIN_TRANSPORTS': 'LARGE_DATA'}


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker', ENV),
        node_action('demo_nodes_cpp', 'listener', 'listener', ENV),
    ]), {}


@skip_without_builtin_transports
class TestLargeDataSameHost(Base):

    def test_shm_wins_over_announced_tcp(self):
        doc = None
        for _ in range(4):
            doc = transport_viz_json(env=ENV)
            chatter = next((t for t in doc['topics'] if t['topic'] == '/chatter'), None)
            if chatter and len(chatter['pairs']) == 1:
                break
        topic, pair = pair_of(doc, '/chatter')
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertIn('both-shm-locators', pair['reasons'])
        kinds = {l['kind'] for l in topic['writers'][0]['unicast_locators']}
        self.assertIn('TCPv4', kinds, topic['writers'][0])
        self.assertIn('SHM', kinds)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
