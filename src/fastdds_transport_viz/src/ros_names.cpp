// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/ros_names.hpp"

#include <string>
#include <string_view>
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

std::string parse_type_hash(const std::string & user_data)
{
  const std::string key = "typehash=";
  for (size_t begin = 0; begin < user_data.size(); ) {
    size_t end = user_data.find(';', begin);
    if (end == std::string::npos) {
      end = user_data.size();
    }
    const std::string field = user_data.substr(begin, end - begin);
    begin = end + 1;
    if (!starts_with(field, key)) {
      continue;
    }
    const std::string value = field.substr(key.size());
    // rosidl parses the same shape: the version prefix and 32 bytes as hex digits
    const std::string prefix = "RIHS01_";
    if (value.size() != prefix.size() + 64 || !starts_with(value, prefix)) {
      return "";
    }
    for (size_t i = prefix.size(); i < value.size(); ++i) {
      const char ch = value[i];
      if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
        return "";
      }
    }
    return value;     // the first "typehash" wins, as the rmw's own parser does
  }
  return "";
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

namespace
{
constexpr std::string_view kActionSep = "/_action/";

/// The action type out of a member that names it: "example_interfaces::action::dds_::
/// Fibonacci_SendGoal_Request_" + tail "_SendGoal_Request_" -> "example_interfaces/action/
/// Fibonacci". "" when the type is not that member's.
std::string action_type_from(const std::string & dds_type, const std::string & tail)
{
  const std::string marker = "::action::dds_::";
  const auto pos = dds_type.find(marker);
  if (pos == std::string::npos) {
    return "";
  }
  const std::string pkg = dds_type.substr(0, pos);
  if (pkg.empty() || pkg.find(':') != std::string::npos) {
    return "";
  }
  const std::string rest = dds_type.substr(pos + marker.size());
  if (!ends_with(rest, tail) || rest.size() == tail.size()) {
    return "";
  }
  return pkg + "/action/" + rest.substr(0, rest.size() - tail.size());
}

struct MemberSpec
{
  const char * suffix;
  RosEntityKind kind;
  const char * type;     // a tail after "::action::dds_::", or the whole type when !names_action
  bool names_action;
};

// send_goal and get_result are services of the action's own package; cancel_goal and status
// are the action_msgs types every action shares, so they can never name the action.
const MemberSpec kMembers[] = {
  {"send_goal", RosEntityKind::ServiceRequest, "_SendGoal_Request_", true},
  {"send_goal", RosEntityKind::ServiceReply, "_SendGoal_Response_", true},
  {"get_result", RosEntityKind::ServiceRequest, "_GetResult_Request_", true},
  {"get_result", RosEntityKind::ServiceReply, "_GetResult_Response_", true},
  {"feedback", RosEntityKind::Topic, "_FeedbackMessage_", true},
  {"cancel_goal", RosEntityKind::ServiceRequest,
    "action_msgs::srv::dds_::CancelGoal_Request_", false},
  {"cancel_goal", RosEntityKind::ServiceReply,
    "action_msgs::srv::dds_::CancelGoal_Response_", false},
  {"status", RosEntityKind::Topic, "action_msgs::msg::dds_::GoalStatusArray_", false},
};
}  // namespace

ActionMember parse_action_member(const std::string & dds_topic, const std::string & dds_type)
{
  ActionMember out;
  const RosName ros = demangle_topic(dds_topic);
  if (ros.kind == RosEntityKind::NotRos) {
    return out;
  }
  // The last separator: "/foo/_action/bar/_action/send_goal" is a constructible service name.
  const auto sep = ros.name.rfind(kActionSep);
  // sep == 0 is "/_action/send_goal", a service whose action name would be empty.
  if (sep == std::string::npos || sep == 0) {
    return out;
  }
  const std::string suffix = ros.name.substr(sep + kActionSep.size());
  if (suffix.find('/') != std::string::npos) {
    return out;
  }
  for (const auto & m : kMembers) {
    if (m.kind != ros.kind || suffix != m.suffix) {
      continue;
    }
    out.matched = true;
    out.action = ros.name.substr(0, sep);
    out.suffix = suffix;
    if (m.names_action) {
      out.action_type = action_type_from(dds_type, m.type);
      out.type_ok = !out.action_type.empty();
    } else {
      out.type_ok = dds_type == m.type;
    }
    return out;
  }
  return out;
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
