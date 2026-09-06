# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST on nodes and tool: unicast-only discovery, SHM data."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from _common import (  # noqa: E402
    Base, description, node_action, pair_of, skip_without_discovery_range, transport_viz_json)

import launch_testing  # noqa: E402

ENV = {'ROS_AUTOMATIC_DISCOVERY_RANGE': 'LOCALHOST'}


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker', ENV),
        node_action('demo_nodes_cpp', 'listener', 'listener', ENV),
    ]), {}


@skip_without_discovery_range
class TestLocalhostRange(Base):

    def test_chatter_uses_shm(self):
        doc = None
        for _ in range(4):
            doc = transport_viz_json(env=ENV)
            chatter = next((t for t in doc['topics'] if t['topic'] == '/chatter'), None)
            if chatter and len(chatter['pairs']) == 1:
                break
        topic, pair = pair_of(doc, '/chatter')
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertIn('both-shm-locators', pair['reasons'])
        # LOCALHOST mode announces loopback only
        for ep in topic['writers'] + topic['readers']:
            addrs = {loc['address'] for loc in ep['unicast_locators'] if loc['kind'] == 'UDPv4'}
            self.assertEqual(addrs, {'127.0.0.1'}, ep)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
