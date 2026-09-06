# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""Live --json --stats output must satisfy schema/transport_viz.schema.json."""
import json
import os
import pathlib
import signal
import subprocess
import sys

import jsonschema
import launch_testing
from ament_index_python.packages import get_package_prefix

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, transport_viz_json  # noqa: E402

SCHEMA = pathlib.Path(__file__).resolve().parents[4] / 'schema' / 'transport_viz.schema.json'
STATS_ENV = {'FASTDDS_STATISTICS': 'RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC'}


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker', STATS_ENV),
        node_action('demo_nodes_cpp', 'listener', 'listener', STATS_ENV),
    ]), {}


class TestJsonSchemaLive(Base):

    def test_output_matches_schema(self):
        with open(SCHEMA) as f:
            validator = jsonschema.Draft202012Validator(json.load(f))
        for args in ([], ['--stats'], ['--all']):
            doc = transport_viz_json(args, timeout=4.0)
            validator.validate(doc)
            used = set()
            for t in doc['topics']:
                used.update(t['unmatched_reasons'])
                for p in t['pairs']:
                    used.update(p['reasons'])
                    used.update(p['warnings'])
            self.assertLessEqual(used, set(doc['reason_code_descriptions']), args)

    def test_watch_json_lines_have_changes(self):
        with open(SCHEMA) as f:
            validator = jsonschema.Draft202012Validator(json.load(f))
        # the binary itself: terminating the `ros2 run` wrapper would leave it running
        binary = os.path.join(get_package_prefix('fastdds_transport_viz'), 'lib',
                              'fastdds_transport_viz', 'transport_viz')
        proc = subprocess.Popen(
            [binary, '--watch', '--json', '--interval', '1', '--timeout', '2'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            docs = [json.loads(proc.stdout.readline()) for _ in range(2)]
        finally:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        for doc in docs:
            validator.validate(doc)
            self.assertIn('changes', doc)
        self.assertEqual(docs[0]['changes'], {'added_pairs': [], 'removed_pairs': [], 'changed_pairs': []})


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
