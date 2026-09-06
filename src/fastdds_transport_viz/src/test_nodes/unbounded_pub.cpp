// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Publishes an unbounded type (String): not eligible for data-sharing.
// Options: --best-effort, --transient-local (QoS for the request/offer tests).
#include <chrono>
#include <memory>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
using namespace std::chrono_literals;
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
  auto node = std::make_shared<rclcpp::Node>("unbounded_pub");
  auto pub = node->create_publisher<std_msgs::msg::String>("unbounded", qos);
  int i = 0;
  auto timer = node->create_wall_timer(
    100ms, [&]() {
      std_msgs::msg::String m;
      m.data = "hello " + std::to_string(i++);
      pub->publish(m);
    });
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
