# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Argument translation, binary lookup and `ros2 transport` end to end with a fake binary."""
import argparse
import json
import os
import re
import stat
import subprocess

import pytest

from ros2transport.api import add_diff_arguments
from ros2transport.api import add_list_arguments
from ros2transport.api import BINARY_ENV
from ros2transport.api import diff_argv
from ros2transport.api import find_binary
from ros2transport.api import list_argv
from ros2transport.api import rmw_error
from ros2transport.verb.diff import DiffVerb
from ros2transport.verb.list import ListVerb

FAKE = """#!/usr/bin/env python3
import json, sys
print(json.dumps(sys.argv[1:]))
sys.exit(3)
"""


@pytest.fixture
def fake_binary(tmp_path):
    path = tmp_path / 'transport_viz'
    path.write_text(FAKE)
    path.chmod(path.stat().st_mode | stat.S_IXUSR)
    return str(path)


def parse(argv):
    parser = argparse.ArgumentParser()
    add_list_arguments(parser)
    return parser.parse_args(argv)


def parse_diff(argv):
    parser = argparse.ArgumentParser()
    add_diff_arguments(parser)
    return parser.parse_args(argv)


def test_list_argv_empty():
    assert list_argv(parse([])) == []


def test_list_argv_all_options():
    args = parse(['--domain', '7', '--timeout', '2.5', '--quiet', '0', '--topic', '^/ch',
                  '--node', 'talker', '--all', '-v', '--explain', '--locators', '--advise',
                  '--json', '--stats', '--color', 'never', '--watch', '--interval', '1'])
    assert list_argv(args) == [
        '--domain', '7', '--timeout', '2.5', '--quiet', '0', '--topic', '^/ch',
        '--node', 'talker', '--all', '-v', '--explain', '--locators', '--advise', '--json',
        '--stats', '--color', 'never', '--watch', '--interval', '1']


def test_color_rejects_unknown():
    with pytest.raises(SystemExit):
        parse(['--color', 'sometimes'])


def test_find_binary_override(fake_binary, monkeypatch):
    monkeypatch.setenv(BINARY_ENV, fake_binary)
    assert find_binary() == fake_binary


DIFF_ONLY = {'--key', '--changes-only'}


def binary_help_options():
    binary = find_binary()
    if binary is None:
        pytest.skip('transport_viz not built')
    out = subprocess.run([binary, '--help'], capture_output=True, text=True, check=True).stdout
    return set(re.findall(r'^\s+(?:-\w, )?(--[\w-]+)', out, re.MULTILINE))


def mirrored_options(add_arguments):
    parser = argparse.ArgumentParser()
    add_arguments(parser)
    mirrored = {s for a in parser._actions for s in a.option_strings if s.startswith('--')}
    return mirrored - {'--help'}      # argparse's own


def test_options_match_binary_help():
    """Every option of `transport_viz --help` is mirrored by `list` (except diff's own)."""
    assert binary_help_options() - {'--list-codes', '--help'} - DIFF_ONLY == \
        mirrored_options(add_list_arguments)


def test_diff_options_match_binary_help():
    """`diff` mirrors its own two options plus the view/rendering options of `list`."""
    binary_opts = binary_help_options()
    assert DIFF_ONLY <= binary_opts
    expected = mirrored_options(add_list_arguments) - {
        '--domain', '--timeout', '--quiet', '--watch', '--interval'} | DIFF_ONLY
    assert mirrored_options(add_diff_arguments) == expected


def test_diff_argv_positionals_first_then_options():
    args = parse_diff(['before.json', 'after.json'])
    assert diff_argv(args) == ['diff', 'before.json', 'after.json']
    args = parse_diff(['--key', 'guid', '--changes-only', '--json', '--color', 'never',
                       '--topic', '^/ch', '-v', 'a.json', '-'])
    assert diff_argv(args) == [
        'diff', 'a.json', '-', '--topic', '^/ch', '-v', '--json', '--color', 'never',
        '--key', 'guid', '--changes-only']
    with pytest.raises(SystemExit):
        parse_diff(['--key', 'name', 'a', 'b'])
    with pytest.raises(SystemExit):
        parse_diff(['only_one.json'])


