# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""2 MB samples over SHM: no fallback, measured SHM traffic carries the bytes."""
import os
import sys

import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, pair_of, STATS_ENV, transport_viz_json  # noqa: E402

SIZE_KB = 2048


def generate_test_description():
    return description([
        node_action('fastdds_transport_viz', 'large_array_pub', 'large_array_pub', STATS_ENV,
                    arguments=['--size-kb', str(SIZE_KB)]),
        node_action('fastdds_transport_viz', 'large_array_sub', 'large_array_sub', STATS_ENV),
    ]), {}


class TestLargeShm(Base):

    def test_large_samples_stay_on_shm(self):
        doc = None
        for _ in range(4):
            doc = transport_viz_json(['--stats'], timeout=6.0)
            t = next((t for t in doc['topics'] if t['topic'] == '/large_array'), None)
            if t and len(t['pairs']) == 1 and t['pairs'][0]['measured']['transports']:
                break
        _, pair = pair_of(doc, '/large_array')
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertEqual(pair['measured']['transports'], ['SHM'], pair)
        self.assertNotIn('measured-transport-mismatch', pair['warnings'])
        # at least a few samples of SIZE_KB were carried during the observation
        self.assertGreater(pair['measured']['bytes'], 3 * SIZE_KB * 1024, pair['measured'])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
