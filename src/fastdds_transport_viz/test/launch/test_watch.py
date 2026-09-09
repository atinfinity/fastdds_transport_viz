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


FRAME_SEP = 'transport_viz  domain'
PAIR_SHM = '/talker@local -> /listener@local'
PAIR_UDP = '/talker@local -> /listener_udp@local'


def has_all(*needles):
    """Baseline predicate for the table: every needle present in the frame."""
    return lambda frame: all(n in frame for n in needles)


def one_chatter_pair(frame):
    """Baseline predicate for --json: the JSON Lines document has one /chatter pair."""
    try:
        doc = json.loads(frame)
    except ValueError:
        return False
    chatter = next((t for t in doc['topics'] if t['topic'] == '/chatter'), None)
    return chatter is not None and len(chatter['pairs']) == 1


def read_until_frame(proc, is_baseline, timeout, sep):
    """
    Read frames until a complete one satisfies `is_baseline`.

    Returns everything read, so the caller can prepend it to the rest of the output. A
    frame counts as complete when the next frame's first line arrives (`sep` starts it:
    the header line of the table, or `{` of a JSON Lines document). On timeout the
    reading stops and the caller carries on: the assertions that follow report far more
    about a watch that never reached the expected state than a bare failure here would.
    """
    deadline = time.monotonic() + timeout
    text = ''
    frame = ''
    for line in proc.stdout:             # blocks until the next line; frames come each 1 s
        text += line
        if line.startswith(sep):
            if is_baseline(frame):
                return text
            frame = line
        else:
            frame += line
        if time.monotonic() > deadline:
            break
    return text


def run_watch(extra, after, action, is_baseline, sep=FRAME_SEP, baseline_timeout=40):
    """
    Run --watch, call action() once a frame satisfies `is_baseline`, watch `after` more.

    Waiting for the frame rather than sleeping a fixed time is what makes the change
    counts deterministic. The watch process is a participant of its own and discovers the
    nodes from scratch, so a fixed head start is a race in both directions: too short and
    the pair the phase starts from lands in the same frame as the one the action adds
    (`+2 pairs`), too long on a fast machine and everything is already in the first frame
    (no marks at all).
    """
    proc = subprocess.Popen(
        [*BIN, '--watch', '--interval', '1', '--timeout', '1', '--quiet', '0',
         '--topic', '^/chatter$', *extra],
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    seen = read_until_frame(proc, is_baseline, baseline_timeout, sep)
    handle = action()
    time.sleep(after)
    proc.send_signal(signal.SIGINT)      # rclcpp's handler ends the watch loop
    try:
        out, _ = proc.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
    return seen + out, handle


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
        try:
            # phase 1: a UDP listener appears while watching
            out, udp = run_watch(
                ['-v'], after=6, action=start_udp_listener, is_baseline=has_all(PAIR_SHM))
            frames = out.split(FRAME_SEP)
            self.assertGreaterEqual(len(frames), 4, out)
            self.assertIn('changes: first frame', out)
            self.assertIn('/listener_udp', out)
            marked = [f for f in frames if '+' in f.split('\n', 3)[-1] and '/listener_udp' in f]
            self.assertTrue(marked, out)
            self.assertIn('+1 pair', out)
            self.assertNotIn('\033', out)            # no terminal: no escape sequences
            # phase 2: the UDP listener goes away while watching
            out2, _ = run_watch(
                ['-v'], after=6, action=lambda: stop_node(udp),
                is_baseline=has_all(PAIR_SHM, PAIR_UDP))
            self.assertIn('-1 pair', out2)
            self.assertIn('(removed)', out2)
            self.assertIn('/listener_udp', out2)
            # phase 3: a listener that disappears and comes back while its ghost row is
            # still shown: the ghost is replaced by the live pair, marked + again
            udp2 = start_udp_listener()

            def bounce():
                stop_node(udp2)
                time.sleep(1.5)
                return start_udp_listener()
            out3, _ = run_watch(
                ['-v'], after=7, action=bounce, is_baseline=has_all(PAIR_SHM, PAIR_UDP))
            self.assertIn('(removed)', out3)
            self.assertIn('+1 pair', out3)
            last = out3.split(FRAME_SEP)[-1]
            self.assertNotIn('(removed)', last, 'ghost replaced by the live pair')
            self.assertIn('/listener_udp', last)
        finally:
            # every phase can raise, and these are plain Popen children that nothing else
            # reaps: one left behind would keep publishing on the shared domain for the
            # rest of the suite (see #66).
            for proc in list(_udp_children):
                stop_node(proc)

    def test_json_frames_carry_changes(self):
        self.wait_for_topic('/chatter')
        try:
            out, _ = run_watch(
                ['--json'], after=5, action=start_udp_listener,
                is_baseline=one_chatter_pair, sep='{')
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
            for proc in list(_udp_children):
                stop_node(proc)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
