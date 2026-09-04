// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
// Publishes a large UInt8MultiArray (size via --size-kb, default 1024) at 5 Hz.
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/u_int8_multi_array.hpp"
using namespace std::chrono_literals;
int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  size_t kb = 1024;
  int period_ms = 200;
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--size-kb") == 0) {kb = std::stoul(argv[i + 1]);}
    if (std::strcmp(argv[i], "--period-ms") == 0) {period_ms = std::stoi(argv[i + 1]);}
  }
  auto node = std::make_shared<rclcpp::Node>("large_array_pub");
  auto pub = node->create_publisher<std_msgs::msg::UInt8MultiArray>("large_array", 10);
  std_msgs::msg::UInt8MultiArray m;
  m.data.assign(kb * 1024, 0x5a);
  auto timer = node->create_wall_timer(std::chrono::milliseconds(period_ms), [&]() {pub->publish(m);});
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
