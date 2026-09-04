// Copyright 2026 dandelion
// SPDX-License-Identifier: Apache-2.0
// Publishes an unbounded type (String): not eligible for data-sharing.
#include <chrono>
#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
using namespace std::chrono_literals;
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("unbounded_pub");
  auto pub = node->create_publisher<std_msgs::msg::String>("unbounded", 10);
  int i = 0;
  auto timer = node->create_wall_timer(100ms, [&]() {
      std_msgs::msg::String m; m.data = "hello " + std::to_string(i++); pub->publish(m);
    });
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
