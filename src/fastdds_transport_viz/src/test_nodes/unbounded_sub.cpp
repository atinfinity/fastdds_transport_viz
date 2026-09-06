// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Subscribes to the unbounded type (String) published by unbounded_pub.
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("unbounded_sub");
  auto sub = node->create_subscription<std_msgs::msg::String>(
    "unbounded", 10, [&](const std_msgs::msg::String & m) {
      RCLCPP_INFO(node->get_logger(), "got %s", m.data.c_str());
    });
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
