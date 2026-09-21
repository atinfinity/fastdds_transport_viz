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
    Base, description, node_action, pair_of, run_tool, skip_without_easy_mode,
    transport_viz_json)

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

    def test_the_auto_server_and_its_clients_are_reported(self):
        # #86: DiscoveryServerAuto is a participant without endpoints on this host, the
        # nodes SUPER_CLIENTs attributed to it by host
        doc = None
        for _ in range(4):
            doc = transport_viz_json(env=ENV)
            if any(p['discovery_protocol'] == 'SERVER' for p in doc['participants']):
                break
        discovery = doc['discovery']
        self.assertEqual(discovery['observer_protocol'], 'SUPER_CLIENT', discovery)
        self.assertEqual(discovery['easy_mode'], '127.0.0.1', discovery)
        self.assertEqual(discovery['discovery_servers'], [], discovery)
        servers = [p for p in doc['participants'] if p['discovery_protocol'] == 'SERVER']
        self.assertEqual(len(servers), 1, doc['participants'])
        server = servers[0]
        self.assertEqual(server['name'], 'DiscoveryServerAuto', server)
        self.assertEqual(server['host_id'], doc['local_host_id'], server)
        self.assertEqual(server['metatraffic_locators'],
                         [{'kind': 'UDPv4', 'address': '127.0.0.1',
                           'port': 7400 + 250 * int(DOMAIN) + 2}], server)
        chatter, _ = pair_of(doc, '/chatter')
        by_prefix = {p['guid_prefix']: p for p in doc['participants']}
        for e in (*chatter['writers'], *chatter['readers']):
            p = by_prefix[e['participant_guid_prefix']]
            self.assertEqual(p['discovery_protocol'], 'SUPER_CLIENT', p)
            self.assertEqual(p['discovery_server'], server['guid_prefix'], p)

    def test_server_was_spawned_for_the_domain(self):
        out = run_tool(['fastdds', 'discovery', 'list'], timeout=30).stdout
        self.assertIn(f'Domain ID: {DOMAIN}', out, out)


@launch_testing.post_shutdown_test()
@skip_without_easy_mode   # Humble's `fastdds` script cannot even be executed
class TestShutdown(Base):

    def test_stop_the_easy_mode_server(self, proc_info):
        # only this domain's server; the daemon and other servers belong to the developer
        subprocess.run(['fastdds', 'discovery', 'stop', '-d', DOMAIN], timeout=30)
