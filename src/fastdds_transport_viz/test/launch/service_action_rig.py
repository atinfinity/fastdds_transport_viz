# Copyright 2026 atinfinity
# SPDX-License-Identifier: Apache-2.0
"""
The rclpy peers of test_service_action.py (#84): argv[1] picks the role.

A service or an action is only on the wire while both sides live, and a client that has
returned takes its endpoints with it -- `ros2 service call` may be gone before the tool
looks. Each role here keeps calling until it is killed.
"""
import sys
import time

from example_interfaces.action import Fibonacci
from example_interfaces.srv import AddTwoInts
import rclpy
from rclpy.action import ActionClient, ActionServer
from rclpy.node import Node

ACTION = 'fibonacci'
SERVICE = 'add_two_ints'


def service_client():
    node = Node('ftv_srv_client')
    client = node.create_client(AddTwoInts, SERVICE)
    client.wait_for_service()
    while rclpy.ok():
        request = AddTwoInts.Request()
        request.a, request.b = 1, 2
        rclpy.spin_until_future_complete(node, client.call_async(request), timeout_sec=2.0)
        time.sleep(0.5)


def action_server():
    def execute(goal_handle):
        sequence = [0, 1]
        for i in range(1, max(2, goal_handle.request.order)):
            sequence.append(sequence[i] + sequence[i - 1])
            feedback = Fibonacci.Feedback()
            feedback.sequence = sequence
            goal_handle.publish_feedback(feedback)
            time.sleep(0.05)
        goal_handle.succeed()
        result = Fibonacci.Result()
        result.sequence = sequence
        return result

    node = Node('ftv_act_server')
    ActionServer(node, Fibonacci, ACTION, execute)
    rclpy.spin(node)


def action_client():
    node = Node('ftv_act_client')
    client = ActionClient(node, Fibonacci, ACTION)
    client.wait_for_server()
    while rclpy.ok():
        goal = Fibonacci.Goal()
        goal.order = 6
        sent = client.send_goal_async(goal, feedback_callback=lambda _: None)
        rclpy.spin_until_future_complete(node, sent, timeout_sec=5.0)
        handle = sent.result()
        if handle is not None and handle.accepted:
            rclpy.spin_until_future_complete(node, handle.get_result_async(), timeout_sec=5.0)
        time.sleep(0.5)


ROLES = {'service_client': service_client,
         'action_server': action_server,
         'action_client': action_client}

if __name__ == '__main__':
    rclpy.init()
    ROLES[sys.argv[1]]()
