// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Reads `ros_discovery_info` (rmw_dds_common/msg/ParticipantEntitiesInfo) on the discovery
// participant and maps endpoint gids to node names. The rclcpp participant misses the
// samples of a same-host node in another IPC namespace: it announces SHM, so that node
// writes them into its own /dev/shm. This reader announces no SHM locator.

#ifndef FASTDDS_TRANSPORT_VIZ__ROS_DISCOVERY_INFO_OBSERVER_HPP_
#define FASTDDS_TRANSPORT_VIZ__ROS_DISCOVERY_INFO_OBSERVER_HPP_

#include <string>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include "fastdds_transport_viz/ros_names.hpp"

namespace fastdds_transport_viz
{

class RosDiscoveryInfoObserver
{
public:
  static constexpr const char * kTopicName = "ros_discovery_info";
  static constexpr const char * kTypeName = "rmw_dds_common::msg::dds_::ParticipantEntitiesInfo_";

  /// Adds a `ros_discovery_info` reader to an existing participant.
  explicit RosDiscoveryInfoObserver(eprosima::fastdds::dds::DomainParticipant * participant);
  ~RosDiscoveryInfoObserver();

  RosDiscoveryInfoObserver(const RosDiscoveryInfoObserver &) = delete;
  RosDiscoveryInfoObserver & operator=(const RosDiscoveryInfoObserver &) = delete;

  /// Take every received sample into the table; samples that do not decode (another
  /// distribution's layout) are skipped.
  void poll();

  /// "/ns/name" of the node owning an endpoint, from the samples taken so far, or "".
  std::string node_for_guid(const EndpointGid & gid) const {return table_.lookup(gid);}

  const NodeNameTable & table() const {return table_;}

  /// The reader (for tests).
  eprosima::fastdds::dds::DataReader * reader() const {return reader_;}

  /// Type support of rmw_dds_common/msg/ParticipantEntitiesInfo over the generated
  /// rosidl_typesupport_fastrtps_cpp callbacks (it also serializes, for tests).
  static eprosima::fastdds::dds::TypeSupport make_type_support();

private:
  eprosima::fastdds::dds::DomainParticipant * participant_;
  eprosima::fastdds::dds::Subscriber * subscriber_{nullptr};
  eprosima::fastdds::dds::Topic * topic_{nullptr};
  eprosima::fastdds::dds::DataReader * reader_{nullptr};
  NodeNameTable table_;
};

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__ROS_DISCOVERY_INFO_OBSERVER_HPP_
