// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/ros_graph_resolver.hpp"

#include <string>
#include <vector>

#include "fastdds_transport_viz/ros_names.hpp"

namespace fastdds_transport_viz
{

RosGraphResolver::RosGraphResolver(rclcpp::Node::SharedPtr node)
: node_(std::move(node))
{
}

void RosGraphResolver::refresh()
{
  std::map<std::array<uint8_t, 16>, std::string> fresh;
  auto names_and_types = node_->get_topic_names_and_types();
  for (const auto & kv : names_and_types) {
    std::vector<rclcpp::TopicEndpointInfo> infos = node_->get_publishers_info_by_topic(kv.first);
    auto subs = node_->get_subscriptions_info_by_topic(kv.first);
    infos.insert(infos.end(), subs.begin(), subs.end());
    for (const auto & info : infos) {
      std::array<uint8_t, 16> guid{};
      const auto & gid = info.endpoint_gid();
      for (size_t i = 0; i < 16 && i < gid.size(); ++i) {
        guid[i] = gid[i];
      }
      // the rmw's unknown name becomes "": its ros_discovery_info sample was not received
      fresh[guid] = normalize_node_name(
        fully_qualified_node_name(info.node_namespace(), info.node_name()));
    }
  }
  guid_to_node_ = std::move(fresh);
}

std::string RosGraphResolver::node_for_guid(const std::array<uint8_t, 16> & guid) const
{
  auto it = guid_to_node_.find(guid);
  return it == guid_to_node_.end() ? std::string() : it->second;
}

}  // namespace fastdds_transport_viz
