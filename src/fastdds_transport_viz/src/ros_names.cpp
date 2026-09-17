// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/ros_names.hpp"

#include <string>
#include <vector>

namespace fastdds_transport_viz
{

namespace
{
bool starts_with(const std::string & s, const std::string & p)
{
  return s.compare(0, p.size(), p) == 0;
}
bool ends_with(const std::string & s, const std::string & p)
{
  return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}
}  // namespace

RosName demangle_topic(const std::string & dds_topic)
{
  RosName out;
  if (starts_with(dds_topic, "rt/")) {
    out.kind = RosEntityKind::Topic;
    out.name = "/" + dds_topic.substr(3);
  } else if (starts_with(dds_topic, "rq/") && ends_with(dds_topic, "Request")) {
    out.kind = RosEntityKind::ServiceRequest;
    out.name = "/" + dds_topic.substr(3, dds_topic.size() - 3 - 7);
  } else if (starts_with(dds_topic, "rr/") && ends_with(dds_topic, "Reply")) {
    out.kind = RosEntityKind::ServiceReply;
    out.name = "/" + dds_topic.substr(3, dds_topic.size() - 3 - 5);
  }
  return out;
}

std::string demangle_type(const std::string & dds_type)
{
  const std::string marker = "::dds_::";
  auto pos = dds_type.find(marker);
  if (pos == std::string::npos || !ends_with(dds_type, "_")) {
    return "";
  }
  std::string pkg_and_ns = dds_type.substr(0, pos);          // "std_msgs::msg"
  std::string name = dds_type.substr(pos + marker.size());     // "String_"
  name.pop_back();
  std::string out;
  for (size_t i = 0; i < pkg_and_ns.size(); ++i) {
    if (pkg_and_ns[i] == ':' && i + 1 < pkg_and_ns.size() && pkg_and_ns[i + 1] == ':') {
      out += '/';
      ++i;
    } else {
      out += pkg_and_ns[i];
    }
  }
  return out + "/" + name;
}

std::string normalize_node_name(const std::string & name)
{
  return name == kUnknownNodeName ? std::string() : name;
}

std::string fully_qualified_node_name(const std::string & ns, const std::string & name)
{
  if (ns.empty() || ns == "/") {
    return "/" + name;
  }
  return ns + "/" + name;
}

bool should_refresh_graph(
  bool refreshed_before, size_t events, size_t events_at_last_refresh,
  double seconds_since_last_event)
{
  return !refreshed_before || events != events_at_last_refresh ||
         seconds_since_last_event <= kGraphRefreshGraceSeconds;
}

std::string merge_node_name(const std::string & graph_name, const std::string & discovery_name)
{
  std::string name = normalize_node_name(graph_name);
  return name.empty() ? normalize_node_name(discovery_name) : name;
}

void NodeNameTable::update(
  const ParticipantPrefix & participant, const std::vector<NodeEntities> & nodes)
{
  auto & entries = by_participant_[participant];
  entries.clear();
  for (const auto & node : nodes) {
    const std::string name = fully_qualified_node_name(node.node_namespace, node.node_name);
    for (const auto * gids : {&node.reader_gids, &node.writer_gids}) {
      for (const auto & gid : *gids) {
        entries[gid] = name;
      }
    }
  }
}

std::string NodeNameTable::lookup(const EndpointGid & gid) const
{
  for (const auto & kv : by_participant_) {
    auto it = kv.second.find(gid);
    if (it != kv.second.end()) {
      return it->second;
    }
  }
  return "";
}

std::map<ParticipantPrefix, std::vector<EndpointGid>> NodeNameTable::announced_by_participant()
const
{
  std::map<ParticipantPrefix, std::vector<EndpointGid>> out;
  for (const auto & kv : by_participant_) {
    auto & gids = out[kv.first];
    gids.reserve(kv.second.size());
    for (const auto & entry : kv.second) {
      gids.push_back(entry.first);
    }
  }
  return out;
}

size_t NodeNameTable::size() const
{
  size_t n = 0;
  for (const auto & kv : by_participant_) {
    n += kv.second.size();
  }
  return n;
}

}  // namespace fastdds_transport_viz
