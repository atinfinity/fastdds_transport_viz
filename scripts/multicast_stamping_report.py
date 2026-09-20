#!/usr/bin/env python3
"""Ratio of RTPS_LOST to RTPS_SENT per destination locator, for the experiment of #130.

Fast DDS stamps the statistics sequence number inside the transport's per-socket send(),
from a counter keyed by destination locator alone, while RTPS_SENT is emitted once per
logical message after the socket loop. A multicast send goes out on the any-address socket
plus one socket per interface, so a sender with N interfaces burns N+1 sequence numbers per
message. A receiver in another network namespace gets one copy and reports the rest as lost.

Both numbers live in the same --stats --json document and cover the same traffic (both are
keyed per sending participant and destination locator), so the verdict is their ratio:

    lost / sent == N          for a multicast locator, N = interfaces of the sender
    lost / sent == 0          when sender and receiver share a network namespace

    multicast_stamping_report.py --sender-node /talker --multicast-expect 2 run*.json

Ratios of the individual runs are printed, never a mean: one bad run must stay visible.
"""
import argparse
import json
import sys

# Of the expected ratio; 0.1 in absolute terms when it is zero. Wide, because RTPS_LOST and
# RTPS_SENT are separate statistics streams whose counters the tool starts reading a moment
# apart, which costs a few percent of the ratio; still narrow enough that the bands of N and
# N+1 stay disjoint, which is what tells the rungs apart.
TOLERANCE = 0.20
MIN_SENT = 50  # messages; below that one message moves the ratio more than the tolerance


def is_multicast(locator):
    """IPv4 224.0.0.0/4, the only multicast this experiment uses."""
    if locator.get('kind') != 'UDPv4':
        return False
    first = locator.get('address', '').split('.')[0]
    return first.isdigit() and 224 <= int(first) <= 239


def locator_name(locator):
    return f"{locator['address']}:{locator['port']}"


def sender_prefix(doc, node):
    """The participant GUID prefix of the publishing node, from any topic it writes."""
    for topic in doc.get('topics', []):
        for writer in topic.get('writers', []):
            if writer.get('node') == node:
                return writer['participant_guid_prefix']
    return None


def delta(entry, field):
    """The counter's growth during the observation (cumulative minus the first sample)."""
    return max(0, entry[field] - entry[f'{field}_first'])


def measure(doc, node):
    """{(reporter, locator name): {'lost': int, 'sent': int, 'multicast': bool}} for one run."""
    src = sender_prefix(doc, node)
    if src is None:
        return None, {}
    stats = doc.get('stats', {})
    sent = {}
    locators = {}
    for entry in stats.get('traffic', []):
        if entry['src_participant_guid_prefix'] == src:
            name = locator_name(entry['dst_locator'])
            sent[name] = sent.get(name, 0) + delta(entry, 'packets')
            locators[name] = entry['dst_locator']
    rows = {}
    for entry in stats.get('lost', []):
        if entry['src_participant_guid_prefix'] != src:
            continue
        reporter = entry.get('reporter_participant_guid_prefix', '')
        name = locator_name(entry['dst_locator'])
        row = rows.setdefault(
            (reporter, name),
            {'lost': 0, 'sent': sent.get(name, 0), 'multicast': is_multicast(entry['dst_locator'])})
        row['lost'] += delta(entry, 'packets')
    # A locator nobody reported a loss on is a result of its own - it is the whole whitelist
    # rung - so it gets a row with no reporter rather than no row at all.
    reported = {name for _, name in rows}
    for name, count in sent.items():
        if name not in reported:
            rows[('', name)] = {'lost': 0, 'sent': count,
                                'multicast': is_multicast(locators[name])}
    return src, rows


def ratio(row):
    return row['lost'] / row['sent'] if row['sent'] else None


def within(value, expected):
    if value is None:
        return False
    if expected == 0:
        return value <= 0.1
    return abs(value - expected) <= TOLERANCE * expected


def judge(runs, multicast_expect, unicast_expect, min_sent=MIN_SENT):
    """(ok, reasons): every judged locator of every run must meet its expected ratio.

    A locator the sender used fewer than min_sent times is left out: the metatraffic group
    carries about five announcements per window, where a single message is worth a fifth of
    the ratio and the tolerance means nothing. Those rows are still printed.
    """
    reasons = []
    judged = 0
    for index, rows in enumerate(runs, start=1):
        for (reporter, name), row in sorted(rows.items()):
            expected = multicast_expect if row['multicast'] else unicast_expect
            if expected is None or row['sent'] < min_sent:
                continue
            judged += 1
            value = ratio(row)
            if not within(value, expected):
                reasons.append(
                    f'run {index}: {name} (reporter {reporter}) ratio {value:.2f}, '
                    f'expected {expected}')
    if not judged:
        reasons.append(
            f'no locator of the judged kind reached {min_sent} sent messages in any run')
    return not reasons, reasons


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('documents', nargs='+', help='transport_viz --json --stats documents')
    parser.add_argument('--sender-node', default='/talker', help='the publishing node')
    parser.add_argument('--label', default='', help='name of the rung, for the table')
    parser.add_argument('--min-sent', type=int, default=MIN_SENT,
                        help='a locator with fewer sent messages is printed but not judged')
    parser.add_argument('--multicast-expect', type=float, default=None,
                        help='expected lost/sent on multicast locators (judged when given)')
    parser.add_argument('--unicast-expect', type=float, default=None,
                        help='expected lost/sent on unicast locators (judged when given)')
    args = parser.parse_args(argv)

    runs = []
    for path in args.documents:
        with open(path, encoding='utf-8') as handle:
            doc = json.load(handle)
        src, rows = measure(doc, args.sender_node)
        if src is None:
            print(f'{path}: no writer of node {args.sender_node}', file=sys.stderr)
        runs.append(rows)

    print(f"\n== {args.label or 'ratios'}: RTPS_LOST / RTPS_SENT per locator of {args.sender_node}")
    print('| run | locator | kind | reporter | sent | lost | ratio | judged |')
    print('|---|---|---|---|---|---|---|---|')
    hidden = 0
    for index, rows in enumerate(runs, start=1):
        for (reporter, name), row in sorted(rows.items()):
            value = ratio(row)
            expected = args.multicast_expect if row['multicast'] else args.unicast_expect
            if expected is None and row['sent'] < args.min_sent:
                hidden += 1  # neither judged nor busy enough to say anything
                continue
            if expected is None:
                judged = 'no expectation'
            elif row['sent'] < args.min_sent:
                judged = 'sample too small'
            else:
                judged = f'against {expected:g}'
            print(f"| {index} | {name} | {'multicast' if row['multicast'] else 'unicast'} "
                  f"| {reporter or '-'} | {row['sent']} | {row['lost']} "
                  f"| {'-' if value is None else f'{value:.2f}'} | {judged} |")
    if hidden:
        print(f'({hidden} rows of under {args.min_sent} sent messages, of the kind this rung '
              'makes no claim about, are left out)')
    ok, reasons = judge(runs, args.multicast_expect, args.unicast_expect, args.min_sent)
    if args.multicast_expect is None and args.unicast_expect is None:
        print('(recorded only, no expectation given)')
        return 0
    for reason in reasons:
        print(f'  {reason}')
    print(f"{'PASS' if ok else 'FAIL'}: {args.label or 'ratios'}")
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
