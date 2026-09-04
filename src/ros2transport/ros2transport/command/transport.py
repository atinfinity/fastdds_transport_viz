# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
from ros2cli.command import add_subparsers_on_demand
from ros2cli.command import CommandExtension


class TransportCommand(CommandExtension):
    """Which Fast DDS transport (UDPv4, SHM, data-sharing, ...) each topic uses, and why."""

    def add_arguments(self, parser, cli_name):
        self._subparser = parser
        add_subparsers_on_demand(
            parser, cli_name, '_verb', 'ros2transport.verb', required=False)

    def main(self, *, parser, args):
        if not hasattr(args, '_verb'):
            self._subparser.print_help()
            return 0
        extension = getattr(args, '_verb')
        return extension.main(args=args)
