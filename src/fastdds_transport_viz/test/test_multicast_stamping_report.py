# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
The arithmetic of scripts/multicast_stamping_report.py (#130).

Which RTPS_LOST / RTPS_SENT ratio counts as the expected one, and which rows the
judgement leaves alone.

The script is not installed; it is loaded from the source tree, like test_scale_measure.py,
and the test is skipped without it.
"""

import importlib.util
import pathlib

import pytest

SCRIPT = (pathlib.Path(__file__).resolve().parents[3] / 'scripts'
          / 'multicast_stamping_report.py')

SENDER = '01.0f.00.01.02.00.00.00.00.00.00.00'
REPORTER = '01.0f.00.02.02.00.00.00.00.00.00.00'


@pytest.fixture(scope='module')
def report():
    if not SCRIPT.is_file():
        pytest.skip(f'{SCRIPT} is not in this tree')
    spec = importlib.util.spec_from_file_location('multicast_stamping_report', SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def locator(address, port=7900):
    return {'address': address, 'kind': 'UDPv4', 'port': port}


def counter(address, packets, first=0, port=7900):
    return {'dst_locator': locator(address, port), 'packets': packets, 'packets_first': first,
            'bytes': 0.0, 'bytes_first': 0.0}


def document(sent, lost, node='/talker'):
    """One --json --stats document: the sender's RTPS_SENT and the reporter's RTPS_LOST."""
    return {
        'topics': [{'writers': [{'node': node, 'participant_guid_prefix': SENDER}]}],
        'stats': {
            'traffic': [dict(entry, src_participant_guid_prefix=SENDER) for entry in sent],
            'lost': [dict(entry, src_participant_guid_prefix=SENDER,
                          reporter_participant_guid_prefix=REPORTER) for entry in lost],
        },
    }


def rows_of(report, sent, lost):
    _, rows = report.measure(document(sent, lost), '/talker')
    return rows


def test_the_ratio_counts_only_what_the_counters_gained_in_the_window(report):
    rows = rows_of(report, [counter('239.255.0.7', 900, first=300)],
                   [counter('239.255.0.7', 1500, first=300)])
    row = rows[(REPORTER, '239.255.0.7:7900')]
    assert (row['sent'], row['lost']) == (600, 1200)
    assert report.ratio(row) == 2.0


def test_an_ipv4_group_address_is_multicast_and_a_host_address_is_not(report):
    assert report.is_multicast(locator('239.255.0.7'))
    assert report.is_multicast(locator('224.0.0.1'))
    assert not report.is_multicast(locator('172.28.0.2'))
    assert not report.is_multicast({'address': 'ff02::1', 'kind': 'UDPv6', 'port': 7900})


def test_two_sockets_of_one_sender_are_one_row_per_reporter_and_locator(report):
    """The same locator can be reported twice in a window; the counts add up."""
    rows = rows_of(report, [counter('239.255.0.7', 100)],
                   [counter('239.255.0.7', 120), counter('239.255.0.7', 80)])
    assert len(rows) == 1
    assert rows[(REPORTER, '239.255.0.7:7900')]['lost'] == 200


def test_a_locator_nobody_reported_a_loss_on_is_a_row_with_no_reporter(report):
    """The whitelist rung's result is exactly this absence, and an empty table says nothing."""
    rows = rows_of(report, [counter('172.28.0.3', 900, port=7412)], [])
    assert rows[('', '172.28.0.3:7412')] == {'lost': 0, 'sent': 900, 'multicast': False}
    assert report.judge([rows], None, 0.0)[0]


def test_the_traffic_of_another_participant_is_not_the_senders(report):
    doc = document([counter('239.255.0.7', 100)], [counter('239.255.0.7', 200)])
    doc['stats']['traffic'][0]['src_participant_guid_prefix'] = REPORTER
    _, rows = report.measure(doc, '/talker')
    assert rows[(REPORTER, '239.255.0.7:7900')]['sent'] == 0


def test_a_document_without_the_sending_node_measures_nothing(report):
    src, rows = report.measure(document([], []), '/listener')
    assert src is None and rows == {}


def test_the_expected_ratio_passes_and_a_neighbouring_integer_does_not(report):
    for lost, ok in ((1800, True), (1700, True), (900, False), (2700, False)):
        runs = [rows_of(report, [counter('239.255.0.7', 900)],
                        [counter('239.255.0.7', lost)])]
        assert report.judge(runs, 2.0, None)[0] is ok


def test_the_control_expects_no_loss_at_all(report):
    runs = [rows_of(report, [counter('239.255.0.7', 900)], [counter('239.255.0.7', 0)])]
    assert report.judge(runs, 0.0, None)[0]
    runs = [rows_of(report, [counter('239.255.0.7', 900)], [counter('239.255.0.7', 180)])]
    assert not report.judge(runs, 0.0, None)[0]


def test_a_locator_the_sender_barely_used_is_not_judged(report):
    """The metatraffic group carries a handful of announcements: too few to divide."""
    runs = [rows_of(report, [counter('239.255.0.1', 5, port=7400)],
                    [counter('239.255.0.1', 28, port=7400)])]
    ok, reasons = report.judge(runs, 2.0, None)
    assert not ok and 'sent messages in any run' in reasons[0]


def test_a_unicast_locator_is_judged_only_against_the_unicast_expectation(report):
    runs = [rows_of(report, [counter('172.28.0.3', 900, port=7412)],
                    [counter('172.28.0.3', 0, port=7412)])]
    assert report.judge(runs, 2.0, None) == (False, [
        'no locator of the judged kind reached 50 sent messages in any run'])
    assert report.judge(runs, None, 0.0)[0]


def test_every_run_is_judged_on_its_own_and_one_bad_run_fails(report):
    good = rows_of(report, [counter('239.255.0.7', 900)], [counter('239.255.0.7', 1800)])
    bad = rows_of(report, [counter('239.255.0.7', 900)], [counter('239.255.0.7', 900)])
    ok, reasons = report.judge([good, bad, good], 2.0, None)
    assert not ok
    assert reasons == ['run 2: 239.255.0.7:7900 (reporter %s) ratio 1.00, expected 2.0'
                       % REPORTER]
