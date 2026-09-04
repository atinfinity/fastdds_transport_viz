// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/ros_names.hpp"

#include <string>

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

}  // namespace fastdds_transport_viz
