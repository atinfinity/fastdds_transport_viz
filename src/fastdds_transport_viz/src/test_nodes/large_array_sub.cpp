// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Subscriber for large_array_pub (std_msgs/UInt8MultiArray on /large_array).
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/u_int8_multi_array.hpp"
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("large_array_sub");
  size_t received = 0;
  auto sub = node->create_subscription<std_msgs::msg::UInt8MultiArray>(
    "large_array", 10, [&](const std_msgs::msg::UInt8MultiArray & m) {
      if (++received % 10 == 1) {
        RCLCPP_INFO(node->get_logger(), "received %zu samples, last %zu bytes", received, m.data.size());
      }
    });
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
