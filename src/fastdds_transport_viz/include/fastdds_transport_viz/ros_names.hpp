// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// rmw_fastrtps name mangling helpers and ROS node names (pure functions).

#ifndef FASTDDS_TRANSPORT_VIZ__ROS_NAMES_HPP_
#define FASTDDS_TRANSPORT_VIZ__ROS_NAMES_HPP_

#include <array>
#include <cstdint>
#include <map>
#include <cstddef>
#include <string>
#include <vector>

namespace fastdds_transport_viz
{

enum class RosEntityKind
{
  NotRos,
  Topic,           // rt/<name>
  ServiceRequest,  // rq/<name>Request
  ServiceReply,    // rr/<name>Reply
};

struct RosName
{
  RosEntityKind kind{RosEntityKind::NotRos};
  std::string name;   // "/chatter", "/add_two_ints"
};

/// "rt/chatter" -> {Topic, "/chatter"}; "rq/fooRequest" -> {ServiceRequest, "/foo"}.
RosName demangle_topic(const std::string & dds_topic);

/// "std_msgs::msg::dds_::String_" -> "std_msgs/msg/String". Returns "" when
/// the name does not look like a ROS 2 type.
std::string demangle_type(const std::string & dds_type);

/// The REP-2011 type hash out of an endpoint's USER_DATA, which the rmw encodes as
/// "key=value;" pairs under the key "typehash" (Jazzy and later; Humble announces none).
/// Returns "RIHS01_<64 lowercase hex>" when the key is there and well formed, "" otherwise:
/// USER_DATA is free for the application to use, so anything else is not a hash of ours.
std::string parse_type_hash(const std::string & user_data);

/// What the rmw reports for an endpoint whose `ros_discovery_info` it has not received.
constexpr const char kUnknownNodeName[] = "_NODE_NAMESPACE_UNKNOWN_/_NODE_NAME_UNKNOWN_";

/// "" for the rmw's unknown node name, `name` otherwise: an empty name keeps unresolved
/// endpoints apart (per GUID / participant) instead of merging them into one node.
std::string normalize_node_name(const std::string & name);

/// "/ns/name"; "/name" for an empty or root namespace.
std::string fully_qualified_node_name(const std::string & ns, const std::string & name);

/// Seconds after the last discovery event during which the ROS graph is still re-queried:
/// the ros_discovery_info sample naming an endpoint's node arrives after the endpoint does.
constexpr double kGraphRefreshGraceSeconds = 5.0;

/// Whether a --watch frame has to query the ROS graph again (#135: two rmw queries per topic,
/// seconds on a thousand topics). `events` counts discovery events and ros_discovery_info
/// samples; a graph that stays quiet past the grace period is not queried at all.
bool should_refresh_graph(
  bool refreshed_before, size_t events, size_t events_at_last_refresh,
  double seconds_since_last_event);

/// The rclcpp graph name when it is known, the name read from `ros_discovery_info` otherwise.
std::string merge_node_name(const std::string & graph_name, const std::string & discovery_name);

using EndpointGid = std::array<uint8_t, 16>;   // GUID prefix (12 bytes) + entity id
using ParticipantPrefix = std::array<uint8_t, 12>;

/// One node of a `ros_discovery_info` sample.
struct NodeEntities
{
  std::string node_namespace;
  std::string node_name;
  std::vector<EndpointGid> reader_gids;
  std::vector<EndpointGid> writer_gids;
};

/// Endpoint gid -> node name, from the `ros_discovery_info` samples of every participant.
class NodeNameTable
{
public:
  /// A sample lists every node of its participant: replaces that participant's entries.
  void update(const ParticipantPrefix & participant, const std::vector<NodeEntities> & nodes);

  /// "/ns/name" of the node owning `gid`, or "".
  std::string lookup(const EndpointGid & gid) const;

  /// Number of known endpoints.
  size_t size() const;

  /// The endpoint gids each participant announced in its latest sample (#133 compares them
  /// with what discovery delivered).
  std::map<ParticipantPrefix, std::vector<EndpointGid>> announced_by_participant() const;

private:
  // kept when a participant leaves: gids are unique, so a stale row never names another endpoint
  std::map<ParticipantPrefix, std::map<EndpointGid, std::string>> by_participant_;
};

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__ROS_NAMES_HPP_
