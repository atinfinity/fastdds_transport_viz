# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
transport_viz argument handling: exits before any DDS participant is created.

Also `transport_viz diff`, which never creates one: the two fixture documents under
web/sample/ (a profile change with every node restarted in between, see docs/how-it-works.md).
"""
import json
import os
import pathlib
import subprocess
import time

from ament_index_python.packages import get_package_prefix

BINARY = os.path.join(get_package_prefix('fastdds_transport_viz'), 'lib', 'fastdds_transport_viz',
                      'transport_viz')
SAMPLES = pathlib.Path(__file__).resolve().parents[3] / 'web' / 'sample'
BEFORE = str(SAMPLES / 'diff_before.json')
AFTER = str(SAMPLES / 'diff_after.json')


def run(*args):
    return subprocess.run([BINARY, *args], capture_output=True, text=True, timeout=30)


def test_help_exits_zero_with_usage():
    for flag in ('-h', '--help'):
        r = run(flag)
        assert r.returncode == 0, r
        assert r.stdout.startswith('Usage: transport_viz [options]'), r.stdout
        for opt in ('--domain', '--timeout', '--quiet', '--topic', '--node', '--all', '--explain',
                    '--locators', '--advise', '--stats', '--json', '--color', '--watch',
                    '--interval', '--list-codes', '--key', '--changes-only'):
            assert opt in r.stdout, opt
        assert 'transport_viz diff <before.json> <after.json>' in r.stdout


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


# ---- transport_viz diff -----------------------------------------------------------------

def pairs_of(doc, topic_name):
    return next(t for t in doc['topics'] if t['topic'] == topic_name)['pairs']


def test_diff_node_key_survives_the_restart_and_exits_1():
    r = run('diff', BEFORE, AFTER)
    assert r.returncode == 1, r
    assert 'changes: +2 pairs  -2 pairs  ~1 changed' in r.stdout, r.stdout
    assert '+  /chatter' in r.stdout and '+  /new_topic' in r.stdout, r.stdout
    assert '-  /old_topic' in r.stdout and '(removed)' in r.stdout, r.stdout
    assert '/cmd_vel' in r.stdout            # unchanged topics stay in the table ...
    assert 'raw_topic' not in r.stdout       # ... but a non-ROS topic needs --all
    assert '\033[' not in r.stdout
    # the pair rows: the changed pair marked '~', the removed one a ghost with its old label
    r = run('diff', '-v', BEFORE, AFTER)
    assert r.returncode == 1, r
    assert '~      /talker@local -> /listener@local' in r.stdout, r.stdout
    assert '-      /talker@local -> /listener_udp@host:0a0b0c0e' in r.stdout, r.stdout


def test_diff_guid_key_sees_every_restarted_pair():
    r = run('diff', '--key', 'guid', BEFORE, AFTER)
    assert r.returncode == 1, r
    assert 'changes: +5 pairs  -5 pairs' in r.stdout, r.stdout
    # the raw DDS pair kept its GUIDs, so under --all it is the one unchanged pair
    r = run('diff', '--key', 'guid', '--all', '--changes-only', BEFORE, AFTER)
    assert r.returncode == 1, r
    assert 'raw_topic' not in r.stdout, r.stdout
    r = run('diff', '--key', 'guid', '--all', BEFORE, AFTER)
    assert '   raw_topic' in r.stdout, r.stdout


def test_diff_of_a_document_with_itself_exits_0():
    for key in ('node', 'guid'):
        r = run('diff', '--key', key, BEFORE, BEFORE)
        assert r.returncode == 0, (key, r)
        assert 'changes: none' in r.stdout, r.stdout
        assert '(removed)' not in r.stdout


def test_diff_json_is_the_after_document_plus_changes():
    r = run('diff', '--json', BEFORE, AFTER)
    assert r.returncode == 1, r
    doc = json.loads(r.stdout)
    assert doc['schema_version'] == 1
    assert doc['observed_at'] == '2026-09-13T09:05:00Z'          # the after document ...
    assert [t['topic'] for t in doc['topics']] == ['/chatter', '/cmd_vel', '/new_topic']
    c = doc['changes']
    assert c['key'] == 'node'
    assert c['before'] == {'observed_at': '2026-09-13T09:00:00Z', 'domain': 0}
    assert sorted((p['topic'], p['reader_node']) for p in c['added_pairs']) == [
        ('/chatter', '/listener2'), ('/new_topic', '/sink2')]
    assert sorted((p['topic'], p['reader_node']) for p in c['removed_pairs']) == [
        ('/chatter', '/listener_udp'), ('/old_topic', '/sink')]
    assert len(c['changed_pairs']) == 1
    ch = c['changed_pairs'][0]
    assert (ch['topic'], ch['writer_node'], ch['reader_node']) == (
        '/chatter', '/talker', '/listener')
    assert ch['from']['transport'] == 'SHM' and ch['to']['transport'] == 'UDPv4'
    # after a restart the GUIDs differ: the key carries the after ones, `from` the before ones
    assert ch['writer_guid'] != ch['from']['writer_guid']
    assert ch['from']['writer_guid'] == pairs_of(json.load(open(BEFORE)), '/chatter')[0][
        'writer_guid']
    assert ch['writer_guid'] == pairs_of(json.load(open(AFTER)), '/chatter')[0]['writer_guid']
    # --changes-only prunes the unchanged /cmd_vel from `topics`, not from `changes`
    r = run('diff', '--json', '--changes-only', BEFORE, AFTER)
    assert r.returncode == 1, r
    pruned = json.loads(r.stdout)
    assert [t['topic'] for t in pruned['topics']] == ['/chatter', '/new_topic']
    assert pruned['changes'] == c


def test_diff_json_matches_the_shipped_diff_sample():
    """web/sample/diff.json is what the binary prints for the fixture pair with --all; the web
    viewer's JavaScript port of the comparison is tested against the same file."""
    r = run('diff', '--all', '--json', BEFORE, AFTER)
    assert r.returncode == 1, r
    with open(SAMPLES / 'diff.json') as f:
        assert json.loads(r.stdout) == json.load(f)


