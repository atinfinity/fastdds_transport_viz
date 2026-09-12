# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""transport_viz argument handling: exits before any DDS participant is created."""
import os
import subprocess
import time

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
                    '--locators', '--advise', '--stats', '--json', '--color', '--watch',
                    '--interval', '--list-codes'):
            assert opt in r.stdout, opt


def test_list_codes_prints_every_code_with_a_description():
    r = run('--list-codes')
    assert r.returncode == 0, r
    lines = r.stdout.splitlines()
    codes = [ln for ln in lines if ln and not ln.startswith(' ')]
    fixes = [ln for ln in lines if ln.startswith('    fix: ')]
    descs = [ln for ln in lines if ln.startswith('    ') and not ln.startswith('    fix: ')]
    assert len(codes) == len(descs) >= 40, (len(codes), len(descs))
    # a remedy line follows the description of the codes that have one, not of every code
    assert 25 <= len(fixes) < len(codes), (len(fixes), len(codes))
    assert 'same-host-guid' in codes and 'shm-not-visible' in codes
    assert codes == sorted(codes)
    i = lines.index('shm-stale-files')
    assert lines[i + 2].startswith("    fix: Run 'fastdds shm clean'"), lines[i:i + 3]
    i = lines.index('same-host-guid')
    assert not lines[i + 2].startswith('    fix:'), lines[i:i + 3]


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


def test_color_modes_are_accepted_and_always_paints_without_a_terminal():
    for mode in ('auto', 'never'):
        r = run('--color', mode, '--list-codes')
        assert r.returncode == 0, (mode, r)
        assert '\033[' not in r.stdout
    # a table with no endpoints still carries the bold "shared memory:" label
    r = run('--color', 'always', '--timeout', '0.5', '--quiet', '0')
    assert r.returncode == 0, r
    assert (
        '\033[1mshared memory: \033[0m' in r.stdout or '(no endpoints discovered' in r.stdout
    ), r.stdout
    assert '\033[' in r.stdout


def test_explain_ros_args_and_default_timeout():
    r = run('--explain', '--timeout', '0.5', '--quiet', '0')
    assert r.returncode == 0, r
    assert "Legend: '?' after a transport" in r.stdout
    # --advise implies --explain (the legend is where the remedies of the codes in use go)
    r = run('--advise', '--timeout', '0.5', '--quiet', '0')
    assert r.returncode == 0, r
    assert "Legend: '?' after a transport" in r.stdout
    # everything after --ros-args is left to rclcpp
    r = run('--timeout', '0.5', '--quiet', '0', '--ros-args', '--log-level', 'warn')
    assert r.returncode == 0, r
    # no --timeout: the default of 3 s applies
    t0 = time.monotonic()
    r = run('--quiet', '0')
    assert r.returncode == 0, r
    assert 2.5 <= time.monotonic() - t0 <= 15


def test_unloadable_rmw_exits_1_before_discovery():
    # The CI image ships only rmw_fastrtps_cpp, so "another middleware" cannot be exercised
    # end to end here (test_rmw_check.cpp pins that verdict). An RMW that cannot be loaded
    # is stopped even earlier: rcl's own load-time check exits 1 naming it before main()
    # runs, so the binary never reaches discovery. Either way: exit 1, the name in stderr.
    env = dict(os.environ, RMW_IMPLEMENTATION='rmw_bogus_cpp')
    t0 = time.monotonic()
    r = subprocess.run([BINARY, '--timeout', '5'], capture_output=True, text=True, timeout=30,
                       env=env)
    assert r.returncode == 1, r
    assert 'rmw_bogus_cpp' in r.stderr, r.stderr
    assert time.monotonic() - t0 < 4, 'exited before the discovery timeout'
    assert r.stdout == ''


def test_discovery_range_off_prints_a_warning():
    env = dict(os.environ, ROS_AUTOMATIC_DISCOVERY_RANGE='OFF')
    r = subprocess.run(
        [BINARY, '--timeout', '0.5', '--quiet', '0'], capture_output=True, text=True,
        timeout=30, env=env)
    assert r.returncode == 0, r
    assert 'ROS_AUTOMATIC_DISCOVERY_RANGE=OFF disables discovery' in r.stderr


def test_invalid_color_mode_exits_2():
    r = run('--color', 'pink')
    assert r.returncode == 2, r
    assert '--color expects auto, always or never' in r.stderr