def run_ros2(args, env_extra):
    env = dict(os.environ, **env_extra)
    return subprocess.run(['ros2', 'transport', *args], capture_output=True, text=True,
                          env=env, timeout=60)


def test_ros2_transport_list_forwards_arguments(fake_binary):
    proc = run_ros2(['list', '-v', '--stats', '--timeout', '2'], {BINARY_ENV: fake_binary})
    assert proc.returncode == 3, proc.stderr           # the fake's exit code passes through
    # options are emitted in the binary's documented order, not the typed order
    assert json.loads(proc.stdout) == ['--timeout', '2', '-v', '--stats']


def test_ros2_transport_diff_forwards_arguments(fake_binary, tmp_path):
    before = tmp_path / 'before.json'
    after = tmp_path / 'after.json'
    before.write_text('{}')
    after.write_text('{}')
    proc = run_ros2(['diff', str(before), str(after), '--changes-only', '--key', 'node'],
                    {BINARY_ENV: fake_binary})
    assert proc.returncode == 3, proc.stderr           # the fake's exit code passes through
    assert json.loads(proc.stdout) == ['diff', str(before), str(after), '--key', 'node',
                                       '--changes-only']


def test_diff_reports_a_missing_file_before_the_binary(tmp_path, capsys, monkeypatch):
    monkeypatch.setenv(BINARY_ENV, '/nonexistent/transport_viz')
    present = tmp_path / 'present.json'
    present.write_text('{}')
    rc = DiffVerb().main(args=parse_diff([str(present), str(tmp_path / 'absent.json')]))
    assert rc == 2
    err = capsys.readouterr().err
    assert 'no such file' in err and 'absent.json' in err
    # '-' (stdin) is not a file to check: the binary is reached (and here cannot run)
    rc = DiffVerb().main(args=parse_diff(['-', str(present)]))
    assert rc == 1
    assert 'cannot run' in capsys.readouterr().err


def test_ros2_transport_codes(fake_binary):
    proc = run_ros2(['codes'], {BINARY_ENV: fake_binary})
    assert proc.returncode == 3, proc.stderr
    assert json.loads(proc.stdout) == ['--list-codes']


def test_ros2_transport_without_verb_prints_help():
    proc = run_ros2([], {})
    assert proc.returncode == 0, proc.stderr
    assert 'list' in proc.stdout and 'codes' in proc.stdout and 'diff' in proc.stdout


def test_rmw_error_only_for_another_middleware(monkeypatch):
    for ok in ('rmw_fastrtps_cpp', 'rmw_fastrtps_dynamic_cpp'):
        monkeypatch.setenv('RMW_IMPLEMENTATION', ok)
        assert rmw_error() is None, ok
    monkeypatch.delenv('RMW_IMPLEMENTATION', raising=False)
    assert rmw_error() is None, 'unset: the binary asks the RMW layer'
    monkeypatch.setenv('RMW_IMPLEMENTATION', 'rmw_cyclonedds_cpp')
    msg = rmw_error()
    assert msg.startswith('ros2 transport: RMW is rmw_cyclonedds_cpp;'), msg
    assert 'RMW_IMPLEMENTATION=rmw_fastrtps_cpp' in msg


def test_other_rmw_stops_list_before_the_binary(monkeypatch, capsys):
    # In-process: `ros2 transport` itself imports rclpy, whose rcl load-time check exits 1
    # for an RMW that is not installed (the CI image has only rmw_fastrtps_cpp), so the
    # verb's own check is reached only for an installed other middleware; call it directly.
    monkeypatch.setenv('RMW_IMPLEMENTATION', 'rmw_cyclonedds_cpp')
    monkeypatch.setenv(BINARY_ENV, '/nonexistent/transport_viz')
    rc = ListVerb().main(args=parse([]))
    assert rc == 1
    err = capsys.readouterr().err
    assert err.startswith('ros2 transport: RMW is rmw_cyclonedds_cpp;'), err
    assert 'not found' not in err, 'stopped before the binary was even looked up'


def test_missing_binary_is_reported():
    proc = run_ros2(['list'], {BINARY_ENV: '/nonexistent/transport_viz'})
    assert proc.returncode == 1
    assert 'cannot run' in proc.stderr
