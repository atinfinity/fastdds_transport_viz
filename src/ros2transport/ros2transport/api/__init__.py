# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
Locate and run the transport_viz binary of fastdds_transport_viz.

The C++ binary keeps all Fast DDS logic and rendering; this package only translates
ros2cli arguments and replaces the current process with the binary, so tables, colors,
``--watch`` terminal handling, JSON output, ``diff`` and exit codes are exactly the
binary's.
"""
import os
import shutil
import sys

BINARY_PACKAGE = 'fastdds_transport_viz'
BINARY_NAME = 'transport_viz'
# RMWs the binary starts on (rmw_fastrtps_dynamic_cpp with a warning of its own).
SUPPORTED_RMW = ('rmw_fastrtps_cpp', 'rmw_fastrtps_dynamic_cpp')
# Overrides the lookup (tests use it to substitute a fake binary).
BINARY_ENV = 'TRANSPORT_VIZ_BINARY'


def add_list_arguments(parser):
    """Mirror the options of ``transport_viz`` (see its ``--help``)."""
    parser.add_argument(
        '--domain', type=int, metavar='ID',
        help='DDS domain id (default: $ROS_DOMAIN_ID or 0)')
    parser.add_argument(
        '--timeout', type=float, metavar='SEC',
        help='max time to wait for discovery (default: 3, 5 with --stats)')
    parser.add_argument(
        '--quiet', type=float, metavar='SEC',
        help='stop early after this many seconds without discovery events '
             '(default: 1; ignored with --stats)')
    parser.add_argument(
        '--topic', metavar='REGEX',
        help='only show topics whose (ROS) name matches the regex')
    parser.add_argument(
        '--node', metavar='REGEX',
        help='only show pairs where the writer or the reader belongs to a node whose full '
             "name matches the regex (that node's unpaired endpoints are kept too)")
    parser.add_argument(
        '--all', action='store_true',
        help='include services/actions and non-ROS DDS topics (a service or an action is one '
             'SERVICE / ACTION row per client-server pair, not its raw rq/ and rr/ topics)')
    parser.add_argument(
        '-v', '--verbose', action='store_true',
        help='expand writer -> reader pairs under each topic')
    parser.add_argument(
        '--explain', action='store_true',
        help='print a legend for every reason code used')
    parser.add_argument(
        '--locators', action='store_true',
        help='add a line under each pair with the locator the tool selected and the '
             'locators that actually carried packets (implies -v; ignored with --json, '
             'which always carries them)')
    parser.add_argument(
        '--advise', action='store_true',
        help="add a 'fix <code>: ...' line under each pair for its reason codes that have a "
             'remedy, and the remedy under each code of the legend (implies -v and --explain; '
             'ignored with --json, which always carries them as reason_code_remedies)')
    parser.add_argument(
        '--json', action='store_true',
        help='emit JSON (schema_version 1) instead of a table')
    parser.add_argument(
        '--stats', action='store_true',
        help='also subscribe to the Fast DDS statistics topics and show the transport that '
             'actually carried packets; observed nodes must run with '
             'FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;'
             'PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;'
             'RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;'
             'NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"')
    parser.add_argument(
        '--color', choices=['auto', 'always', 'never'], metavar='MODE',
        help='auto|always|never: ANSI colors for transports and warnings '
             '(default: auto = only when stdout is a terminal; honours NO_COLOR)')
    parser.add_argument(
        '--watch', action='store_true',
        help='keep observing and re-render every --interval seconds, marking added (+), '
             'changed (~) and removed (-) pairs; on a terminal, keys: q quit, p pause, '
             'v pairs, e legend, a all, l locators, f fixes. With --json, emits one compact '
             'document per line '
             '(JSON Lines) with a `changes` object')
    parser.add_argument(
        '--interval', type=float, metavar='SEC',
        help='refresh period for --watch (default: 2)')


# (argparse destination, binary option, takes a value)
_LIST_OPTIONS = (
    ('domain', '--domain', True),
    ('timeout', '--timeout', True),
    ('quiet', '--quiet', True),
    ('topic', '--topic', True),
    ('node', '--node', True),
    ('all', '--all', False),
    ('verbose', '-v', False),
    ('explain', '--explain', False),
    ('locators', '--locators', False),
    ('advise', '--advise', False),
    ('json', '--json', False),
    ('stats', '--stats', False),
    ('color', '--color', True),
    ('watch', '--watch', False),
    ('interval', '--interval', True),
)


def list_argv(args):
    """Translate parsed ``list`` arguments into transport_viz arguments."""
    argv = []
    for dest, option, has_value in _LIST_OPTIONS:
        value = getattr(args, dest, None)
        if has_value:
            if value is not None:
                argv += [option, _format(value)]
        elif value:
            argv.append(option)
    return argv


# `diff` takes the view and rendering options of `list` (not the observation ones)
_DIFF_OPTIONS = tuple(
    o for o in _LIST_OPTIONS
    if o[0] not in ('domain', 'timeout', 'quiet', 'watch', 'interval')) + (
    ('key', '--key', True),
    ('changes_only', '--changes-only', False),
)


def add_diff_arguments(parser):
    """Mirror the options of ``transport_viz diff`` (see its ``--help``)."""
    parser.add_argument(
        'before', metavar='BEFORE',
        help='a --json document of transport_viz (a JSON Lines file of --watch --json '
             "counts by its last document; '-' reads stdin)")
    parser.add_argument(
        'after', metavar='AFTER', help='the document to compare it with')
    parser.add_argument(
        '--key', choices=['node', 'guid'], metavar='MODE',
        help='node|guid: match the pairs of the two documents by (topic, writer node, reader '
             'node), which survives restarting the nodes (default), or by their GUIDs as '
             '--watch does')
    parser.add_argument(
        '--changes-only', action='store_true',
        help='only the topics with an added, changed or removed pair (with --json, `topics` '
             'is pruned the same way)')
    parser.add_argument(
        '--topic', metavar='REGEX',
        help='only compare topics whose (ROS) name matches the regex')
    parser.add_argument(
        '--node', metavar='REGEX',
        help='only compare pairs where the writer or the reader belongs to a node whose '
             'full name matches the regex')
    parser.add_argument(
        '--all', action='store_true',
        help='include services/actions and non-ROS DDS topics (a service or an action is one '
             'SERVICE / ACTION row per client-server pair, not its raw rq/ and rr/ topics)')
    parser.add_argument(
        '-v', '--verbose', action='store_true',
        help='expand writer -> reader pairs under each topic')
    parser.add_argument(
        '--explain', action='store_true',
        help='print a legend for every reason code used')
    parser.add_argument(
        '--locators', action='store_true',
        help='add a line under each pair with the selected and measured locators (implies -v)')
    parser.add_argument(
        '--advise', action='store_true',
        help="add a 'fix <code>: ...' line under each pair for its reason codes that have a "
             'remedy (implies -v and --explain)')
    parser.add_argument(
        '--json', action='store_true',
        help='emit the after document plus a `changes` object (schema_version 1) instead of '
             'a table')
    parser.add_argument(
        '--stats', action='store_true',
        help='accepted for symmetry with list; the documents decide whether measured '
             'transports are shown')
    parser.add_argument(
        '--color', choices=['auto', 'always', 'never'], metavar='MODE',
        help='auto|always|never: ANSI colors for transports, warnings and marks '
             '(default: auto = only when stdout is a terminal; honours NO_COLOR)')


def diff_argv(args):
    """Translate parsed ``diff`` arguments into transport_viz arguments."""
    argv = ['diff', args.before, args.after]
    for dest, option, has_value in _DIFF_OPTIONS:
        value = getattr(args, dest, None)
        if has_value:
            if value is not None:
                argv += [option, _format(value)]
        elif value:
            argv.append(option)
    return argv


def _format(value):
    if isinstance(value, float) and value.is_integer():
        return str(int(value))
    return str(value)


def find_binary():
    """Return the path of transport_viz, or None."""
    override = os.environ.get(BINARY_ENV)
    if override:
        return override
    try:
        from ament_index_python.packages import get_package_prefix
        from ament_index_python.packages import PackageNotFoundError
        try:
            candidate = os.path.join(
                get_package_prefix(BINARY_PACKAGE), 'lib', BINARY_PACKAGE, BINARY_NAME)
            if os.access(candidate, os.X_OK):
                return candidate
        except PackageNotFoundError:
            pass
    except ImportError:
        pass
    return shutil.which(BINARY_NAME)


def rmw_error():
    """Return the message to print instead of running the binary on another RMW, or None."""
    # An unset RMW_IMPLEMENTATION (the distro's default RMW) is left to the binary, which
    # asks the RMW layer itself.
    rmw = os.environ.get('RMW_IMPLEMENTATION')
    if not rmw or rmw in SUPPORTED_RMW:
        return None
    return (f'ros2 transport: RMW is {rmw}; this tool observes Fast DDS and needs '
            'rmw_fastrtps_cpp (set RMW_IMPLEMENTATION=rmw_fastrtps_cpp, see Limitations in '
            'the README)')


def exec_binary(argv):
    """Replace this process with ``transport_viz argv``; returns only on failure."""
    binary = find_binary()
    if binary is None:
        print(
            f"ros2 transport: '{BINARY_NAME}' not found. Build and source the "
            f'{BINARY_PACKAGE} package (see README).', file=sys.stderr)
        return 1
    sys.stdout.flush()
    sys.stderr.flush()
    try:
        os.execv(binary, [binary, *argv])
    except OSError as e:
        print(f'ros2 transport: cannot run {binary}: {e}', file=sys.stderr)
        return 1
