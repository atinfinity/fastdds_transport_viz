# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""FASTDDS_BUILTIN_TRANSPORTS=LARGE_DATAv6 on one host: TCPv6 (and UDPv6) locators are
announced, SHM is still chosen. Skipped without an IPv6 interface."""
import os
import sys
import unittest

import launch
import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, pair_of, transport_viz_json  # noqa: E402
from test_udpv6 import has_ipv6_interface  # noqa: E402

ENV = {'FASTDDS_BUILTIN_TRANSPORTS': 'LARGE_DATAv6'}


def generate_test_description():
    if has_ipv6_interface():
        nodes = [node_action('demo_nodes_cpp', 'talker', 'talker', ENV),
                 node_action('demo_nodes_cpp', 'listener', 'listener', ENV)]
    else:
        nodes = [launch.actions.ExecuteProcess(cmd=['sleep', '30'], output='screen')]
    return description(nodes), {}


class TestLargeDataV6(Base):

    @unittest.skipUnless(has_ipv6_interface(), 'no IPv6 interface')
    def test_tcpv6_announced_shm_chosen(self):
        doc = None
        for _ in range(5):
            doc = transport_viz_json(env=ENV)
            if any(t['topic'] == '/chatter' and t['pairs'] for t in doc['topics']):
                break
        topic, pair = pair_of(doc, '/chatter')
        kinds = {l['kind'] for ep in topic['writers'] + topic['readers'] for l in ep['unicast_locators']}
        self.assertIn('TCPv6', kinds, kinds)
        self.assertIn('SHM', kinds, kinds)
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertIn('both-shm-locators', pair['reasons'])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
