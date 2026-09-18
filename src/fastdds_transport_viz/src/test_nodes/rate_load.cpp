// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Fixed-rate load for the delivered-rate verification (#143, scripts/integration_test.sh
// rate_stats): publishes and/or subscribes one topic at --hz. std_msgs/String (unbounded: SHM
// or UDP, never data-sharing) unless --bounded, then std_msgs/Int32 (data-sharing eligible).
// `both` puts the publisher and the subscription in one node, so Fast DDS delivers inside the
// process (rclcpp intra-process communication stays off: that path has no DDS sample to
// count). On exit the process prints what it published and received to stderr.
//
//   rate_load pub|sub|both --topic NAME [--hz HZ] [--bounded]
#include <atomic>
#include <cctype>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{
struct Options
{
  std::string mode;
  std::string topic;
  double hz{10.0};
  bool bounded{false};
};

Options parse(int argc, char ** argv)
{
  Options o;
  if (argc < 2) {
    std::fprintf(stderr, "usage: rate_load pub|sub|both --topic NAME [--hz HZ] [--bounded]\n");
    std::exit(2);
  }
  o.mode = argv[1];
  for (int i = 2; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--topic") && i + 1 < argc) {
      o.topic = argv[++i];
    } else if (!std::strcmp(argv[i], "--hz") && i + 1 < argc) {
      o.hz = std::atof(argv[++i]);
    } else if (!std::strcmp(argv[i], "--bounded")) {
      o.bounded = true;
    } else {
      std::fprintf(stderr, "rate_load: unknown argument %s\n", argv[i]);
      std::exit(2);
    }
  }
  if ((o.mode != "pub" && o.mode != "sub" && o.mode != "both") || o.topic.empty() ||
    o.hz <= 0.0)
  {
    std::fprintf(stderr, "usage: rate_load pub|sub|both --topic NAME [--hz HZ] [--bounded]\n");
    std::exit(2);
  }
  return o;
}

template<typename Msg>
int run(const Options & o, rclcpp::Node::SharedPtr node)
{
  std::atomic<uint64_t> sent{0}, received{0};
  typename rclcpp::Subscription<Msg>::SharedPtr sub;
  typename rclcpp::Publisher<Msg>::SharedPtr pub;
  if (o.mode != "pub") {
    sub = node->create_subscription<Msg>(o.topic, 10, [&](const Msg &) {++received;});
  }
  if (o.mode != "sub") {
    pub = node->create_publisher<Msg>(o.topic, 10);
  }
  rclcpp::TimerBase::SharedPtr timer;
  if (pub) {
    timer = node->create_wall_timer(
      std::chrono::nanoseconds(static_cast<int64_t>(1e9 / o.hz)), [&]() {
        Msg m;
        pub->publish(m);
        ++sent;
      });
  }
  const auto start = std::chrono::steady_clock::now();
  rclcpp::spin(node);
  const double elapsed =
    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::fprintf(
    stderr, "rate_load %s %s: published %" PRIu64 " (%.1f/s), received %" PRIu64
    " (%.1f/s) in %.1f s\n", o.mode.c_str(), o.topic.c_str(), sent.load(),
    elapsed > 0 ? sent / elapsed : 0.0, received.load(),
    elapsed > 0 ? received / elapsed : 0.0, elapsed);
  return 0;
}
}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  const Options o = parse(argc, argv);
  // one node name per topic and mode: several rate_load processes share a container
  std::string name = "rate_load_" + o.mode + "_" + o.topic;
  for (auto & c : name) {
    if (!std::isalnum(static_cast<unsigned char>(c))) {c = '_';}
  }
  auto node = std::make_shared<rclcpp::Node>(name);
  const int rc = o.bounded ? run<std_msgs::msg::Int32>(o, node) :
    run<std_msgs::msg::String>(o, node);
  rclcpp::shutdown();
  return rc;
}
