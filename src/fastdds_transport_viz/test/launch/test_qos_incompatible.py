# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""QoS request/offer: a BEST_EFFORT writer never matches a RELIABLE reader, and a VOLATILE
writer never matches a TRANSIENT_LOCAL reader; the tool shows those pairs as NONE with
qos-incompatible-* reasons, and the compatible pair stays SHM."""
import os
import sys

import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action  # noqa: E402

PKG = 'fastdds_transport_viz'


def generate_test_description():
    return description([
        node_action(PKG, 'unbounded_pub', 'pub'),
        node_action(PKG, 'unbounded_pub', 'pub_be', arguments=['--best-effort']),
        node_action(PKG, 'unbounded_sub', 'sub'),
        node_action(PKG, 'unbounded_sub', 'sub_tl', arguments=['--transient-local']),
    ]), {}


class TestQosIncompatible(Base):

    def pairs(self):
        chatter = None
        for _ in range(5):
            _, chatter = self.wait_for_topic('/unbounded')
            if len(chatter['pairs']) == 4:
                return {(p['writer_node'], p['reader_node']): p for p in chatter['pairs']}
        raise AssertionError(chatter)

    def test_verdicts(self):
        pairs = self.pairs()
        ok = pairs[('/pub', '/sub')]
        self.assertEqual(ok['transport'], 'SHM', ok)
        self.assertNotIn('qos-incompatible', ok['warnings'])

        rel = pairs[('/pub_be', '/sub')]
        self.assertEqual(rel['transport'], 'NONE', rel)
        self.assertEqual(rel['confidence'], 'certain')
        self.assertEqual(rel['reasons'], ['qos-incompatible-reliability'], rel)
        self.assertIn('qos-incompatible', rel['warnings'])

        dur = pairs[('/pub', '/sub_tl')]
        self.assertEqual(dur['transport'], 'NONE', dur)
        self.assertEqual(dur['reasons'], ['qos-incompatible-durability'], dur)

        both = pairs[('/pub_be', '/sub_tl')]
        self.assertEqual(both['transport'], 'NONE', both)
        self.assertEqual(both['reasons'], ['qos-incompatible-reliability', 'qos-incompatible-durability'], both)

    def test_announced_qos_in_json(self):
        _, topic = self.wait_for_topic('/unbounded')
        by_node = {ep['node']: ep for ep in topic['writers'] + topic['readers']}
        self.assertEqual(by_node['/pub_be']['qos']['reliability'], 'BEST_EFFORT')
        self.assertEqual(by_node['/sub_tl']['qos']['durability'], 'TRANSIENT_LOCAL')
        q = by_node['/pub']['qos']
        self.assertIsNone(q['deadline_s'])
        self.assertEqual(q['liveliness'], 'AUTOMATIC')
        self.assertIsNone(q['liveliness_lease_s'])
        self.assertEqual(q['ownership'], 'SHARED')
        self.assertEqual(q['partitions'], [])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
