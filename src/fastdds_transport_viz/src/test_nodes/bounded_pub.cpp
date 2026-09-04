// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Publishes a bounded (fixed-size) type at 10 Hz: eligible for data-sharing.
#include <chrono>
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
using namespace std::chrono_literals;
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("bounded_pub");
  auto pub = node->create_publisher<std_msgs::msg::Int32>("bounded", 10);
  int32_t i = 0;
  auto timer = node->create_wall_timer(100ms, [&]() {
      std_msgs::msg::Int32 m; m.data = i++; pub->publish(m);
    });
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
