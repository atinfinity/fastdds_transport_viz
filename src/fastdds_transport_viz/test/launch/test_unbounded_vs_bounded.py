# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""With datasharing_auto.xml, the bounded pair is predicted DATA_SHARING (likely) while the
unbounded String writer resolves to OFF and stays on SHM."""
import os
import sys

import launch_testing
from ament_index_python.packages import get_package_share_directory

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, pair_of  # noqa: E402

ENV = {
    'FASTRTPS_DEFAULT_PROFILES_FILE': os.path.join(
        get_package_share_directory('fastdds_transport_viz'), 'config', 'datasharing_auto.xml'),
    'RMW_FASTRTPS_USE_QOS_FROM_XML': '1',
}


def generate_test_description():
    return description([
        node_action('fastdds_transport_viz', 'bounded_pub', 'bounded_pub', ENV),
        node_action('fastdds_transport_viz', 'bounded_sub', 'bounded_sub', ENV),
        node_action('fastdds_transport_viz', 'unbounded_pub', 'unbounded_pub', ENV),
        node_action('fastdds_transport_viz', 'unbounded_sub', 'unbounded_sub', ENV),
    ]), {}


class TestUnboundedVsBounded(Base):

    def test_bounded_is_datasharing_likely(self):
        doc, _ = self.wait_for_topic('/bounded')
        _, pair = pair_of(doc, '/bounded')
        self.assertEqual(pair['transport'], 'DATA_SHARING', pair)
        self.assertEqual(pair['confidence'], 'likely', pair)
        self.assertIn('datasharing-qos-enabled-both', pair['reasons'])
        self.assertIn('datasharing-unverified-by-traffic', pair['reasons'])

    def test_unbounded_stays_on_shm(self):
        doc, topic = self.wait_for_topic('/unbounded')
        self.assertEqual(topic['type'], 'std_msgs/msg/String')
        _, pair = pair_of(doc, '/unbounded')
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertIn('datasharing-disabled-writer', pair['reasons'])
        self.assertEqual(pair['writer_node'], '/unbounded_pub')
        self.assertEqual(topic['writers'][0]['qos']['data_sharing'], 'OFF', topic['writers'][0])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
