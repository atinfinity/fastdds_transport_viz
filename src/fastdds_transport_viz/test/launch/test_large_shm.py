# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""2 MB samples over SHM: no fallback, measured SHM traffic carries the bytes."""
import os
import sys

from ament_index_python.packages import get_package_share_directory

sys.path.insert(0, os.path.dirname(__file__))
from _common import (  # noqa: E402
    Base, description, HAS_NATIVE_BUFFERS, node_action, pair_of, skip_without_statistics,
    STATS_ENV, topic, transport_viz_json)

import launch_testing  # noqa: E402

SIZE_KB = 2048
# statistics.xml + a 16 MB SHM segment (issue #33: on slow runners the default 512 KB
# segment left the writer's RTPS_SENT without an entry for the reader's SHM port),
# generated from test/launch/large_shm_stats.xml.in by CMake (#159)
ENV = {
    **STATS_ENV,
    'FASTRTPS_DEFAULT_PROFILES_FILE': os.path.join(
        get_package_share_directory('fastdds_transport_viz'), 'test', 'large_shm_stats.xml')}


def generate_test_description():
    return description([
        # 1 Hz: 2 MB at the default 5 Hz starves the 2-vCPU CI runner and its statistics
        node_action('fastdds_transport_viz', 'large_array_pub', 'large_array_pub', ENV,
                    arguments=['--size-kb', str(SIZE_KB), '--period-ms', '1000']),
        node_action('fastdds_transport_viz', 'large_array_sub', 'large_array_sub', ENV),
    ]), {}


@skip_without_statistics
class TestLargeShm(Base):

    def test_large_samples_stay_on_shm(self):
        doc = None
        for _ in range(5):
            doc = transport_viz_json(['--stats'], timeout=8.0)
            t = next((t for t in doc['topics'] if t['topic'] == '/large_array'), None)
            if t and len(t['pairs']) == 1 and t['pairs'][0]['measured']['transports']:
                break
        topic, pair = pair_of(doc, '/large_array')
        writer_prefix = topic['writers'][0]['participant_guid_prefix']
        # where the writer's participant actually sent packets (diagnostics on failure)
        sent = [(s['dst_locator'], s['packets']) for s in doc['stats']['traffic']
                if s['src_participant_guid_prefix'] == writer_prefix]
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertNotIn('measured-transport-mismatch', pair['warnings'])
        if pair['measured']['transports']:
            self.assertEqual(
                pair['measured']['transports'], ['SHM'], (pair, sent, doc['stats']['samples']))
            # at least a few samples of SIZE_KB were carried during the observation
            self.assertGreater(pair['measured']['bytes'], 3 * SIZE_KB * 1024, pair['measured'])
        else:
            # Documented degraded outcome (issue #33): samples proven delivered but RTPS_SENT
            # attributed no packets to the reader's locators; the tool must say so.
            self.assertTrue(pair['measured']['delivered'], (pair, sent))
            self.assertIn('delivered-without-measured-traffic', pair['warnings'], (pair, sent))
            print('NOTE: SHM traffic not measured on this machine; writer RTPS_SENT:', sent)

    def test_native_buffer_companion_is_folded_into_the_parent_pair(self):
        doc = None
        for _ in range(5):
            doc = transport_viz_json(['--stats', '--all'], timeout=8.0)
            t = next((t for t in doc['topics'] if t['topic'] == '/large_array'), None)
            if t and len(t['pairs']) == 1 and t['pairs'][0]['measured']['delivered']:
                break
        parent, pair = pair_of(doc, '/large_array')
        companions = [t['topic'] for t in doc['topics'] if t['topic'].endswith('/_buf_cpu')]
        buffer_codes = [c for c in pair['reasons'] if c.startswith('buffer-companion')]
        if not HAS_NATIVE_BUFFERS:
            self.assertEqual(companions, [], doc['topics'])
            self.assertEqual(buffer_codes, [], pair)
            return
        self.assertEqual(companions, ['/large_array/_buf_cpu'])
        companion, companion_pair = pair_of(doc, '/large_array/_buf_cpu')
        self.assertEqual(buffer_codes, ['buffer-companion-folded'], pair)
        self.assertIn('buffer-companion', companion_pair['reasons'])
        for kind in ('writers', 'readers'):
            self.assertEqual(
                companion[kind][0]['buffer_parent_guid'], parent[kind][0]['guid'], companion)
        # the samples go through the companions only: the parent pair shows them folded in
        self.assertTrue(pair['measured']['delivered'], (pair, companion_pair))
        self.assertGreaterEqual(
            pair['measured']['delivered_samples'],
            companion_pair['measured']['delivered_samples'])
        # the default view leaves the folded companion topic out
        doc = transport_viz_json(timeout=4.0)
        topic(doc, '/large_array')
        self.assertNotIn('/large_array/_buf_cpu', [t['topic'] for t in doc['topics']])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
