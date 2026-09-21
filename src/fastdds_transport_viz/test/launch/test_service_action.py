# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
Services and actions on the wire group under one row each (#84).

`add_two_ints_server` with a client that keeps calling, and an
`example_interfaces/action/Fibonacci` server and client: `--all` must show the raw
`rq/`/`rr/` topics as one `SERVICE` row per service and one `ACTION` row per action, with
the members counted by direction of travel -- 1/1 for a service, 3/5 for an action.
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from _common import (  # noqa: E402
    Base, description, node_action, run_tool, transport_viz_json)

import launch  # noqa: E402
import launch_testing  # noqa: E402

RIG = os.path.join(os.path.dirname(__file__), 'service_action_rig.py')
SERVICE = '/add_two_ints'
ACTION = '/fibonacci'
# rcl_action creates all five at construction: three services and two topics
ACTION_MEMBERS = ('send_goal', 'cancel_goal', 'get_result', 'feedback', 'status')


def rig(role):
    return launch.actions.ExecuteProcess(cmd=[sys.executable, RIG, role], output='screen')


def generate_test_description():
    return description([
        node_action('demo_nodes_cpp', 'add_two_ints_server', 'add_two_ints_server'),
        rig('service_client'),
        rig('action_server'),
        rig('action_client'),
    ]), {}


def members(doc, group):
    return [t for t in doc['topics'] if t.get('group') == group]


class TestServiceAndActionGrouping(Base):

    def document(self, group, expected):
        """--all until `group` has all its members paired (discovery takes a moment)."""
        last = None
        for _ in range(8):
            doc = transport_viz_json(extra_args=['--all'])
            found = members(doc, group)
            if len(found) == expected and all(t['pairs'] for t in found):
                return doc, found
            last = found
            time.sleep(1.0)
        raise AssertionError(f'{group}: {[(t["dds_topic"], len(t["pairs"])) for t in last or []]}')

    def test_a_service_is_two_members_of_one_group(self):
        _, found = self.document(SERVICE, 2)
        self.assertEqual({t['kind'] for t in found}, {'service'}, found)
        self.assertEqual(sorted(t['direction'] for t in found), ['to_client', 'to_server'])
        # the DDS topic says Reply, the DDS type says _Response_
        by_dir = {t['direction']: t for t in found}
        self.assertTrue(by_dir['to_server']['dds_topic'].startswith('rq/'), by_dir)
        self.assertTrue(by_dir['to_client']['dds_topic'].startswith('rr/'), by_dir)
        self.assertEqual(by_dir['to_server']['type'], 'example_interfaces/srv/AddTwoInts_Request')
        self.assertEqual(by_dir['to_client']['type'], 'example_interfaces/srv/AddTwoInts_Response')

    def test_an_action_is_eight_members_three_out_and_five_back(self):
        _, found = self.document(ACTION, 8)
        self.assertEqual({t['kind'] for t in found}, {'action'}, found)
        directions = [t['direction'] for t in found]
        self.assertEqual(directions.count('to_server'), 3, found)
        self.assertEqual(directions.count('to_client'), 5, found)
        suffixes = {t['dds_topic'].rsplit('/_action/', 1)[1] for t in found}
        self.assertEqual(
            suffixes,
            {'send_goalRequest', 'send_goalReply', 'cancel_goalRequest', 'cancel_goalReply',
             'get_resultRequest', 'get_resultReply', 'feedback', 'status'})

    def test_a_parameter_service_nobody_calls_is_still_a_service(self):
        doc = transport_viz_json(extra_args=['--all'])
        found = members(doc, '/add_two_ints_server/list_parameters')
        self.assertEqual(len(found), 2, [t['dds_topic'] for t in doc['topics']])
        self.assertEqual({t['kind'] for t in found}, {'service'}, found)
        # nobody is calling it: each member keeps its own unmatched reason
        self.assertEqual(sorted(r for t in found for r in t['unmatched_reasons']),
                         ['no-matching-reader', 'no-matching-writer'])

    def test_a_plain_topic_keeps_its_kind(self):
        doc = transport_viz_json(extra_args=['--all'])
        rosout = [t for t in doc['topics'] if t['dds_topic'] == 'rt/rosout']
        self.assertEqual(len(rosout), 1, [t['dds_topic'] for t in doc['topics']])
        self.assertEqual([rosout[0]['kind'], rosout[0]['group'], rosout[0]['direction']],
                         ['topic', '', ''])

    def test_the_table_shows_one_row_per_service_and_action(self):
        self.document(ACTION, 8)
        out = run_tool(
            ['ros2', 'run', 'fastdds_transport_viz', 'transport_viz',
             '--all', '--timeout', '6', '--quiet', '0'],
            timeout=60).stdout
        service_rows = [line for line in out.splitlines()
                        if line.startswith(f'SERVICE {SERVICE} ')]
        action_rows = [line for line in out.splitlines()
                       if line.startswith(f'ACTION {ACTION} ')]
        self.assertEqual(len(service_rows), 1, out)
        self.assertEqual(len(action_rows), 1, out)
        # the row names both sides and counts the members each way
        self.assertIn('/add_two_ints_server', service_rows[0])
        self.assertIn('/ftv_srv_client', service_rows[0])
        self.assertIn('/ftv_act_server', action_rows[0])
        self.assertIn('/ftv_act_client', action_rows[0])
        # the raw member topics are gone from the table
        for member in ACTION_MEMBERS:
            self.assertNotIn(f'/_action/{member}', out)
        self.assertNotIn('rq/add_two_ints', out)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
