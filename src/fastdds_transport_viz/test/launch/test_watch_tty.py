# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
--watch on a pseudo terminal.

Alternate screen, in-place painting, the key bindings, width truncation and
Ctrl-C.
"""
import fcntl
import os
import pty
import re
import select
import signal
import struct
import sys
import termios
import time

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action  # noqa: E402

from ament_index_python.packages import get_package_prefix  # noqa: E402
import launch_testing  # noqa: E402

BINARY = os.path.join(
    get_package_prefix('fastdds_transport_viz'), 'lib', 'fastdds_transport_viz', 'transport_viz')


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker'),
        node_action('demo_nodes_cpp', 'listener', 'listener'),
    ]), {}


class Tty:
    """transport_viz --watch with stdin/stdout on a pty of the given size."""

    def __init__(self, rows, cols, extra=()):
        self.master, slave = pty.openpty()
        fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        self.pid = os.fork()
        if self.pid == 0:   # child
            os.setsid()
            fcntl.ioctl(slave, termios.TIOCSCTTY, 0)
            os.dup2(slave, 0)
            os.dup2(slave, 1)
            os.dup2(slave, 2)
            try:
                os.execv(BINARY, [
                    BINARY, '--watch', '--interval', '1', '--timeout', '1', '--quiet', '0',
                    '--topic', '^/chatter$', *extra])
            finally:
                os._exit(127)
        os.close(slave)
        self.out = b''

    def read(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            r, _, _ = select.select([self.master], [], [], 0.1)
            if r:
                try:
                    self.out += os.read(self.master, 65536)
                except OSError:
                    break
        return self.out.decode('utf-8', 'replace')

    def key(self, ch):
        os.write(self.master, ch.encode())

    def wait(self, timeout=15):
        end = time.time() + timeout
        while time.time() < end:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                return os.waitstatus_to_exitcode(status)
            self.read(0.2)
        os.kill(self.pid, signal.SIGKILL)
        os.waitpid(self.pid, 0)
        raise AssertionError('transport_viz did not exit')


class TestWatchTty(Base):

    def test_keys_and_alternate_screen(self):
        self.wait_for_topic('/chatter')
        t = Tty(50, 200)
        out = t.read(4)
        self.assertIn('\033[?1049h', out)          # alternate screen
        self.assertIn('\033[?25l', out)            # cursor hidden
        self.assertIn('\033[H', out)               # frames painted in place
        self.assertIn('\033[K', out)
        self.assertIn('q quit   p pause   v pairs   e legend   a all', out)
        self.assertNotIn('->', out)                # no pair rows yet
        t.key('v')
        out = t.read(2.5)
        self.assertIn('/talker@local -> /listener@local', out)
        t.key('e')
        out = t.read(2.5)
        self.assertIn('Reason codes:', out)
        t.key('a')
        out = t.read(2.5)
        self.assertIn('[all]', out)
        t.key('p')
        out = t.read(2.5)
        self.assertIn('[PAUSED]', out)
        self.assertIn('p resume', out)
        t.key('P')
        out = t.read(2.5)
        self.assertIn('p pause', out.split('[PAUSED]')[-1])
        t.key('q')
        code = t.wait()
        self.assertEqual(code, 0, t.out.decode('utf-8', 'replace')[-1500:])
        out = t.read(0.5)
        self.assertIn('\033[?1049l', out)          # back to the main screen
        self.assertIn('\033[?25h', out)

    def test_narrow_terminal_truncates_and_ctrl_c_quits(self):
        self.wait_for_topic('/chatter')
        t = Tty(20, 60, ['-v'])
        out = t.read(4)
        self.assertIn('…', out)               # lines cut to 60 columns
        for line in out.replace('\r', '').split('\n'):
            visible = re.sub(r'\033\[[0-9;?]*[A-Za-z]', '', line)
            self.assertLessEqual(len(visible), 60, line)
        t.key('\x03')                              # Ctrl-C in raw mode: key 3, like 'q'
        code = t.wait()
        self.assertEqual(code, 0, t.out.decode('utf-8', 'replace')[-1500:])


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
