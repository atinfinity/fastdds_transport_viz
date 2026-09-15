// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Synthetic load for the scale verification (#74, scripts/scale_test.sh): one process of a
// system of --processes processes, each with --nodes nodes, sharing --topics topics. Every
// topic has one writer and --readers readers, the readers in other processes chosen from a
// fixed seed, so all processes (in one container or several) derive the same topology from
// the same arguments. std_msgs/String at --rate Hz, every tenth topic std_msgs/Int32
// (bounded). Parameter services, rosout and parameter events stay on, as in a real node.
//
//   scale_load --index I --processes P [--nodes N] [--topics T] [--readers R] [--seed S]
//              [--rate HZ]
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32.hpp"
#include "std_msgs/msg/string.hpp"

namespace
{

struct Args
{
  int index{-1};
  int processes{0};
  int nodes{1};
  int topics{100};
  int readers{4};
  uint32_t seed{74};
  double rate{10.0};
};

bool parse(int argc, char ** argv, Args & a)
{
  for (int i = 1; i < argc; ++i) {
    const std::string flag = argv[i];
    if (flag == "--ros-args") {break;}
    if (i + 1 >= argc) {
      std::fprintf(stderr, "%s requires a value\n", flag.c_str());
      return false;
    }
    const char * v = argv[++i];
    if (flag == "--index") {a.index = std::atoi(v);} else if (flag == "--processes") {
      a.processes = std::atoi(v);
    } else if (flag == "--nodes") {a.nodes = std::atoi(v);} else if (flag == "--topics") {
      a.topics = std::atoi(v);
    } else if (flag == "--readers") {a.readers = std::atoi(v);} else if (flag == "--seed") {
      a.seed = static_cast<uint32_t>(std::strtoul(v, nullptr, 10));
    } else if (flag == "--rate") {
      a.rate = std::atof(v);
    } else {
      std::fprintf(stderr, "unknown option: %s\n", flag.c_str());
      return false;
    }
  }
  if (a.processes < 1 || a.index < 0 || a.index >= a.processes || a.nodes < 1 || a.topics < 0 ||
    a.readers < 0 || a.rate <= 0)
  {
    std::fprintf(
      stderr, "usage: scale_load --index I --processes P [--nodes N] [--topics T] "
      "[--readers R] [--seed S] [--rate HZ]\n");
    return false;
  }
  return true;
}

struct Endpoint
{
  int process;
  int node;
};

struct TopicPlan
{
  std::string name;
  bool bounded;
  Endpoint writer;
  std::vector<Endpoint> readers;
};

/// The same plan in every process: writers round-robin over the processes, readers in
/// min(readers, processes - 1) distinct other processes drawn from the seed.
std::vector<TopicPlan> plan(const Args & a)
{
  std::vector<TopicPlan> out;
  for (int t = 0; t < a.topics; ++t) {
    std::mt19937 rng(a.seed * 1000003u + static_cast<uint32_t>(t));
    TopicPlan p;
    char name[32];
    std::snprintf(name, sizeof(name), "/scale/t%04d", t);
    p.name = name;
    p.bounded = t % 10 == 9;
    p.writer = {t % a.processes, (t / a.processes) % a.nodes};
    std::vector<int> others;
    for (int q = 0; q < a.processes; ++q) {
      if (q != p.writer.process) {others.push_back(q);}
    }
    // partial Fisher-Yates with rng() directly: the distributions are implementation-defined
    const size_t n = std::min(others.size(), static_cast<size_t>(a.readers));
    for (size_t k = 0; k < n; ++k) {
      const size_t j = k + rng() % (others.size() - k);
      std::swap(others[k], others[j]);
      p.readers.push_back({others[k], static_cast<int>(rng() % static_cast<uint32_t>(a.nodes))});
    }
    out.push_back(std::move(p));
  }
  return out;
}

}  // namespace

int main(int argc, char ** argv)
{
  Args a;
  if (!parse(argc, argv, a)) {
    return 2;
  }
  rclcpp::init(argc, argv);
  const auto topics = plan(a);

  std::vector<rclcpp::Node::SharedPtr> nodes;
  for (int n = 0; n < a.nodes; ++n) {
    char name[32];
    std::snprintf(name, sizeof(name), "scale_p%03d_n%02d", a.index, n);
    nodes.push_back(std::make_shared<rclcpp::Node>(name));
  }

  std::map<int, std::vector<rclcpp::PublisherBase::SharedPtr>> strings, ints;   // per node
  std::vector<rclcpp::SubscriptionBase::SharedPtr> subscriptions;
  for (const auto & t : topics) {
    if (t.writer.process == a.index) {
      auto & node = nodes[t.writer.node];
      if (t.bounded) {
        ints[t.writer.node].push_back(node->create_publisher<std_msgs::msg::Int32>(t.name, 10));
      } else {
        strings[t.writer.node].push_back(node->create_publisher<std_msgs::msg::String>(t.name, 10));
      }
    }
    for (const auto & r : t.readers) {
      if (r.process != a.index) {continue;}
      auto & node = nodes[r.node];
      if (t.bounded) {
        subscriptions.push_back(
          node->create_subscription<std_msgs::msg::Int32>(
            t.name, 10, [](const std_msgs::msg::Int32 &) {}));
      } else {
        subscriptions.push_back(
          node->create_subscription<std_msgs::msg::String>(
            t.name, 10, [](const std_msgs::msg::String &) {}));
      }
    }
  }

  // one timer per node publishes every topic it writes
  std::vector<rclcpp::TimerBase::SharedPtr> timers;
  int64_t count = 0;
  const auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::duration<double>(1.0 / a.rate));
  for (int n = 0; n < a.nodes; ++n) {
    timers.push_back(
      nodes[n]->create_wall_timer(
        period, [&, n]() {
          std_msgs::msg::String s;
          s.data = "scale sample " + std::to_string(count);
          for (auto & p : strings[n]) {
            std::static_pointer_cast<rclcpp::Publisher<std_msgs::msg::String>>(p)->publish(s);
          }
          std_msgs::msg::Int32 i;
          i.data = static_cast<int32_t>(count);
          for (auto & p : ints[n]) {
            std::static_pointer_cast<rclcpp::Publisher<std_msgs::msg::Int32>>(p)->publish(i);
          }
          ++count;
        }));
  }

  rclcpp::executors::SingleThreadedExecutor executor;
  for (auto & node : nodes) {
    executor.add_node(node);
  }
  executor.spin();
  rclcpp::shutdown();
  return 0;
}
