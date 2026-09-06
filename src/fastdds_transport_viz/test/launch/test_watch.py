# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
--watch without a terminal.

Frames are printed one after another with change marks (a pair appears, then
disappears); --watch --json carries the `changes` object.
"""
import json
import os
import signal
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, udpv4_only_env  # noqa: E402

from ament_index_python.packages import get_package_prefix  # noqa: E402
import launch_testing  # noqa: E402

# the binary itself: `ros2 run` would not forward the signal that ends the watch loop
BIN = [os.path.join(get_package_prefix('fastdds_transport_viz'), 'lib', 'fastdds_transport_viz',
                    'transport_viz')]


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker'),
        node_action('demo_nodes_cpp', 'listener', 'listener'),
    ]), {}


def run_watch(extra, seconds, action_at, action):
    """Run --watch for `seconds`, calling action() after `action_at` seconds."""
    proc = subprocess.Popen(
        [*BIN, '--watch', '--interval', '1', '--timeout', '1', '--quiet', '0',
         '--topic', '^/chatter$', *extra],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    time.sleep(action_at)
    handle = action()
    time.sleep(seconds - action_at)
    proc.send_signal(signal.SIGINT)      # rclcpp's handler ends the watch loop
    try:
        out, _ = proc.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
    return out, handle


LISTENER = os.path.join(
    get_package_prefix('demo_nodes_cpp'), 'lib', 'demo_nodes_cpp', 'listener')
_udp_children = []


def start_udp_listener():
    env = dict(os.environ, **udpv4_only_env())
    proc = subprocess.Popen(
        [LISTENER, '--ros-args', '-r', '__node:=listener_udp'],
        env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    _udp_children.append(proc)
    return proc


def stop_node(proc):
    """SIGINT so the participant unregisters itself (a kill would leave it to lease expiry)."""
    proc.send_signal(signal.SIGINT)
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
    if proc in _udp_children:
        _udp_children.remove(proc)


class TestWatch(Base):

    def test_table_frames_mark_added_and_removed_pairs(self):
        self.wait_for_topic('/chatter')
        # phase 1: a UDP listener appears while watching
        out, udp = run_watch(['-v'], seconds=9, action_at=3, action=start_udp_listener)
        frames = out.split('transport_viz  domain')
        self.assertGreaterEqual(len(frames), 4, out)
        self.assertIn('changes: first frame', out)
        self.assertIn('/listener_udp', out)
        marked = [f for f in frames if '+' in f.split('\n', 3)[-1] and '/listener_udp' in f]
        self.assertTrue(marked, out)
        self.assertIn('+1 pair', out)
        self.assertNotIn('\033', out)            # no terminal: no escape sequences
        # phase 2: the UDP listener goes away while watching
        out2, _ = run_watch(['-v'], seconds=9, action_at=3, action=lambda: stop_node(udp))
        self.assertIn('-1 pair', out2)
        self.assertIn('(removed)', out2)
        self.assertIn('/listener_udp', out2)
        # phase 3: a listener that disappears and comes back while its ghost row is still
        # shown: the ghost is replaced by the live pair, marked + again
        udp2 = start_udp_listener()
        time.sleep(3)

        def bounce():
            stop_node(udp2)
            time.sleep(1.5)
            return start_udp_listener()
        try:
            out3, _ = run_watch(['-v'], seconds=10, action_at=3, action=bounce)
            self.assertIn('(removed)', out3)
            self.assertIn('+1 pair', out3)
            last = out3.split('transport_viz  domain')[-1]
            self.assertNotIn('(removed)', last, 'ghost replaced by the live pair')
            self.assertIn('/listener_udp', last)
        finally:
            for proc in list(_udp_children):
                stop_node(proc)

    def test_json_frames_carry_changes(self):
        self.wait_for_topic('/chatter')
        out, udp = run_watch(['--json'], seconds=8, action_at=3, action=start_udp_listener)
        try:
            docs = [json.loads(line) for line in out.splitlines() if line.strip()]
            self.assertGreaterEqual(len(docs), 3, out[:500])
            for d in docs:
                self.assertIn('changes', d)
                for key in ('added_pairs', 'removed_pairs', 'changed_pairs'):
                    self.assertIn(key, d['changes'])
            self.assertTrue(any(d['changes']['added_pairs'] for d in docs), out[:500])
            added = next(d for d in docs if d['changes']['added_pairs'])
            self.assertEqual(added['changes']['added_pairs'][0]['topic'], '/chatter')
        finally:
            stop_node(udp)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
