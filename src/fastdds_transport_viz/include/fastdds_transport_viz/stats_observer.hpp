// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Subscribes to the Fast DDS statistics topics published by monitored
// participants (those started with FASTDDS_STATISTICS=...) and aggregates:
//   RTPS_SENT        -> bytes/packets per (source participant, destination locator)
//   HISTORY_LATENCY  -> proof that a writer's samples reached a reader
//   PHYSICAL_DATA    -> participant -> host / user / process

#ifndef FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_
#define FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>

#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

class StatsObserver
{
public:
  /// Adds statistics readers to an existing participant.
  explicit StatsObserver(eprosima::fastdds::dds::DomainParticipant * participant);
  ~StatsObserver();

  StatsObserver(const StatsObserver &) = delete;
  StatsObserver & operator=(const StatsObserver &) = delete;

  /// Drain every reader and return a copy of the aggregated data.
  StatsData snapshot();

  /// Value for FASTDDS_STATISTICS that monitored nodes need.
  static std::string required_env_value();

private:
  struct Reader
  {
    eprosima::fastdds::dds::Topic * topic{nullptr};
    bool owns_topic{true};   // false when reusing a topic Fast DDS created (FASTDDS_STATISTICS)
    eprosima::fastdds::dds::DataReader * reader{nullptr};
  };
  Reader create_reader(
    const std::string & topic_name, eprosima::fastdds::dds::TypeSupport type);
  void drain();

  eprosima::fastdds::dds::DomainParticipant * participant_;
  eprosima::fastdds::dds::Subscriber * subscriber_{nullptr};
  Reader rtps_sent_;
  Reader history_latency_;
  Reader physical_data_;
  Reader data_count_;

  std::mutex mutex_;
  using TrafficKey = std::tuple<std::string, int, std::string, uint32_t>;  // src, kind, addr, port
  std::map<TrafficKey, TrafficSample> traffic_;
  StatsData data_;
};

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_
