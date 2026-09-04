# Copyright 2026 dandelion
# SPDX-License-Identifier: Apache-2.0
"""--stats: nodes started with FASTDDS_STATISTICS => measured SHM traffic and host names."""
import os
import sys

import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, topic, transport_viz_json  # noqa: E402

from ament_index_python.packages import get_package_share_directory  # noqa: E402

STATS_ENV = {
    'FASTDDS_STATISTICS': 'RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC',
    # lift the statistics writers' 10-instance resource limit (see README)
    'FASTRTPS_DEFAULT_PROFILES_FILE': os.path.join(
        get_package_share_directory('fastdds_transport_viz'), 'config', 'statistics.xml'),
}


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker', STATS_ENV),
        node_action('demo_nodes_cpp', 'listener', 'listener', STATS_ENV),
        node_action('demo_nodes_cpp', 'listener', 'listener_udp',
                    {**STATS_ENV, 'FASTDDS_BUILTIN_TRANSPORTS': 'UDPv4'}),
    ]), {}


class TestStats(Base):

    def test_measured_transports(self):
        doc = None
        for _ in range(4):
            doc = transport_viz_json(['--stats'], timeout=6.0)
            chatter = topic(doc, '/chatter')
            if len(chatter['pairs']) == 2 and all(p['measured']['transports'] for p in chatter['pairs']):
                break
        self.assertTrue(doc['stats']['enabled'])
        self.assertGreater(doc['stats']['samples'], 0, doc['stats'])
        pairs = {p['reader_node']: p for p in chatter['pairs']}
        self.assertEqual(set(pairs), {'/listener', '/listener_udp'}, chatter)

        shm = pairs['/listener']
        self.assertEqual(shm['transport'], 'SHM')
        self.assertTrue(shm['measured']['available'], shm)
        self.assertEqual(shm['measured']['transports'], ['SHM'], shm)
        self.assertGreater(shm['measured']['packets'], 0)
        self.assertIn('measured-shm-traffic', shm['reasons'])
        self.assertNotIn('measured-transport-mismatch', shm['warnings'])

        udp = pairs['/listener_udp']
        self.assertEqual(udp['transport'], 'UDPv4')
        self.assertEqual(udp['measured']['transports'], ['UDPv4'], udp)
        self.assertIn('measured-udpv4-traffic', udp['reasons'])
        self.assertNotIn('measured-transport-mismatch', udp['warnings'])

        # PHYSICAL_DATA gives host names and processes
        writer = chatter['writers'][0]
        self.assertTrue(writer['host_name'], writer)
        self.assertTrue(writer['process'], writer)
        # table/JSON host label is the hostname part of "<hostname>:<numeric id>"
        self.assertEqual(shm['writer_host'], writer['host_name'].split(':')[0])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
