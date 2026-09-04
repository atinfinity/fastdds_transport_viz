// Copyright 2026 dandelion
// SPDX-License-Identifier: Apache-2.0
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("bounded_sub");
  auto sub = node->create_subscription<std_msgs::msg::Int32>(
    "bounded", 10, [&](const std_msgs::msg::Int32 & m) {
      RCLCPP_INFO_THROTTLE(node->get_logger(), *node->get_clock(), 2000, "got %d", m.data);
    });
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
