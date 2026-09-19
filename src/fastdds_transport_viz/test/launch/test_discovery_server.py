# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
ROS_DISCOVERY_SERVER: SUPER_CLIENT vs CLIENT.

Nodes are clients of `fastdds discovery`; the tool observes as SUPER_CLIENT
automatically (a plain CLIENT does not learn about /chatter).
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, node_action, pair_of, transport_viz_json  # noqa: E402

import launch  # noqa: E402
import launch_testing  # noqa: E402

SERVER = '127.0.0.1:11811'
ENV = {'ROS_DISCOVERY_SERVER': SERVER}


def generate_test_description():
    # Fast DDS 2.x requires a server id; the 3.x tool has no -i option. Humble's `fastdds`
    # script has no shebang (not executable from here), it ships fast-discovery-server.
    distro = os.environ.get('ROS_DISTRO')
    if distro == 'humble':
        cmd = ['fast-discovery-server', '-i', '0']
    else:
        cmd = ['fastdds', 'discovery', *(['-i', '0'] if distro == 'jazzy' else [])]
    server = launch.actions.ExecuteProcess(
        cmd=[*cmd, '-l', '127.0.0.1', '-p', '11811'], output='screen')
    nodes = [node_action('demo_nodes_cpp', 'talker', 'talker', ENV),
             node_action('demo_nodes_cpp', 'listener', 'listener', ENV)]
    return launch.LaunchDescription([
        server,
        launch.actions.TimerAction(period=1.0, actions=nodes),
        launch.actions.TimerAction(period=3.0, actions=[launch_testing.actions.ReadyToTest()]),
    ]), {}


class TestDiscoveryServer(Base):

    def test_super_client_sees_the_pair(self):
        doc = None
        for _ in range(4):
            doc = transport_viz_json(env=ENV)
            chatter = next((t for t in doc['topics'] if t['topic'] == '/chatter'), None)
            if chatter and len(chatter['pairs']) == 1:
                break
        chatter, pair = pair_of(doc, '/chatter')
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertEqual({pair['writer_node'], pair['reader_node']}, {'/talker', '/listener'})
        # #86: the server is a participant without endpoints, the nodes its clients
        discovery = doc['discovery']
        self.assertEqual(discovery['observer_protocol'], 'SUPER_CLIENT', discovery)
        self.assertEqual(discovery['discovery_server_env'], SERVER, discovery)
        self.assertEqual(discovery['discovery_servers'],
                         [{'kind': 'UDPv4', 'address': '127.0.0.1', 'port': 11811}], discovery)
        self.assertEqual(discovery['easy_mode'], '', discovery)
        servers = [p for p in doc['participants'] if p['discovery_protocol'] == 'SERVER']
        self.assertEqual(len(servers), 1, doc['participants'])
        server = servers[0]
        if os.environ.get('ROS_DISTRO') in ('humble', 'jazzy'):
            # `fastdds discovery -i 0` (fast-discovery-server on Humble): the fixed server
            # prefix of server id 0; the 3.x CLI without -i starts a "Guidless Server"
            self.assertEqual(server['guid_prefix'], '44.53.00.5f.45.50.52.4f.53.49.4d.41', server)
        else:
            self.assertEqual(server['name'], 'eProsima Guidless Server', server)
        self.assertEqual(server['vendor'], 'eProsima', server)
        self.assertEqual(server['metatraffic_locators'],
                         [{'kind': 'UDPv4', 'address': '127.0.0.1', 'port': 11811}], server)
        self.assertIsNone(server['discovery_server'], server)
        self.assertFalse(server['own'], server)
        by_prefix = {p['guid_prefix']: p for p in doc['participants']}
        for e in (*chatter['writers'], *chatter['readers']):
            p = by_prefix[e['participant_guid_prefix']]
            # Fast DDS 3.6 (Lyrical, Rolling) announces a CLIENT as SUPER_CLIENT
            self.assertIn(p['discovery_protocol'], ('CLIENT', 'SUPER_CLIENT'), p)
            self.assertEqual(p['discovery_server'], server['guid_prefix'], p)
            self.assertEqual(p['vendor'], 'eProsima', p)
            self.assertTrue(p['metatraffic_locators'], p)
        # the tool's own participants are never clients of anything in the report
        for p in doc['participants']:
            if p['own']:
                self.assertIsNone(p['discovery_server'], p)

    @unittest.skipUnless(
        os.environ.get('ROS_DISTRO') in ('jazzy',),
        'Fast DDS 3.6+ (Lyrical, Rolling) relays every endpoint to plain clients too')
    def test_plain_client_is_blind(self):
        doc = transport_viz_json(env={**ENV, 'ROS_SUPER_CLIENT': 'FALSE'})
        names = [t['topic'] for t in doc['topics']]
        self.assertNotIn('/chatter', names, names)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
