// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Maps DDS endpoint GUIDs to ROS 2 node names using the rclcpp graph API.

#ifndef FASTDDS_TRANSPORT_VIZ__ROS_GRAPH_RESOLVER_HPP_
#define FASTDDS_TRANSPORT_VIZ__ROS_GRAPH_RESOLVER_HPP_

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"

namespace fastdds_transport_viz
{

class RosGraphResolver
{
public:
  explicit RosGraphResolver(rclcpp::Node::SharedPtr node);

  /// Re-query the ROS graph (publishers/subscriptions of every topic).
  void refresh();

  /// refresh() when should_refresh_graph() says so; returns whether it did.
  bool refresh_if_changed(size_t events, double seconds_since_last_event);

  /// Fully qualified node name ("/ns/node") for an endpoint GUID, or "".
  std::string node_for_guid(const std::array<uint8_t, 16> & guid) const;

private:
  void refresh_names();
  rclcpp::Node::SharedPtr node_;
  std::map<std::array<uint8_t, 16>, std::string> guid_to_node_;
  bool refreshed_{false};
  size_t events_at_last_refresh_{0};
};

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__ROS_GRAPH_RESOLVER_HPP_
