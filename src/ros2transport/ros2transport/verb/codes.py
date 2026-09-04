# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
from ros2transport.api import exec_binary
from ros2transport.verb import VerbExtension


class CodesVerb(VerbExtension):
    """List all reason codes with their descriptions (transport_viz --list-codes)."""

    def main(self, *, args):
        return exec_binary(['--list-codes'])
