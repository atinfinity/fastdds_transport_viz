// Copyright 2026 dandelion
// SPDX-License-Identifier: Apache-2.0
//
// transport_viz: show which Fast DDS transport each ROS 2 topic uses and why.

#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/discovery_observer.hpp"
#include "fastdds_transport_viz/model.hpp"
#include "fastdds_transport_viz/render.hpp"
#include "fastdds_transport_viz/ros_graph_resolver.hpp"

using namespace std::chrono_literals;
using fastdds_transport_viz::Endpoint;
using fastdds_transport_viz::RenderOptions;
using fastdds_transport_viz::Snapshot;

namespace
{

struct Options
{
  int domain{-1};             // -1 => ROS_DOMAIN_ID / 0
  double timeout{3.0};        // seconds to wait for discovery
  double quiet{1.0};          // stop early after this many silent seconds
  bool json{false};
  bool verbose{false};
  bool explain{false};
  bool all{false};
  bool watch{false};
  double interval{2.0};
  std::string topic_regex;
  bool list_codes{false};
};

void usage()
{
  std::cout <<
    "Usage: transport_viz [options]\n"
    "\n"
    "Show which Fast DDS transport each ROS 2 topic is communicated over and why.\n"
    "Run it in the same environment (env vars, XML profile, network/IPC namespace)\n"
    "as the nodes you want to observe.\n"
    "\n"
    "Options:\n"
    "  --domain <id>      DDS domain id (default: $ROS_DOMAIN_ID or 0)\n"
    "  --timeout <sec>    max time to wait for discovery (default: 3)\n"
    "  --quiet <sec>      stop early after this many seconds without discovery events\n"
    "                     (default: 1)\n"
    "  --topic <regex>    only show topics whose (ROS) name matches the regex\n"
    "  --all              include services/actions and non-ROS DDS topics\n"
    "  -v, --verbose      expand writer -> reader pairs under each topic\n"
    "  --explain          print a legend for every reason code used\n"
    "  --json             emit JSON (schema_version 1) instead of a table\n"
    "  --watch            keep observing and re-render every --interval seconds\n"
    "  --interval <sec>   refresh period for --watch (default: 2)\n"
    "  --list-codes       list all reason codes with descriptions and exit\n"
    "  -h, --help         this help\n";
}

bool parse(int argc, char ** argv, Options & o)
{
  auto need = [&](int & i, const char * flag) -> const char * {
      if (i + 1 >= argc) {
        std::cerr << flag << " requires a value\n";
        std::exit(2);
      }
      return argv[++i];
    };
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--domain") {o.domain = std::atoi(need(i, "--domain"));} else if (a == "--timeout") {
      o.timeout = std::atof(need(i, "--timeout"));
    } else if (a == "--quiet") {o.quiet = std::atof(need(i, "--quiet"));} else if (a == "--topic") {
      o.topic_regex = need(i, "--topic");
    } else if (a == "--interval") {o.interval = std::atof(need(i, "--interval"));} else if (a == "--all") {
      o.all = true;
    } else if (a == "-v" || a == "--verbose") {o.verbose = true;} else if (a == "--explain") {
      o.explain = true;
    } else if (a == "--json") {o.json = true;} else if (a == "--watch") {o.watch = true;} else if (a ==
      "--list-codes")
    {
      o.list_codes = true;
    } else if (a == "-h" || a == "--help") {usage(); std::exit(0);} else if (a == "--ros-args") {
      break;   // leave ROS args to rclcpp
    } else {
      std::cerr << "unknown option: " << a << "\n";
      usage();
      return false;
    }
  }
  return true;
}

std::string now_iso8601()
{
  std::time_t t = std::time(nullptr);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
  return buf;
}

