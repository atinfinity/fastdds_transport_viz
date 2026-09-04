# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""`ros2 transport list --json` against real demo nodes (talker + listener => SHM)."""
import json
import os
import subprocess
import unittest

import launch
import launch_ros.actions
import launch_testing
import pytest


@pytest.mark.launch_test
def generate_test_description():
    env = dict(os.environ, RMW_IMPLEMENTATION='rmw_fastrtps_cpp')
    return launch.LaunchDescription([
        launch_ros.actions.Node(package='demo_nodes_cpp', executable='talker', name='talker',
                                output='screen', env=env),
        launch_ros.actions.Node(package='demo_nodes_cpp', executable='listener',
                                name='listener', output='screen', env=env),
        launch_testing.actions.ReadyToTest(),
    ]), {}


class TestListLive(unittest.TestCase):

    def test_chatter_pair(self):
        env = dict(os.environ, RMW_IMPLEMENTATION='rmw_fastrtps_cpp')
        doc = None
        for _ in range(4):        # nodes may still be starting
            out = subprocess.run(
                ['ros2', 'transport', 'list', '--json', '--timeout', '6', '--quiet', '0'],
                capture_output=True, text=True, env=env, timeout=60)
            self.assertEqual(out.returncode, 0, out.stderr)
            doc = json.loads(out.stdout)
            chatter = next((t for t in doc['topics'] if t['topic'] == '/chatter'), None)
            if chatter and len(chatter['pairs']) == 1:
                break
        self.assertEqual(doc['schema_version'], 1)
        self.assertIsNotNone(chatter, doc['topics'])
        self.assertEqual(len(chatter['pairs']), 1, chatter)
        pair = chatter['pairs'][0]
        self.assertEqual(pair['transport'], 'SHM', pair)
        self.assertIn('same-host-guid', pair['reasons'])


@launch_testing.post_shutdown_test()
class TestShutdown(unittest.TestCase):

    def test_exit_codes(self, proc_info):
        pass
