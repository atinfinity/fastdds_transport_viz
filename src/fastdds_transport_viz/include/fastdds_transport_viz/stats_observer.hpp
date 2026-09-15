// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Subscribes to the Fast DDS statistics topics published by monitored
// participants (those started with FASTDDS_STATISTICS=...) and aggregates:
//   RTPS_SENT        -> bytes/packets per (source participant, destination locator)
//   HISTORY_LATENCY  -> proof that a writer's samples reached a reader
//   PHYSICAL_DATA    -> participant -> host / user / process
//   DATA_COUNT, PUBLICATION_THROUGHPUT, HISTORY_LATENCY values, and the reliability
//   counters RTPS_LOST, RESENT_DATAS, HEARTBEAT_COUNT, GAP_COUNT, ACKNACK_COUNT, NACKFRAG_COUNT

#ifndef FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_
#define FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/rtps/common/LocatorList.hpp>

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

  /// Drain the readers without copying; call periodically during the observation so
  /// that the first and the last sample of every counter are both seen (the readers
  /// keep only the latest sample per instance).
  void poll();

  /// The subscriber holding the statistics readers (for tests).
  eprosima::fastdds::dds::Subscriber * subscriber() const {return subscriber_;}

  /// Samples the statistics readers reported lost (a gap in a writer's sequence) or rejected
  /// (a resource limit) since they were created; development profiling only (FTV_PROFILE).
  uint64_t samples_lost() const {return listener_.lost;}
  uint64_t samples_rejected() const {return listener_.rejected;}

  /// Value for FASTDDS_STATISTICS that monitored nodes need.
  static std::string required_env_value();

private:
  struct Reader
  {
    eprosima::fastdds::dds::Topic * topic{nullptr};
    bool owns_topic{true};   // false when reusing a topic Fast DDS created (FASTDDS_STATISTICS)
    eprosima::fastdds::dds::DataReader * reader{nullptr};
  };
  struct Listener : public eprosima::fastdds::dds::DataReaderListener
  {
    std::atomic<uint64_t> lost{0};
    std::atomic<uint64_t> rejected{0};
    void on_sample_lost(
      eprosima::fastdds::dds::DataReader *,
      const eprosima::fastdds::dds::SampleLostStatus & status) override
    {
      lost += static_cast<uint64_t>(status.total_count_change);
    }
    void on_sample_rejected(
      eprosima::fastdds::dds::DataReader *,
      const eprosima::fastdds::dds::SampleRejectedStatus & status) override
    {
      rejected += static_cast<uint64_t>(status.total_count_change);
    }
  };
  Reader create_reader(
    const std::string & topic_name, eprosima::fastdds::dds::TypeSupport type);
  void drain();

  eprosima::fastdds::dds::DomainParticipant * participant_;
  Listener listener_;   // declared before the readers it outlives
  eprosima::fastdds::dds::Subscriber * subscriber_{nullptr};
  // unicast locators every statistics reader announces (empty: the participant's defaults)
  eprosima::fastdds::rtps::LocatorList reader_locators_;
  bool reader_locators_probed_{false};
  Reader rtps_sent_;
  Reader history_latency_;
  Reader physical_data_;
  Reader data_count_;
  Reader throughput_;
  Reader rtps_lost_;
  Reader resent_datas_;
  Reader heartbeat_count_;
  Reader gap_count_;
  Reader acknack_count_;
  Reader nackfrag_count_;

  std::mutex mutex_;
  using TrafficKey = std::tuple<std::string, int, std::string, uint32_t>;  // src, kind, addr, port
  std::map<TrafficKey, TrafficSample> traffic_;
  // reporter, src, kind, addr, port: two receivers of one multicast group report the same
  // (src, dst)
  using LostKey = std::tuple<std::string, std::string, int, std::string, uint32_t>;
  std::map<LostKey, TrafficSample> lost_;
  StatsData data_;
};

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_
