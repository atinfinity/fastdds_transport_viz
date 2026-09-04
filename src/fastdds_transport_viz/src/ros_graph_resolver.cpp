// Copyright 2026 dandelion
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/ros_graph_resolver.hpp"

#include <string>
#include <vector>

namespace fastdds_transport_viz
{

namespace
{
std::string fq_name(const std::string & ns, const std::string & name)
{
  if (ns.empty() || ns == "/") {
    return "/" + name;
  }
  return ns + "/" + name;
}
}  // namespace

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
      fresh[guid] = fq_name(info.node_namespace(), info.node_name());
    }
  }
  guid_to_node_ = std::move(fresh);
}

std::string RosGraphResolver::node_for_guid(const std::array<uint8_t, 16> & guid) const
{
  auto it = guid_to_node_.find(guid);
  return it == guid_to_node_.end() ? std::string() : it->second;
}

std::string RosGraphResolver::own_node_name() const
{
  return node_->get_fully_qualified_name();
}

}  // namespace fastdds_transport_viz
