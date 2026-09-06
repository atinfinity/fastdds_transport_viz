# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""FASTDDS_BUILTIN_TRANSPORTS=UDPv6 on nodes and tool => UDPv6 (skipped without IPv6)."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from _common import (  # noqa: E402
    Base, description, node_action, pair_of, skip_without_builtin_transports, transport_viz_json)

import launch  # noqa: E402
import launch_testing  # noqa: E402

ENV = {'FASTDDS_BUILTIN_TRANSPORTS': 'UDPv6'}


def has_ipv6_interface():
    """Return whether a non-loopback interface has an IPv6 address (needed for UDPv6)."""
    try:
        with open('/proc/net/if_inet6') as f:
            return any(line.split()[-1] != 'lo' for line in f)
    except OSError:
        return False


def generate_test_description():
    if has_ipv6_interface():
        nodes = [node_action('demo_nodes_cpp', 'talker', 'talker', ENV),
                 node_action('demo_nodes_cpp', 'listener', 'listener', ENV)]
    else:
        # keep the launch alive while the test skips itself (launch_testing fails when
        # every process is gone before the tests ran)
        nodes = [launch.actions.ExecuteProcess(cmd=['sleep', '30'], output='screen')]
    return description(nodes), {}


@skip_without_builtin_transports
class TestUdpv6(Base):

    def test_chatter_uses_udpv6(self):
        if not has_ipv6_interface():
            self.skipTest('no IPv6 address on a non-loopback interface in this environment')
        doc = None
        for _ in range(4):
            doc = transport_viz_json(env=ENV)
            chatter = next((t for t in doc['topics'] if t['topic'] == '/chatter'), None)
            if chatter and len(chatter['pairs']) == 1:
                break
        _, pair = pair_of(doc, '/chatter')
        self.assertEqual(pair['transport'], 'UDPv6', pair)
        self.assertIn('common-udpv6-locator', pair['reasons'])
        self.assertIn('same-host-guid', pair['reasons'])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
