# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""transport_viz argument handling: exits before any DDS participant is created."""
import os
import subprocess

from ament_index_python.packages import get_package_prefix

BINARY = os.path.join(get_package_prefix('fastdds_transport_viz'), 'lib', 'fastdds_transport_viz',
                      'transport_viz')


def run(*args):
    return subprocess.run([BINARY, *args], capture_output=True, text=True, timeout=30)


def test_help_exits_zero_with_usage():
    for flag in ('-h', '--help'):
        r = run(flag)
        assert r.returncode == 0, r
        assert r.stdout.startswith('Usage: transport_viz [options]'), r.stdout
        for opt in ('--domain', '--timeout', '--quiet', '--topic', '--node', '--all', '--explain',
                    '--stats', '--json', '--color', '--watch', '--interval', '--list-codes'):
            assert opt in r.stdout, opt


def test_list_codes_prints_every_code_with_a_description():
    r = run('--list-codes')
    assert r.returncode == 0, r
    lines = r.stdout.splitlines()
    codes = [ln for ln in lines if ln and not ln.startswith(' ')]
    descs = [ln for ln in lines if ln.startswith('    ')]
    assert len(codes) == len(descs) >= 40, (len(codes), len(descs))
    assert 'same-host-guid' in codes and 'shm-not-visible' in codes
    assert codes == sorted(codes)


def test_unknown_option_exits_2():
    r = run('--bogus')
    assert r.returncode == 2, r
    assert 'unknown option: --bogus' in r.stderr
    assert 'Usage:' in r.stdout


def test_missing_value_exits_2():
    for flag in ('--timeout', '--topic', '--node', '--domain', '--color'):
        r = run(flag)
        assert r.returncode == 2, (flag, r)
        assert f'{flag} requires a value' in r.stderr, (flag, r.stderr)


def test_invalid_regex_exits_2():
    r = run('--topic', '(')
    assert r.returncode == 2, r
    assert "--topic: invalid regex '('" in r.stderr
    r = run('--node', '[')
    assert r.returncode == 2, r
    assert "--node: invalid regex '['" in r.stderr


def test_invalid_color_mode_exits_2():
    r = run('--color', 'pink')
    assert r.returncode == 2, r
    assert '--color expects auto, always or never' in r.stderr
