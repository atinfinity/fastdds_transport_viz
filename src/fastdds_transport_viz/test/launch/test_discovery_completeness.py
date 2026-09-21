# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
The run says whether discovery had settled when it stopped (#133).

A one-shot run ends on a --quiet window without discovery events, which a system that is
still announcing its endpoints produces as easily as a settled one. The `discovery` object
of the JSON document, and a warning line on stderr, tell the two apart: every endpoint gid
the live participants announce in `ros_discovery_info` must have been discovered.

Whether a given run here ends up incomplete is a race, so this file asserts the invariant
(the warning appears exactly when `complete` is false) and the settled case. The comparison
itself and the wording of the warning are unit tests in test_decision.cpp.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, run_tool  # noqa: E402

WARNING = 'warning: discovery was still in progress'


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'talker', 'talker'),
        node_action('demo_nodes_cpp', 'listener', 'listener'),
    ]), {}


def run(extra_args):
    """Run transport_viz --json and return (document, stderr)."""
    cmd = ['ros2', 'run', 'fastdds_transport_viz', 'transport_viz', '--json', *extra_args]
    p = run_tool(cmd, timeout=60)
    return json.loads(p.stdout), p.stderr


class TestDiscoveryCompleteness(Base):

    def test_a_settled_system_is_reported_complete(self):
        # the nodes have been up since the launch: nothing is left to announce
        doc, err = run(['--timeout', '10', '--quiet', '1'])
        d = doc['discovery']
        self.assertIs(d['complete'], True, (d, err))
        self.assertEqual(d['announced_not_discovered'], 0, d)
        self.assertIn(d['stopped_on'], ('quiet', 'timeout'), d)
        self.assertGreater(d['events'], 0, d)
        self.assertGreater(d['endpoints'], 0, d)
        self.assertNotIn(WARNING, err, err)

    def test_the_warning_appears_exactly_when_the_view_is_incomplete(self):
        # 0.3 s is not enough for a full discovery round, so this run may stop with
        # announcements outstanding; whether it does is a race, the invariant is not
        doc, err = run(['--timeout', '0.3', '--quiet', '0'])
        d = doc['discovery']
        self.assertEqual(d['stopped_on'], 'timeout', d)
        if d['complete'] is False:
            self.assertGreater(d['announced_not_discovered'], 0, d)
            self.assertIn(WARNING, err, err)
            self.assertIn('--timeout', err, err)
        else:
            self.assertEqual(d['announced_not_discovered'], 0, d)
            self.assertNotIn(WARNING, err, err)

    def test_the_field_travels_through_a_saved_document(self):
        doc, _ = run(['--timeout', '10', '--quiet', '1'])
        self.assertEqual(
            set(doc['discovery']),
            {'complete', 'stopped_on', 'events', 'endpoints', 'announced_not_discovered',
             'observer_protocol', 'discovery_servers', 'discovery_server_env', 'easy_mode'})
