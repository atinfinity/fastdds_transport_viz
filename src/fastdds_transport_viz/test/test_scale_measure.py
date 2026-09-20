# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
The judgement of scripts/scale_measure.py (#167): which results pass, and why not.

The script is not installed; it is loaded from the source tree, like the samples of
test_cli_args.py, and the test is skipped without it.
"""

import importlib.util
import pathlib

import pytest

SCRIPT = pathlib.Path(__file__).resolve().parents[3] / 'scripts' / 'scale_measure.py'


@pytest.fixture(scope='module')
def scale_measure():
    if not SCRIPT.is_file():
        pytest.skip(f'{SCRIPT} is not in this tree')
    spec = importlib.util.spec_from_file_location('scale_measure', SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def budget(value, limit, op):
    ok = None if value is None else {'<': value < limit, '<=': value <= limit,
                                     '>=': value >= limit}[op]
    return {'value': value, 'limit': limit, 'op': op, 'pass': ok, 'description': ''}


def result(budgets, healthy=True, **extra):
    return {'budgets': budgets, 'load_healthy': healthy, **extra}


def frame_budgets(median, p95):
    return {'watch_frame_median_ms': budget(median, 250.0, '<'),
            'watch_frame_p95_ms': budget(p95, 500.0, '<')}


def test_every_budget_within_its_limit_passes(scale_measure):
    ok, failed = scale_measure.judge(result({
        **frame_budgets(180.0, 260.0),
        'stats_coverage': budget(1.0, 0.95, '>='),
    }))
    assert ok and failed == []


def test_a_failed_budget_is_named_with_its_value_and_limit(scale_measure):
    ok, failed = scale_measure.judge(result({
        **frame_budgets(180.0, 260.0),
        'stats_coverage': budget(0.0, 0.95, '>='),
    }))
    assert not ok
    assert failed == ['stats_coverage 0.0 (must be >= 0.95)']


def test_the_frame_median_and_the_p95_are_judged_on_their_own(scale_measure):
    # #177: a stall fails the p95 with a healthy median, a slow build fails the median
    ok, failed = scale_measure.judge(result(frame_budgets(180.0, 620.0)))
    assert not ok and failed == ['watch_frame_p95_ms 620.0 (must be < 500.0)']
    ok, failed = scale_measure.judge(result(frame_budgets(281.7, 386.0)))
    assert not ok and failed == ['watch_frame_median_ms 281.7 (must be < 250.0)']


def test_a_failure_says_what_the_host_was_doing(scale_measure):
    # #177: the load's statistics writers work while the tool is matched, so the host is
    # busier at the watch runs than before the tool; the FAIL line carries both
    ok, failed = scale_measure.judge(result(
        frame_budgets(281.7, 386.0),
        load_before={'vm_cores_load_only': 2.85},
        watch={'plain': {'frame_ms_median': 216.5, 'vm_cores': 4.2, 'cpu_cores': 0.61},
               'verbose': {'frame_ms_median': 281.7, 'vm_cores': 6.1, 'cpu_cores': 0.98},
               'nostats': {'frame_ms_median': 115.6, 'vm_cores': 3.9, 'cpu_cores': 0.3}}))
    assert not ok
    assert failed == ['watch_frame_median_ms 281.7 (must be < 250.0) (host: load 2.85 cores '
                      'before the tool, 6.10 during the watch run, tool 0.98 cores)']


def test_a_result_without_load_before_fails_without_a_host_share(scale_measure):
    # results recorded before load_before existed still judge
    ok, failed = scale_measure.judge(result(frame_budgets(281.7, 386.0)))
    assert not ok and failed == ['watch_frame_median_ms 281.7 (must be < 250.0)']


def test_budgets_without_a_value_are_not_judged(scale_measure):
    # Humble: no statistics module, every stats_* budget is None
    ok, failed = scale_measure.judge(result({
        'tool_rss_mb': budget(120.0, 300.0, '<'),
        'stats_watch_coverage': budget(None, 0.95, '>='),
        'stats_coverage': budget(None, 0.95, '>='),
    }))
    assert ok and failed == []


def test_a_dead_load_fails_whatever_the_budgets_say(scale_measure):
    ok, failed = scale_measure.judge(result({'tool_rss_mb': budget(120.0, 300.0, '<')},
                                            healthy=False))
    assert not ok
    assert failed == ['load processes died']


def test_the_reasons_accumulate(scale_measure):
    ok, failed = scale_measure.judge(result({
        'tool_cpu_cores': budget(1.4, 1.0, '<'),
        'stats_coverage': budget(0.5, 0.95, '>='),
    }, healthy=False))
    assert not ok
    assert failed == ['tool_cpu_cores 1.4 (must be < 1.0)', 'stats_coverage 0.5 (must be >= 0.95)',
                      'load processes died']