def test_diff_filters_apply_to_both_documents():
    r = run('diff', '--json', '--topic', '^/chatter$', BEFORE, AFTER)
    assert r.returncode == 1, r
    c = json.loads(r.stdout)['changes']
    assert {p['topic'] for p in c['added_pairs'] + c['removed_pairs'] + c['changed_pairs']} == {
        '/chatter'}
    r = run('diff', '--json', '--node', 'teleop', BEFORE, AFTER)
    assert r.returncode == 0, r
    assert json.loads(r.stdout)['changes']['added_pairs'] == []
    r = run('diff', '--topic', '(', BEFORE, AFTER)
    assert r.returncode == 2, r


def test_diff_reads_stdin_and_json_lines():
    with open(AFTER) as f:
        after = f.read()
    r = subprocess.run([BINARY, 'diff', BEFORE, '-'], input=after, capture_output=True,
                       text=True, timeout=30)
    assert r.returncode == 1, r
    assert 'changes: +2 pairs  -2 pairs  ~1 changed' in r.stdout
    # a --watch --json log: two compact documents, the last one counts
    lines = json.dumps(json.load(open(BEFORE))) + '\n' + json.dumps(json.load(open(AFTER))) + '\n'
    r = subprocess.run([BINARY, 'diff', BEFORE, '-'], input=lines, capture_output=True,
                       text=True, timeout=30)
    assert r.returncode == 1, r
    assert '2 documents (JSON Lines), comparing the last one' in r.stderr, r.stderr
    r = subprocess.run([BINARY, 'diff', '-', '-'], capture_output=True, text=True, timeout=30)
    assert r.returncode == 2, r
    assert 'only one of the two documents can come from stdin' in r.stderr


def test_diff_colors_marks_when_asked():
    r = run('diff', '--color', 'always', BEFORE, AFTER)
    assert r.returncode == 1, r
    assert '\033[32m+\033[0m' in r.stdout, r.stdout       # green added mark
    assert '\033[2m-\033[0m' in r.stdout, r.stdout        # dim removed mark
    assert '\033[1mchanges: \033[0m' in r.stdout, r.stdout


def test_diff_errors_exit_2(tmp_path):
    r = run('diff', BEFORE)
    assert r.returncode == 2 and 'diff needs two documents' in r.stderr, r
    r = run('diff', BEFORE, str(tmp_path / 'missing.json'))
    assert r.returncode == 2 and 'cannot read' in r.stderr, r
    bad = tmp_path / 'bad.json'
    bad.write_text('not json at all')
    r = run('diff', BEFORE, str(bad))
    assert r.returncode == 2 and 'not valid JSON' in r.stderr, r
    bad.write_text('{"schema_version": 2}')
    r = run('diff', BEFORE, str(bad))
    assert r.returncode == 2 and 'schema_version 2 is not supported' in r.stderr, r
    for flag in (['--domain', '1'], ['--timeout', '1'], ['--watch'], ['--interval', '1']):
        r = run('diff', *flag, BEFORE, AFTER)
        assert r.returncode == 2 and 'does not apply to diff' in r.stderr, (flag, r)
    r = run('--key', 'guid')
    assert r.returncode == 2 and "applies to 'transport_viz diff' only" in r.stderr, r
    r = run('diff', '--key', 'name', BEFORE, AFTER)
    assert r.returncode == 2 and '--key expects node or guid' in r.stderr, r
    r = run('bogus')
    assert r.returncode == 2 and 'unknown command: bogus' in r.stderr, r
    # a domain mismatch is a warning, not an error
    other = tmp_path / 'other_domain.json'
    doc = json.load(open(BEFORE))
    doc['domain'] = 7
    other.write_text(json.dumps(doc))
    r = run('diff', BEFORE, str(other))
    assert r.returncode == 0, r
    assert 'different domains (0 and 7)' in r.stderr, r.stderr
