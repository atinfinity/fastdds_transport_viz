# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""A failing tool run in a launch test says how it ended and what it printed (#215)."""
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'launch'))
from _common import exit_status, run_tool  # noqa: E402, I100


def test_exit_status_names_the_signal_ros2_run_passes_on():
    # ros2 run exits with its child's return code: a child killed by SIGSEGV gives 245
    assert exit_status(245) == 'exit status 245: killed by SIGSEGV'
    assert exit_status(-11) == 'exit status -11: killed by SIGSEGV'
    assert exit_status(2) == 'exit status 2'


def test_a_crash_shows_the_signal_and_the_captured_output():
    script = ('import os, signal, sys; print("partial doc", flush=True); '
              'print("last words", file=sys.stderr, flush=True); '
              'os.kill(os.getpid(), signal.SIGSEGV)')
    with pytest.raises(AssertionError) as e:
        run_tool([sys.executable, '-c', script], timeout=30)
    msg = str(e.value)
    assert 'killed by SIGSEGV' in msg, msg
    assert 'last words' in msg, msg
    assert 'partial doc' in msg, msg


def test_ros2_run_style_exit_code_is_decoded():
    with pytest.raises(AssertionError) as e:
        run_tool([sys.executable, '-c', 'import sys; sys.exit(-11 % 256)'], timeout=30)
    assert 'exit status 245: killed by SIGSEGV' in str(e.value)


def test_a_successful_run_returns_the_process():
    p = run_tool([sys.executable, '-c', 'print("ok")'], timeout=30)
    assert p.stdout == 'ok\n'
