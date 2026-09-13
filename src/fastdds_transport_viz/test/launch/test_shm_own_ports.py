# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
The shared-memory line does not count the tool's own participants as nodes (#51).

No nodes at all, on a private domain: every SHM port the tool sees belongs to its own
rmw and discovery participants, so no port is checked. On Lyrical and Rolling rclcpp
creates no endpoint that resolves to the tool's node name, and the tool used to report
its own ports as the nodes' (`checked_ports: [7000, ...]`).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from _common import Base, description, transport_viz_json  # noqa: E402

import launch  # noqa: E402

DOMAIN = '73'


def generate_test_description():
    # nothing of ROS: a plain process only keeps the launch alive while the test runs
    return description([launch.actions.ExecuteProcess(cmd=['sleep', '60'])]), {}


class TestShmOwnPorts(Base):

    def test_tool_alone_checks_no_port(self):
        doc = transport_viz_json(extra_args=['--all'], env={'ROS_DOMAIN_ID': DOMAIN})
        shm = doc['shm']
        self.assertTrue(shm['available'], shm)
        self.assertEqual(shm['checked_ports'], [], shm)
        self.assertEqual(shm['missing_ports'], [], shm)
        self.assertTrue(shm['nodes_visible'], shm)
        self.assertNotIn('shm-not-visible', shm['warnings'], shm)
