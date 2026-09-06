# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Shared helpers for the launch_testing integration tests."""
import json
import os
import subprocess
import time
import unittest

from ament_index_python.packages import get_package_share_directory
import launch
import launch_ros.actions
import launch_testing.actions


def transport_viz_json(extra_args=(), timeout=6.0, env=None):
    """Run transport_viz --json and return the parsed document."""
    cmd = ['ros2', 'run', 'fastdds_transport_viz', 'transport_viz',
           '--json', '--timeout', str(timeout), '--quiet', '0', *extra_args]
    full_env = dict(os.environ)
    if env:
        full_env.update(env)
    out = subprocess.run(cmd, check=True, capture_output=True, text=True, env=full_env,
                         timeout=timeout + 30).stdout
    return json.loads(out)


def topic(doc, name):
    for t in doc['topics']:
        if t['topic'] == name:
            return t
    raise AssertionError(f'topic {name} not found in {[t["topic"] for t in doc["topics"]]}')


def node_action(package, executable, name, env_overrides=None, arguments=None):
    env = dict(os.environ)
    if env_overrides:
        env.update(env_overrides)
    return launch_ros.actions.Node(
        package=package, executable=executable, name=name, output='screen',
        env=env, arguments=list(arguments or []))


# demo_nodes_cpp publishes std_msgs/String up to Kilted and example_interfaces/String in Rolling
STRING_TYPES = ('std_msgs/msg/String', 'example_interfaces/msg/String')

# ROS 2 Humble ships Fast DDS 2.6: no statistics module in the binary, no
# FASTDDS_BUILTIN_TRANSPORTS, no ROS_AUTOMATIC_DISCOVERY_RANGE / ROS_STATIC_PEERS (Iron+),
# and only the SHM locator of a same-host peer is visible (see how-it-works.md).
FASTDDS_26 = os.environ.get('ROS_DISTRO') == 'humble'
HAS_STATISTICS = not FASTDDS_26
HAS_BUILTIN_TRANSPORTS_ENV = not FASTDDS_26
HAS_DISCOVERY_RANGE = not FASTDDS_26
skip_without_statistics = unittest.skipUnless(
    HAS_STATISTICS, 'Fast DDS built without the statistics module')
skip_without_builtin_transports = unittest.skipUnless(
    HAS_BUILTIN_TRANSPORTS_ENV, 'FASTDDS_BUILTIN_TRANSPORTS needs Fast DDS >= 2.12')
skip_without_discovery_range = unittest.skipUnless(
    HAS_DISCOVERY_RANGE, 'ROS_AUTOMATIC_DISCOVERY_RANGE needs ROS 2 Iron or later')


def udpv4_only_env():
    """Environment that makes a node's participant UDPv4-only (no SHM)."""
    if HAS_BUILTIN_TRANSPORTS_ENV:
        return {'FASTDDS_BUILTIN_TRANSPORTS': 'UDPv4'}
    return {
        'FASTRTPS_DEFAULT_PROFILES_FILE': os.path.join(
            os.path.dirname(__file__), 'udpv4_only.xml')}


STATS_ENV = {
    'FASTDDS_STATISTICS': (
        'RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;'
        'DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC;RESENT_DATAS_TOPIC;'
        'HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC'),
    # lift the statistics writers' 10-instance resource limit (see docs/statistics.md)
    'FASTRTPS_DEFAULT_PROFILES_FILE': os.path.join(
        get_package_share_directory('fastdds_transport_viz'), 'config', 'statistics.xml'),
}


def pair_of(doc, name, reader_node=None):
    """Return the single pair of topic `name` (or the one whose reader is `reader_node`)."""
    t = topic(doc, name)
    pairs = [p for p in t['pairs'] if reader_node is None or p['reader_node'] == reader_node]
    assert len(pairs) == 1, t
    return t, pairs[0]


def description(nodes):
    return launch.LaunchDescription([
        *nodes,
        # give discovery a head start before the test runs
        launch.actions.TimerAction(period=2.0, actions=[launch_testing.actions.ReadyToTest()]),
    ])


class Base(unittest.TestCase):

    def wait_for_topic(self, name, attempts=5):
        last = None
        for _ in range(attempts):
            doc = transport_viz_json()
            try:
                return doc, topic(doc, name)
            except AssertionError as e:  # noqa: PERF203
                last = e
                time.sleep(1.0)
        raise last
