# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Data-sharing (bounded_pub/bounded_sub with datasharing_auto_stats.xml) confirmed as
certain by --stats through HISTORY_LATENCY + DATA_COUNT."""
import os
import sys

import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, pair_of, STATS_ENV, transport_viz_json  # noqa: E402

from ament_index_python.packages import get_package_share_directory  # noqa: E402

ENV = {
    **STATS_ENV,
    'FASTRTPS_DEFAULT_PROFILES_FILE': os.path.join(
        get_package_share_directory('fastdds_transport_viz'), 'config', 'datasharing_auto_stats.xml'),
    'RMW_FASTRTPS_USE_QOS_FROM_XML': '1',
}


def generate_test_description():
    return description([
        node_action('fastdds_transport_viz', 'bounded_pub', 'bounded_pub', ENV),
        node_action('fastdds_transport_viz', 'bounded_sub', 'bounded_sub', ENV),
    ]), {}


class TestDataSharingStats(Base):

    def test_datasharing_is_certain(self):
        doc = None
        for _ in range(5):
            doc = transport_viz_json(['--stats'], timeout=6.0)
            t = next((t for t in doc['topics'] if t['topic'] == '/bounded'), None)
            if t and len(t['pairs']) == 1 and t['pairs'][0]['confidence'] == 'certain':
                break
        _, pair = pair_of(doc, '/bounded')
        self.assertEqual(pair['transport'], 'DATA_SHARING', pair)
        self.assertEqual(pair['confidence'], 'certain', pair)
        self.assertTrue(pair['measured']['delivered'], pair)
        self.assertGreater(pair['measured']['delivered_samples'], 0, pair)
        self.assertEqual(pair['measured']['data_submessages'], 0, pair)
        codes = set(pair['reasons'])
        self.assertTrue(
            codes & {'datasharing-confirmed-no-data-submessages', 'datasharing-confirmed-no-traffic'}, pair)
        self.assertNotIn('datasharing-not-used', pair['warnings'])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