Snapshot collect(
  fastdds_transport_viz::DiscoveryObserver & observer,
  fastdds_transport_viz::RosGraphResolver & resolver,
  const Options & o, int domain, double observation_seconds)
{
  resolver.refresh();
  const std::string own = resolver.own_node_name();

  std::vector<Endpoint> endpoints = observer.snapshot();
  std::vector<Endpoint> kept;
  std::regex re;
  if (!o.topic_regex.empty()) {
    re = std::regex(o.topic_regex);
  }
  for (auto & e : endpoints) {
    e.node_name = resolver.node_for_guid(e.guid_bytes);
    // Our own rclcpp node's endpoints: topic endpoints resolve to our node
    // name, service endpoints live under our node name (services are not
    // covered by the graph API).
    if (e.node_name == own || e.ros_topic.rfind(own + "/", 0) == 0) {
      continue;
    }
    if (!o.all) {
      // Default view: ROS topics only. Services (rq/rr) and raw DDS topics need --all.
      if (e.ros_topic.empty() || e.dds_topic.rfind("rt/", 0) != 0) {
        continue;
      }
    }
    if (e.dds_topic == "ros_discovery_info" || e.dds_topic.rfind("_fastdds_", 0) == 0) {
      continue;
    }
    if (!o.topic_regex.empty()) {
      const std::string & name = e.ros_topic.empty() ? e.dds_topic : e.ros_topic;
      if (!std::regex_search(name, re)) {
        continue;
      }
    }
    kept.push_back(std::move(e));
  }

  Snapshot snap;
  snap.domain = domain;
  snap.observed_at = now_iso8601();
  snap.observation_seconds = observation_seconds;
  snap.local_host_id = observer.local_host_id();
  snap.endpoints = std::move(kept);
  snap.topics = fastdds_transport_viz::summarize(snap.endpoints);
  return snap;
}

}  // namespace

int main(int argc, char ** argv)
{
  Options o;
  if (!parse(argc, argv, o)) {
    return 2;
  }
  if (o.list_codes) {
    for (const auto & c : fastdds_transport_viz::known_codes()) {
      std::cout << c << "\n    " << fastdds_transport_viz::explain(c) << "\n";
    }
    return 0;
  }

  int domain = o.domain;
  if (domain < 0) {
    const char * env = std::getenv("ROS_DOMAIN_ID");
    domain = env ? std::atoi(env) : 0;
  }

  rclcpp::InitOptions init_opts;
  init_opts.set_domain_id(static_cast<size_t>(domain));
  rclcpp::init(argc, argv, init_opts);
  int rc = 0;
  {
    rclcpp::NodeOptions node_opts;
    node_opts.start_parameter_services(false)
    .start_parameter_event_publisher(false)
    .enable_rosout(false)
    .parameter_overrides({rclcpp::Parameter("start_type_description_service", false)});
    auto node = std::make_shared<rclcpp::Node>(
      "_transport_viz_" + std::to_string(getpid()), node_opts);
    fastdds_transport_viz::RosGraphResolver resolver(node);
    fastdds_transport_viz::DiscoveryObserver observer(domain);

    RenderOptions ropt;
    ropt.verbose = o.verbose;
    ropt.explain = o.explain;

    const auto start = std::chrono::steady_clock::now();
    // Wait until --timeout, or until discovery has been quiet for --quiet
    // seconds (but never less than --quiet seconds in total).
    for (;;) {
      std::this_thread::sleep_for(50ms);
      auto now = std::chrono::steady_clock::now();
      double elapsed = std::chrono::duration<double>(now - start).count();
      double since_last = std::chrono::duration<double>(now - observer.last_event()).count();
      if (elapsed >= o.timeout) {break;}
      if (o.quiet > 0 && elapsed >= o.quiet && since_last >= o.quiet) {break;}
      if (!rclcpp::ok()) {break;}
    }

    do {
      double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
      Snapshot snap = collect(observer, resolver, o, domain, elapsed);
      std::string out = o.json ? fastdds_transport_viz::render_json(snap, ropt) :
        fastdds_transport_viz::render_table(snap, ropt);
      if (o.watch && !o.json) {
        std::cout << "\033[2J\033[H";   // clear screen, home cursor
        std::cout << "transport_viz  domain " << domain << "  " << snap.observed_at
                  << "  (refresh " << o.interval << "s, Ctrl-C to quit)\n\n";
      }
      std::cout << out << std::flush;
      if (o.watch) {
        auto deadline = std::chrono::steady_clock::now() +
          std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::duration<double>(o.interval));
        while (rclcpp::ok() && std::chrono::steady_clock::now() < deadline) {
          std::this_thread::sleep_for(50ms);
        }
      }
    } while (o.watch && rclcpp::ok());
  }
  rclcpp::shutdown();
  return rc;
}
