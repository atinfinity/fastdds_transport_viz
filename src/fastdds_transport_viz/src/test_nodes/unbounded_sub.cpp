// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Subscribes to the unbounded type (String) published by unbounded_pub.
// Options: --best-effort, --transient-local (QoS for the request/offer tests).
#include <memory>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::QoS qos(10);
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--best-effort") {
      qos.best_effort();
    } else if (a == "--transient-local") {
      qos.transient_local();
    }
  }
  auto node = std::make_shared<rclcpp::Node>("unbounded_sub");
  auto sub = node->create_subscription<std_msgs::msg::String>(
    "unbounded", qos, [&](const std_msgs::msg::String & m) {
      RCLCPP_INFO(node->get_logger(), "got %s", m.data.c_str());
    });
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
