# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
from ros2transport.api import add_list_arguments
from ros2transport.api import exec_binary
from ros2transport.api import list_argv
from ros2transport.verb import VerbExtension


class ListVerb(VerbExtension):
    """Show the transport of every topic (runs transport_viz)."""

    def add_arguments(self, parser, cli_name):
        add_list_arguments(parser)

    def main(self, *, args):
        return exec_binary(list_argv(args))
