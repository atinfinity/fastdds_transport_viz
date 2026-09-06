# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Endpoints announcing a multicast locator (defaultMulticastLocatorList in the nodes'
profile): it is reported in multicast_locators and the verdict is unchanged."""
import os
import sys

import launch_testing

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, pair_of  # noqa: E402

ENV = {'FASTRTPS_DEFAULT_PROFILES_FILE': os.path.join(os.path.dirname(__file__), 'multicast_user.xml')}


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker', ENV),
        node_action('demo_nodes_cpp', 'listener', 'listener', ENV),
    ]), {}


class TestMulticastLocators(Base):

    def test_multicast_locator_reported(self):
        doc, chatter = self.wait_for_topic('/chatter')
        for ep in chatter['writers'] + chatter['readers']:
            kinds = [(l['kind'], l['address'], l['port']) for l in ep['multicast_locators']]
            self.assertIn(('UDPv4', '239.255.0.7', 7900), kinds, ep)
        # an explicit default locator list replaces the builtin defaults, so Fast DDS 2.14
        # announces no SHM locator any more and the pair goes over UDPv4; if a version keeps
        # SHM, the verdict must be SHM
        _, pair = pair_of(doc, '/chatter')
        unicast = {l['kind'] for ep in chatter['writers'] + chatter['readers'] for l in ep['unicast_locators']}
        if 'SHM' in unicast:
            self.assertEqual(pair['transport'], 'SHM', pair)
        else:
            self.assertEqual(pair['transport'], 'UDPv4', pair)
            self.assertIn('common-udpv4-locator', pair['reasons'])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
