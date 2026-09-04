// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// rmw_fastrtps name mangling helpers (pure functions).

#ifndef FASTDDS_TRANSPORT_VIZ__ROS_NAMES_HPP_
#define FASTDDS_TRANSPORT_VIZ__ROS_NAMES_HPP_

#include <string>

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

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__ROS_NAMES_HPP_
