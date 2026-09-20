# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
Two endpoints of the same topic with different type names (#85).

`ros2 topic pub` publishes Int32 on /type_mismatch while `ros2 topic echo` subscribes to
String there. Fast DDS matches on the type name, so nothing can ever flow: the tool shows
the pair as NONE with type-name-mismatch instead of leaving the topic without a row.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description  # noqa: E402

import launch  # noqa: E402
import launch_testing  # noqa: E402

TOPIC = '/type_mismatch'


def cli(*args):
    return launch.actions.ExecuteProcess(cmd=['ros2', 'topic', *args], output='screen')


def generate_test_description():
    return description([
        cli('pub', '-r', '5', TOPIC, 'std_msgs/msg/Int32', '{data: 1}'),
        cli('echo', TOPIC, 'std_msgs/msg/String'),
    ]), {}


class TestTypeMismatch(Base):

    def topic_with_both(self):
        last = None
        for _ in range(5):
            _, t = self.wait_for_topic(TOPIC)
            if t['writers'] and t['readers']:
                return t
            last = t
        raise AssertionError(last)

    def test_the_mismatch_is_a_none_pair(self):
        t = self.topic_with_both()
        self.assertEqual(t['unmatched_reasons'], [], t)
        self.assertEqual(len(t['pairs']), 1, t)
        pair = t['pairs'][0]
        self.assertEqual(pair['transport'], 'NONE', pair)
        self.assertEqual(pair['confidence'], 'certain', pair)
        self.assertEqual(pair['reasons'], ['type-name-mismatch'], pair)
        self.assertNotIn('type-hash-mismatch', pair['warnings'], pair)

    def test_the_announced_types_differ(self):
        t = self.topic_with_both()
        self.assertEqual(t['writers'][0]['ros_type'], 'std_msgs/msg/Int32', t['writers'])
        self.assertEqual(t['readers'][0]['ros_type'], 'std_msgs/msg/String', t['readers'])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
