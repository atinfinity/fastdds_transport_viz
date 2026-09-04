# Copyright 2026 dandelion
# SPDX-License-Identifier: Apache-2.0
"""listener started with FASTDDS_BUILTIN_TRANSPORTS=UDPv4 => no SHM locator => UDPv4."""
import os
import sys

import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action  # noqa: E402


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker'),
        node_action('demo_nodes_cpp', 'listener', 'listener',
                    {'FASTDDS_BUILTIN_TRANSPORTS': 'UDPv4'}),
    ]), {}


class TestSameHostUdp(Base):

    def test_chatter_falls_back_to_udpv4(self):
        _, chatter = self.wait_for_topic('/chatter')
        self.assertEqual(len(chatter['pairs']), 1, chatter)
        pair = chatter['pairs'][0]
        self.assertEqual(pair['transport'], 'UDPv4', pair)
        self.assertIn('same-host-guid', pair['reasons'])
        self.assertIn('reader-no-shm-locator', pair['reasons'])
        self.assertNotIn('writer-no-shm-locator', pair['reasons'])
        self.assertIn('common-udpv4-locator', pair['reasons'])
        reader = chatter['readers'][0]
        self.assertFalse(any(l['kind'] == 'SHM' for l in reader['unicast_locators']), reader)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
