#!/usr/bin/env python3
# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Measure transport_viz against a running scale system (#74); run by scripts/scale_test.sh.

Runs inside the container of the observed nodes (same host id and /dev/shm), after the load
has warmed up:

  one-shot   the default table 3 times (median), plus one -v table for its line count
  --stats    --json at the default --timeout 5 and at --timeout 30: statistics coverage, dropped
             statistics samples; the 30 s document is kept for the web viewer
  --watch    --interval 2 --stats for --watch-seconds, without and with -v: frame times

Timings come from FTV_PROFILE=1 (JSON lines on stderr); CPU time and peak RSS of the tool from
wait4(), CPU of the whole VM from /proc/stat. Writes <out>/<label>.json and prints a Markdown row.
"""

import argparse
import datetime
import json
import os
import platform
import signal
import statistics
import subprocess
import sys
import tempfile
import time

BUDGETS = {
    # name: (limit, comparison, description)
    'oneshot_table_ms': (2000.0, '<', 'one-shot table after discovery (collect + render, median)'),
    'watch_frame_p95_ms': (250.0, '<', '--watch frame, p95 of both runs'),
    'tool_cpu_cores': (1.0, '<', 'tool CPU, average over the --watch and 30 s --stats runs'),
    'tool_rss_mb': (300.0, '<', 'tool peak RSS over every run'),
    'stats_dropped_samples': (0, '<=', 'statistics samples lost or rejected after the first '
                              '--watch frame (both runs)'),
    'stats_coverage': (0.95, '>=', 'measured pairs at --timeout 5 / measured pairs at 30, over '
                       'the /scale pairs when there are any'),
}


def tool_path():
    distro = os.environ.get('ROS_DISTRO', 'jazzy')
    return f'/ws/build/{distro}/install/fastdds_transport_viz/lib/fastdds_transport_viz/transport_viz'


def vm_cpu():
    with open('/proc/stat') as f:
        fields = [int(x) for x in f.readline().split()[1:]]
    idle = fields[3] + fields[4]
    return sum(fields) - idle, sum(fields)


def meminfo():
    out = {}
    with open('/proc/meminfo') as f:
        for line in f:
            key, value = line.split(':', 1)
            out[key] = int(value.split()[0]) // 1024   # MB
    return out


def other_processes(exclude):
    """Count and summed RSS (MB) of the processes in this container, by command name."""
    by_comm = {}
    for pid in os.listdir('/proc'):
        if not pid.isdigit() or int(pid) in exclude:
            continue
        try:
            with open(f'/proc/{pid}/status') as f:
                status = dict(line.split(':', 1) for line in f if ':' in line)
        except OSError:
            continue
        comm = status.get('Name', '?').strip()
        rss = int(status.get('VmRSS', '0 kB').split()[0]) / 1024
        count, total = by_comm.get(comm, (0, 0.0))
        by_comm[comm] = (count + 1, total + rss)
    return by_comm


def percentile(values, q):
    if not values:
        return None
    ordered = sorted(values)
    rank = max(0, min(len(ordered) - 1, int(round(q * len(ordered) + 0.5)) - 1))
    return ordered[rank]


def run_tool(args, seconds=None, keep_stdout=None):
    """Run transport_viz with FTV_PROFILE=1; SIGINT after `seconds` for --watch."""
    env = dict(os.environ, FTV_PROFILE='1')
    busy0, total0 = vm_cpu()
    start = time.monotonic()
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        proc = subprocess.Popen([tool_path()] + args, stdout=out, stderr=err, env=env)
        if seconds is not None:
            time.sleep(seconds)
            proc.send_signal(signal.SIGINT)
        deadline = time.monotonic() + (60 if seconds is not None else 180)
        status, rusage = 0, None
        while True:
            pid, status, rusage = os.wait4(proc.pid, os.WNOHANG)
            if pid != 0:
                break
            if time.monotonic() > deadline:
                proc.kill()
                pid, status, rusage = os.wait4(proc.pid, 0)
                status = -1
                break
            time.sleep(0.05)
        wall = time.monotonic() - start
        busy1, total1 = vm_cpu()
        out.seek(0)
        stdout = out.read()
        err.seek(0)
        stderr = err.read().decode(errors='replace')
    profile, other_stderr = [], []
    for line in stderr.splitlines():
        if line.startswith('{"ftv_profile"'):
            profile.append(json.loads(line))
        elif line.strip():
            other_stderr.append(line)
    if keep_stdout:
        with open(keep_stdout, 'wb') as f:
            f.write(stdout)
    cpu = rusage.ru_utime + rusage.ru_stime
    return {
        'args': args,
        'exit': os.waitstatus_to_exitcode(status) if status >= 0 else 'killed',
        'wall_s': round(wall, 2),
        'cpu_s': round(cpu, 2),
        'cpu_cores': round(cpu / wall, 3),
        'rss_mb': round(rusage.ru_maxrss / 1024, 1),
        'vm_cores': round((busy1 - busy0) / max(1, total1 - total0) * os.cpu_count(), 2),
        'stdout_bytes': len(stdout),
        'stderr': other_stderr[-5:],
        'profile': profile,
        'stdout': stdout,
    }


def phase(run, name):
    return [e for e in run['profile'] if e['ftv_profile'] == name]


def last(run, name, key, default=None):
    events = phase(run, name)
    return events[-1].get(key, default) if events else default


def pair_sets(document):
    """(all pairs, measured pairs, participants) of a --json document."""
    pairs, measured, participants = set(), set(), set()
    for topic in document.get('topics', []):
        for side in ('writers', 'readers'):
            for e in topic.get(side, []):
                participants.add(e.get('participant_guid_prefix'))
        for p in topic.get('pairs', []):
            key = (topic['topic'], p['writer_guid'], p['reader_guid'])
            pairs.add(key)
            m = p.get('measured') or {}
            if m.get('packets', 0) > 0 or m.get('delivered'):
                measured.add(key)
    return pairs, measured, participants


def stats_run(timeout, keep):
    run = run_tool(['--stats', '--json', '--timeout', str(timeout)], keep_stdout=keep)
    try:
        document = json.loads(run['stdout'])
    except ValueError:
        document = {}
    pairs, measured, participants = pair_sets(document)
    run['document'] = document
    run['pairs'], run['measured'], run['participants'] = pairs, measured, participants
    return run


def summarize_run(run, extra=None):
    out = {k: v for k, v in run.items()
           if k not in ('profile', 'stdout', 'document', 'pairs', 'measured', 'participants')}
    out.update(extra or {})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--label', required=True, help='output name, e.g. large or limit_p60')
    ap.add_argument('--scenario', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--warmup', type=float, default=20)
    ap.add_argument('--watch-seconds', type=float, default=60)
    ap.add_argument('--load', default='', help='description of the load, e.g. "40 processes"')
    ap.add_argument('--expected-pairs', type=int, default=0,
                    help='pairs of the synthetic /scale topics the load should create')
    ap.add_argument('--load-command', default='scale_load',
                    help='process name of the load, for its count and memory')
    ap.add_argument('--expected-load-processes', type=int, default=0)
    ap.add_argument('--date', default=datetime.date.today().isoformat(),
                    help='date of the run (the container clock is UTC)')
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    print(f'== {args.label}: warming up {args.warmup:.0f} s', file=sys.stderr, flush=True)
    time.sleep(args.warmup)
    me = {os.getpid()}
    busy0, total0 = vm_cpu()
    time.sleep(5)
    busy1, total1 = vm_cpu()
    vm_load_only = round((busy1 - busy0) / max(1, total1 - total0) * os.cpu_count(), 2)

    def load_state():
        procs = other_processes(me)
        count, rss = procs.get(args.load_command, (0, 0.0))
        mem = meminfo()
        return {
            'load_processes': count,
            'load_rss_mb': round(rss),
            'container_rss_mb': round(sum(v[1] for v in procs.values())),
            'mem_available_mb': mem.get('MemAvailable'),
        }

    result = {
        'label': args.label,
        'scenario': args.scenario,
        'date': args.date,
        'host': {
            'arch': platform.machine(),
            'cpus': os.cpu_count(),
            'mem_total_mb': meminfo().get('MemTotal'),
            'kernel': platform.release(),
        },
        'ros_distro': os.environ.get('ROS_DISTRO'),
        # Fast DDS 2.x is packaged as fastrtps, 3.x as fastdds; a missing package prints nothing
        'fastdds': subprocess.run(
            ['bash', '-c', 'dpkg-query -W -f "\\${Version}" ros-$ROS_DISTRO-fastrtps '
             'ros-$ROS_DISTRO-fastdds 2>/dev/null'],
            capture_output=True, text=True).stdout.strip(),
        'load': {'description': args.load, 'expected_scale_pairs': args.expected_pairs},
        'load_before': dict(load_state(), vm_cores_load_only=vm_load_only),
    }

    print('== one-shot table x3', file=sys.stderr, flush=True)
    oneshots = [run_tool([]) for _ in range(3)]
    table_ms = [(last(r, 'collect', 'ms', 0) + last(r, 'render', 'ms', 0)) for r in oneshots]
    verbose = run_tool(['-v'], keep_stdout=os.path.join(args.out, f'{args.label}.table-v.txt'))
    result['oneshot'] = {
        'table_ms_median': round(statistics.median(table_ms), 1),
        'discovery_ms_median': round(statistics.median(
            last(r, 'discovery', 'ms', 0) for r in oneshots)),
        'summarize_ms_median': round(statistics.median(
            last(r, 'summarize', 'ms', 0) for r in oneshots), 1),
        'pairs': [last(r, 'collect', 'pairs') for r in oneshots],
        'topics': [last(r, 'collect', 'topics') for r in oneshots],
        'verbose_lines': last(verbose, 'render', 'lines'),
        'verbose_bytes': last(verbose, 'render', 'bytes'),
        'runs': [summarize_run(r) for r in oneshots + [verbose]],
    }

    print('== --stats --json at --timeout 5 and 30', file=sys.stderr, flush=True)
    s5 = stats_run(5, os.path.join(args.out, f'{args.label}.stats5.json'))
    s30 = stats_run(30, os.path.join(args.out, f'{args.label}.viz.json'))
    # A reader that matches a statistics writer late misses the samples already gone from
    # its keep-last history and counts them as lost; that is not the tool falling behind, so
    # these one-shot counts are recorded only (the budget uses --watch growth below).
    startup_lost = {f'timeout_{t}': (last(r, 'drain', 'sample_lost', 0) or 0)
                    + (last(r, 'drain', 'sample_rejected', 0) or 0)
                    for t, r in ((5, s5), (30, s30))}
    # Topics without steady data (/parameter_events, /rosout) often have no packet in a 5 s
    # window at all, so the synthetic loads count their 10 Hz /scale pairs only.
    synthetic = any(k[0].startswith('/scale/') for k in s30['pairs'])

    def steady(keys):
        return {k for k in keys if k[0].startswith('/scale/')} if synthetic else set(keys)
    m5, m30 = steady(s5['measured']), steady(s30['measured'])
    # nothing measured even in 30 s while there are pairs: the statistics did not get through
    coverage = len(m5 & m30) / len(m30) if m30 else (0.0 if s30['pairs'] else None)
    scale_pairs = sum(1 for k in s30['pairs'] if k[0].startswith('/scale/'))
    missing_5s = {}
    for topic, _, _ in s30['measured'] - s5['measured']:
        group = '/scale/*' if topic.startswith('/scale/') else topic
        missing_5s[group] = missing_5s.get(group, 0) + 1
    result['stats'] = {
        'pairs_30s': len(s30['pairs']),
        'scale_pairs_30s': scale_pairs,
        'topics_30s': len(s30['document'].get('topics', [])),
        'participants_30s': len(s30['participants']),
        'participants_with_stats_30s': len(
            s30['document'].get('stats', {}).get('participants_with_stats', [])),
        'measured_pairs_5s': len(s5['measured']),
        'measured_pairs_30s': len(s30['measured']),
        'coverage_basis_pairs_30s': len(m30),
        'coverage': round(coverage, 4) if coverage is not None else None,
        'missing_at_5s_by_topic': missing_5s,
        'pairs_5s': len(s5['pairs']),
        'oneshot_lost_samples': startup_lost,
        'samples_30s': last(s30, 'drain', 'samples'),
        'viewer_json_bytes': len(s30['stdout']),
        'runs': [summarize_run(r, {'drain_ms_last': last(r, 'drain', 'ms'),
                                   'apply_stats_ms_last': last(r, 'apply_stats', 'ms')})
                 for r in (s5, s30)],
    }

    watch = {}
    for name, extra in (('plain', []), ('verbose', ['-v'])):
        print(f'== --watch --stats {" ".join(extra)} for {args.watch_seconds:.0f} s',
              file=sys.stderr, flush=True)
        run = run_tool(['--watch', '--interval', '2', '--stats'] + extra,
                       seconds=args.watch_seconds,
                       keep_stdout=os.path.join(args.out, f'{args.label}.watch-{name}.txt'))
        frames = [e['ms'] for e in phase(run, 'frame')]
        frame_pairs = [e.get('pairs', 0) for e in phase(run, 'frame')]
        drains = [(e.get('sample_lost', 0) or 0) + (e.get('sample_rejected', 0) or 0)
                  for e in phase(run, 'drain')]
        watch[name] = {
            'frames': len(frames),
            # A watch that sees no pair has nothing to time: fast frames, not a pass.
            'pairs_min_max': [min(frame_pairs), max(frame_pairs)] if frame_pairs else None,
            'frame_ms_median': round(statistics.median(frames), 1) if frames else None,
            'frame_ms_p95': round(percentile(frames, 0.95), 1) if frames else None,
            'update_ms_median': round(statistics.median(
                e['ms'] for e in phase(run, 'update')), 1) if frames else None,
            'phase_ms_median': {
                p: round(statistics.median(e['ms'] for e in phase(run, p)), 1)
                for p in ('drain', 'resolve', 'summarize', 'apply_stats', 'collect', 'update', 'render')
                if phase(run, p)},
            'lost_samples_first_last': [drains[0], drains[-1]] if drains else None,
            'dropped_samples': drains[-1] - drains[0] if drains else None,
            'render_lines_median': statistics.median(
                e['lines'] for e in phase(run, 'render')) if frames else None,
            **summarize_run(run),
        }
    result['watch'] = watch
    dropped = max((w['dropped_samples'] or 0) for w in watch.values())
    result['stats']['dropped_samples'] = dropped
    result['load_after'] = load_state()

    measured = {
        'oneshot_table_ms': result['oneshot']['table_ms_median'],
        'watch_frame_p95_ms': max((w['frame_ms_p95'] or float('inf')) for w in watch.values()),
        'tool_cpu_cores': max([w['cpu_cores'] for w in watch.values()]
                              + [s30['cpu_cores']]),
        'tool_rss_mb': max(r['rss_mb'] for r in oneshots + [verbose, s5, s30]
                           + [dict(rss_mb=w['rss_mb']) for w in watch.values()]),
        'stats_dropped_samples': dropped,
        'stats_coverage': coverage if coverage is not None else 0.0,
    }
    # No participant publishes statistics (Humble's Fast DDS 2.6 has no statistics module):
    # the statistics budgets have nothing to judge.
    no_stats = result['stats']['participants_with_stats_30s'] == 0
    budgets = {}
    for key, (limit, op, description) in BUDGETS.items():
        value = measured[key]
        if no_stats and key.startswith('stats_'):
            budgets[key] = {'value': None, 'limit': limit, 'op': op, 'pass': None,
                            'description': description}
            continue
        ok = {'<': value < limit, '<=': value <= limit, '>=': value >= limit}[op]
        budgets[key] = {'value': value, 'limit': limit, 'op': op, 'pass': ok,
                        'description': description}
    result['budgets'] = budgets
    healthy = (not args.expected_load_processes
               or result['load_after']['load_processes'] >= args.expected_load_processes)
    result['load_healthy'] = healthy
    result['pass'] = all(b['pass'] is not False for b in budgets.values()) and healthy

    path = os.path.join(args.out, f'{args.label}.json')
    with open(path, 'w') as f:
        json.dump(result, f, indent=2)
        f.write('\n')
    print(f'== wrote {path}', file=sys.stderr)
    print(markdown_row(result))
    return 0


def markdown_row(r):
    o, s, w = r['oneshot'], r['stats'], r['watch']
    failed = [k for k, b in r['budgets'].items() if b['pass'] is False]
    failed = [f'`{k}`' for k in failed]
    failed += [f'`--watch{" -v" if k == "verbose" else ""}` saw no pairs'
               for k, run in w.items() if not (run.get('pairs_min_max') or [0, 0])[1]]
    if not r['load_healthy']:
        failed.append('load processes died')
    verdict = 'pass' if not failed else 'over: ' + ', '.join(failed)
    return (
        f"| {r['date']} | {r['label']} | {r['ros_distro']} ({r['fastdds'].split('-')[0]}) "
        f"| {r['host']['arch']}, {r['host']['cpus']} CPU, {r['host']['mem_total_mb'] / 1024:.1f} GB "
        f"| {s['participants_30s']} / {s['topics_30s']} / {s['pairs_30s']} "
        f"| {o['discovery_ms_median'] / 1000:.1f} s / {o['table_ms_median']:.0f} ms "
        f"(pairs {min(p or 0 for p in o['pairs'])}) "
        f"| {w['plain']['frame_ms_median']} / {w['plain']['frame_ms_p95']} ms "
        f"(`-v` {w['verbose']['frame_ms_median']} / {w['verbose']['frame_ms_p95']} ms) "
        f"| {r['budgets']['tool_cpu_cores']['value']:.2f} / {r['budgets']['tool_rss_mb']['value']:.0f} MB "
        f"| {s['dropped_samples']} ({max(s['oneshot_lost_samples'].values())} at start) "
        f"/ {s['coverage'] if r['budgets']['stats_coverage']['pass'] is not None else 'n/a'} "
        f"| {o['verbose_lines']} "
        f"| {r['load_after']['load_rss_mb']} MB, {r['load_after']['mem_available_mb']} MB free "
        f"| {verdict} |")


if __name__ == '__main__':
    sys.exit(main())
