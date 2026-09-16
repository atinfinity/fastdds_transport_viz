// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Subscribes to the Fast DDS statistics topics published by monitored
// participants (those started with FASTDDS_STATISTICS=...) and aggregates:
//   RTPS_SENT        -> bytes/packets per (source participant, destination locator)
//   HISTORY_LATENCY  -> proof that a writer's samples reached a reader
//   PHYSICAL_DATA    -> participant -> host / user / process
//   DATA_COUNT, HISTORY_LATENCY values, and the reliability
//   counters RTPS_LOST, RESENT_DATAS, HEARTBEAT_COUNT, GAP_COUNT, ACKNACK_COUNT, NACKFRAG_COUNT

#ifndef FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_
#define FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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

/// How often the observer's own thread takes the statistics readers (#141). The readers keep
/// only the last sample of an instance, so what bounds the loss is the time between two takes,
/// not --interval: before this, a --watch run drained once per frame (2 s by default) and not
/// at all while paused, and a large system overwrote most samples in between.
inline constexpr int kStatsDrainIntervalMs = 50;
/// How many samples one drain processes before it hands the aggregate back. A drain of a large
/// system runs for over a second, and snapshot() must not wait for a whole one.
inline constexpr size_t kStatsDrainLockBatchSamples = 256;

class StatsObserver
{
public:
  /// Adds statistics readers to an existing participant and starts draining them every
  /// kStatsDrainIntervalMs until the observer is destroyed.
  explicit StatsObserver(eprosima::fastdds::dds::DomainParticipant * participant);
  ~StatsObserver();

  StatsObserver(const StatsObserver &) = delete;
  StatsObserver & operator=(const StatsObserver &) = delete;

  /// Drain every reader and return a copy of the aggregated data.
  StatsData snapshot();

  /// Drain the readers without copying. The observer's own thread calls this every
  /// kStatsDrainIntervalMs; it stays public because the tests drive a drain deterministically
  /// instead of waiting for the thread.
  void poll();

  /// The subscriber holding the statistics readers (for tests).
  eprosima::fastdds::dds::Subscriber * subscriber() const {return subscriber_;}

  /// Samples the statistics readers reported lost (a gap in a writer's sequence) or rejected
  /// (a resource limit) since they were created. `samples_lost_at_start` holds the burst that
  /// arrives while the statistics writers are still matching the readers; `samples_lost`
  /// only what was lost afterwards, which is the tool failing to keep up (#134).
  uint64_t samples_lost() const {return listener_.lost;}
  uint64_t samples_lost_at_start() const {return listener_.lost_at_start;}
  uint64_t samples_rejected() const {return listener_.rejected;}
  /// Drains that ended in an exception. The drain thread counts them instead of dying.
  uint64_t drain_errors() const {return drain_errors_;}

  /// Value for FASTDDS_STATISTICS that monitored nodes need.
  static std::string required_env_value();

private:
  struct Reader
  {
    eprosima::fastdds::dds::Topic * topic{nullptr};
    bool owns_topic{true};   // false when reusing a topic Fast DDS created (FASTDDS_STATISTICS)
    eprosima::fastdds::dds::DataReader * reader{nullptr};
  };
  /// Counts what the readers did not get (#134). A reader that has just matched a statistics
  /// writer is told about every sample the writer's keep-last history already dropped, which
  /// is not the tool falling behind: a loss within kStatisticsLateJoinGraceSeconds of a new
  /// writer match, or before the first match of all, lands in `lost_at_start`. A node joining
  /// later thus excuses five seconds of loss, not what keeps coming after it. Called from
  /// Fast DDS listener threads.
  struct Listener : public eprosima::fastdds::dds::DataReaderListener
  {
    std::atomic<uint64_t> lost{0};
    std::atomic<uint64_t> lost_at_start{0};
    std::atomic<uint64_t> rejected{0};
    /// The grace period is measured from the last writer match, so it cannot run before the
    /// first one: discovery alone takes longer than it on some distributions.
    std::atomic<bool> any_match{false};
    std::atomic<int64_t> last_match_ticks{
      std::chrono::steady_clock::now().time_since_epoch().count()};
    void on_sample_lost(
      eprosima::fastdds::dds::DataReader *,
      const eprosima::fastdds::dds::SampleLostStatus & status) override;
    void on_sample_rejected(
      eprosima::fastdds::dds::DataReader *,
      const eprosima::fastdds::dds::SampleRejectedStatus & status) override;
    void on_subscription_matched(
      eprosima::fastdds::dds::DataReader *,
      const eprosima::fastdds::dds::SubscriptionMatchedStatus & status) override;
  };
  Reader create_reader(
    const std::string & topic_name, eprosima::fastdds::dds::TypeSupport type);
  void drain();
  void drain_loop();

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

  std::mutex drain_wait_mutex_;
  std::condition_variable drain_cv_;
  std::atomic<bool> drain_stop_{false};
  std::atomic<uint64_t> drain_errors_{0};
  // declared last so that it is joined before anything it touches goes away
  std::thread drain_thread_;
};

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_
