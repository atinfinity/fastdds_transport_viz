# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Shared helpers for the launch_testing integration tests."""
import json
import os
import subprocess
import time
import unittest

import launch
import launch_ros.actions
import launch_testing.actions

from ament_index_python.packages import get_package_share_directory


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


STATS_ENV = {
    'FASTDDS_STATISTICS': 'RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC',
    # lift the statistics writers' 10-instance resource limit (see docs/statistics.md)
    'FASTRTPS_DEFAULT_PROFILES_FILE': os.path.join(
        get_package_share_directory('fastdds_transport_viz'), 'config', 'statistics.xml'),
}


def pair_of(doc, name, reader_node=None):
    """The single pair of topic `name` (or the one whose reader is `reader_node`)."""
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
