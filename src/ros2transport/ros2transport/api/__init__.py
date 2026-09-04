# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Locate and run the transport_viz binary of fastdds_transport_viz.

The C++ binary keeps all Fast DDS logic and rendering; this package only translates
ros2cli arguments and replaces the current process with the binary, so tables, colors,
``--watch`` terminal handling, JSON output and exit codes are exactly the binary's.
"""
import os
import shutil
import sys

BINARY_PACKAGE = 'fastdds_transport_viz'
BINARY_NAME = 'transport_viz'
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
        '--all', action='store_true',
        help='include services/actions and non-ROS DDS topics')
    parser.add_argument(
        '-v', '--verbose', action='store_true',
        help='expand writer -> reader pairs under each topic')
    parser.add_argument(
        '--explain', action='store_true',
        help='print a legend for every reason code used')
    parser.add_argument(
        '--json', action='store_true',
        help='emit JSON (schema_version 1) instead of a table')
    parser.add_argument(
        '--stats', action='store_true',
        help='also subscribe to the Fast DDS statistics topics and show the transport that '
             'actually carried packets; observed nodes must run with '
             'FASTDDS_STATISTICS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC"')
    parser.add_argument(
        '--color', choices=['auto', 'always', 'never'], metavar='MODE',
        help='auto|always|never: ANSI colors for transports and warnings '
             '(default: auto = only when stdout is a terminal; honours NO_COLOR)')
    parser.add_argument(
        '--watch', action='store_true',
        help='keep observing and re-render every --interval seconds, marking added (+), '
             'changed (~) and removed (-) pairs; on a terminal, keys: q quit, p pause, '
             'v pairs, e legend, a all. With --json, emits one compact document per line '
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
    ('all', '--all', False),
    ('verbose', '-v', False),
    ('explain', '--explain', False),
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
