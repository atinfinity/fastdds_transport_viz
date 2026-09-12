# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
`transport_viz diff` on two live captures.

A talker runs for the whole test; the listener is started by the test itself so that it
can be restarted between two --json captures. The restart gives the listener's endpoints
new GUIDs: the default node key reports no change (exit 0), the GUID key a removed and an
added pair (exit 1). A second, renamed listener then shows up as an added pair under both.
"""
import json
import os
import signal
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, node_action, transport_viz_json  # noqa: E402

from ament_index_python.packages import get_package_prefix  # noqa: E402
import jsonschema  # noqa: E402
import launch_testing  # noqa: E402

BINARY = os.path.join(get_package_prefix('fastdds_transport_viz'), 'lib',
                      'fastdds_transport_viz', 'transport_viz')
SCHEMA = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'schema',
                      'transport_viz.schema.json')
VALIDATOR = (
    getattr(jsonschema, 'Draft202012Validator', None) or
    jsonschema.validators.validator_for({'$schema': 'http://json-schema.org/draft-07/schema#'}))


def generate_test_description():
    return description([node_action('demo_nodes_cpp', 'talker', 'talker')]), {}


def start_listener(name):
    proc = subprocess.Popen(
        ['ros2', 'run', 'demo_nodes_cpp', 'listener', '--ros-args', '-r', f'__node:={name}'],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    return proc


def stop(proc):
    os.killpg(os.getpgid(proc.pid), signal.SIGINT)
    try:
        proc.wait(timeout=10)
    except subprocess.TimeoutExpired:
        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        proc.wait()
    time.sleep(2.0)   # let the departure be discovered


def diff(*args):
    return subprocess.run([BINARY, 'diff', *args], capture_output=True, text=True, timeout=30)


class TestDiff(Base):

    def capture(self, path, reader_node):
        """--json with a /chatter pair whose reader is `reader_node`, written to `path`."""
        for _ in range(8):
            doc = transport_viz_json(['--topic', '^/chatter$'])
            chatter = next((t for t in doc['topics'] if t['topic'] == '/chatter'), None)
            if chatter and any(p['reader_node'] == reader_node for p in chatter['pairs']):
                with open(path, 'w') as f:
                    json.dump(doc, f)
                return doc
            time.sleep(1.0)
        self.fail(f'no /chatter pair with reader {reader_node} discovered')

    def test_restart_and_a_new_listener(self):
        with open(SCHEMA) as f:
            validator = VALIDATOR(json.load(f))
        tmp = os.environ.get('TMPDIR', '/tmp')
        before, after, after2 = (os.path.join(tmp, f'transport_viz_diff_{n}.json')
                                 for n in ('before', 'after', 'after2'))
        listener = start_listener('listener')
        try:
            before_doc = self.capture(before, '/listener')
        finally:
            stop(listener)
        listener = start_listener('listener')
        try:
            after_doc = self.capture(after, '/listener')
            pair_before = next(p for p in before_doc['topics'][0]['pairs']
                               if p['reader_node'] == '/listener')
            pair_after = next(p for p in after_doc['topics'][0]['pairs']
                              if p['reader_node'] == '/listener')
            self.assertNotEqual(pair_before['reader_guid'], pair_after['reader_guid'],
                                'the restart must have given the listener a new GUID')

            # node key (default): the same nodes talk the same way => no change
            r = diff(before, after)
            self.assertEqual(r.returncode, 0, r)
            self.assertIn('changes: none', r.stdout)
            # GUID key: the old pair went away, a new one appeared
            r = diff('--key', 'guid', '--json', before, after)
            self.assertEqual(r.returncode, 1, r)
            doc = json.loads(r.stdout)
            validator.validate(doc)
            self.assertEqual(doc['changes']['key'], 'guid')
            self.assertEqual(doc['changes']['before']['observed_at'], before_doc['observed_at'])
            self.assertEqual([p['reader_guid'] for p in doc['changes']['removed_pairs']],
                             [pair_before['reader_guid']])
            self.assertEqual([p['reader_guid'] for p in doc['changes']['added_pairs']],
                             [pair_after['reader_guid']])
            self.assertEqual(doc['changes']['changed_pairs'], [])

            # a second listener under another name: one added pair under both keys
            listener2 = start_listener('listener2')
            try:
                self.capture(after2, '/listener2')
            finally:
                stop(listener2)
            for key in ('node', 'guid'):
                r = diff('--key', key, '--json', '--changes-only', after, after2)
                self.assertEqual(r.returncode, 1, (key, r))
                doc = json.loads(r.stdout)
                validator.validate(doc)
                added = doc['changes']['added_pairs']
                self.assertEqual([(p['topic'], p['reader_node']) for p in added],
                                 [('/chatter', '/listener2')], key)
                self.assertEqual(doc['changes']['removed_pairs'], [], key)
                self.assertEqual([t['topic'] for t in doc['topics']], ['/chatter'], key)
            r = diff('-v', after, after2)
            self.assertEqual(r.returncode, 1, r)
            self.assertIn('+      /talker@local -> /listener2@local', r.stdout)
            self.assertIn('changes: +1 pair', r.stdout)
        finally:
            stop(listener)


@launch_testing.post_shutdown_test()
class TestShutdown(Base):

    def test_exit_codes(self, proc_info):
        pass
