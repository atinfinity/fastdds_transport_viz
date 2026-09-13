# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
ROS2_EASY_MODE (Fast DDS 3.2+): Discovery Server per host, P2P builtin transport.

Talker, listener and the tool run on this host with ROS2_EASY_MODE pointing at
loopback, on a private domain so that the auto-started server (port
7400 + 250 * domain + 2) does not collide with the other tests. Expect SHM on one
host and no multicast locator at all (P2P = SHM + TCPv4 user data, UDPv4 unicast
to the local server for discovery).
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
from _common import (  # noqa: E402
    Base, description, node_action, pair_of, skip_without_easy_mode, transport_viz_json)

import launch_testing  # noqa: E402

DOMAIN = '71'
ENV = {'ROS2_EASY_MODE': '127.0.0.1', 'ROS_DOMAIN_ID': DOMAIN}


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker', ENV),
        node_action('demo_nodes_cpp', 'listener', 'listener', ENV),
    ]), {}


@skip_without_easy_mode
class TestEasyMode(Base):

    def test_same_host_pair_is_shm_without_multicast(self):
        doc = None
        for _ in range(4):
            doc = transport_viz_json(extra_args=['--locators'], env=ENV)
            chatter = next((t for t in doc['topics'] if t['topic'] == '/chatter'), None)
            if chatter and len(chatter['pairs']) == 1:
                break
        chatter, pair = pair_of(doc, '/chatter')
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertIn('same-host-guid', pair['reasons'], pair)
        self.assertIn('both-shm-locators', pair['reasons'], pair)
        self.assertEqual({pair['writer_node'], pair['reader_node']}, {'/talker', '/listener'})
        for e in (*chatter['writers'], *chatter['readers']):
            # P2P: no multicast, user data locators are SHM and TCPv4 only
            self.assertEqual(e['multicast_locators'], [], e)
            self.assertEqual({loc['kind'] for loc in e['unicast_locators']}, {'SHM', 'TCPv4'}, e)

    def test_server_was_spawned_for_the_domain(self):
        out = subprocess.run(['fastdds', 'discovery', 'list'], check=True,
                             capture_output=True, text=True, timeout=30).stdout
        self.assertIn(f'Domain ID: {DOMAIN}', out, out)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_stop_the_easy_mode_server(self, proc_info):
        # only this domain's server; the daemon and other servers belong to the developer
        subprocess.run(['fastdds', 'discovery', 'stop', '-d', DOMAIN], timeout=30)
