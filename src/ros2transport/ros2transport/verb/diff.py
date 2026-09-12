# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
import os
import sys

from ros2transport.api import add_diff_arguments
from ros2transport.api import diff_argv
from ros2transport.api import exec_binary
from ros2transport.verb import VerbExtension


class DiffVerb(VerbExtension):
    """Compare two --json documents: what moved between two runs (transport_viz diff)."""

    def add_arguments(self, parser, cli_name):
        add_diff_arguments(parser)

    def main(self, *, args):
        # No DDS participant is involved, so the RMW does not matter here. A missing file
        # is reported before the exec so the message comes from this command.
        for path in (args.before, args.after):
            if path != '-' and not os.path.isfile(path):
                print(f'ros2 transport diff: no such file: {path}', file=sys.stderr)
                return 2
        return exec_binary(diff_argv(args))
