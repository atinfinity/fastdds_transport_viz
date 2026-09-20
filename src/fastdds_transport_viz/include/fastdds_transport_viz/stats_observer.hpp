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
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/rtps/common/LocatorList.hpp>

#include "fastdds_transport_viz/decision.hpp"
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
/// The HISTORY_LATENCY reader's depth per instance (#143). Samples are counted per pair, and
/// what the reader can hold between two drains bounds the rate it can count: 10 saturated at
/// about 200 samples/s per pair, 100 counts 1000/s per pair to within 0.1 %.
inline constexpr int kStatsLatencyHistoryDepth = 100;
/// The window a --watch frame's delivered rate is measured over (#143), in whole seconds of
/// the samples' source timestamps.
inline constexpr int kStatsRateWindowSeconds = 5;

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

  /// Keep only the last kStatsRateWindowSeconds of HISTORY_LATENCY samples for the delivered
  /// rate (#143): --watch. Without it the window is the whole observation.
  void set_rate_window(bool sliding);

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
  /// The part of samples_lost() that HISTORY_LATENCY's best-effort reader reported (#141).
  uint64_t samples_lost_latency() const {return listener_.lost_latency;}
  uint64_t samples_lost_at_start() const {return listener_.lost_at_start;}
  uint64_t samples_rejected() const {return listener_.rejected;}
  /// Statistics writers that were discovered but could not be matched because their QoS is
  /// incompatible with the readers' (#141). Nothing they publish is ever received, and
  /// nothing is counted as lost either: a reader only hears about the samples of writers it
  /// did match, so without this the loss would be invisible.
  uint64_t writers_incompatible_qos() const {return listener_.incompatible_qos;}
  /// Drains that ended in an exception. The drain thread counts them instead of dying.
  uint64_t drain_errors() const {return drain_errors_;}

  /// What the settle rule of a --stats one-shot waits for (#168): the RTPS_SENT writers the
  /// reader matched (discovered and QoS-compatible - one that never matches never delivers,
  /// and must not hold the run to its cap), and those of them no sample was taken from yet,
  /// as "<topic> <writer guid>". A writer's transient-local history arrives as one handoff,
  /// so the first sample taken from it says the handoff has begun. RTPS_SENT only: it is
  /// what "measured" means, and every participant sends something (its own announcement at
  /// the least) within seconds, while RTPS_LOST, GAP_COUNT or NACKFRAG_COUNT writers match
  /// and then stay silent for as long as nothing is lost - a healthy system would wait for
  /// them until the cap.
  /// `measured_instances` is what the handoffs produce: RTPS_SENT instances whose counter
  /// moved since their first sample towards a discovered reader's unicast port (#179,
  /// measures_a_pair), the pairs' measured packets. The one-shot ends once it stops
  /// growing. The ports come from set_reader_ports(); before the first call nothing counts.
  struct Settle
  {
    size_t announced{0};
    size_t heard{0};
    size_t measured_instances{0};
    std::vector<std::string> unheard;
  };
  Settle settle_status();

  /// The unicast (kind, port) of the discovered readers (decision.hpp reader_ports), which
  /// settle_status() counts measured instances against (#179).
  void set_reader_ports(ReaderPorts ports);

  /// Value for FASTDDS_STATISTICS that monitored nodes need.
  static std::string required_env_value();

private:
  struct Reader
  {
    eprosima::fastdds::dds::Topic * topic{nullptr};
    bool owns_topic{true};   // false when reusing a topic Fast DDS created (FASTDDS_STATISTICS)
    eprosima::fastdds::dds::DataReader * reader{nullptr};
  };
  /// What the tool needs from a statistics topic, which is what its reader's QoS follows.
  enum class ReaderKind
  {
    /// PHYSICAL_DATA: one sample per participant, published once. Reliable and
    /// transient-local, or the host and pid of a participant that started before the tool
    /// would never be seen.
    kIdentity,
    /// The cumulative counters (RTPS_SENT, RTPS_LOST, DATA_COUNT, ...). The tool reports
    /// last - first, and `first` is the transient-local sample from before the observation
    /// started, so these cannot go best-effort or volatile however loud they are.
    kCounter,
    /// HISTORY_LATENCY: one sample per delivered change, only counted and averaged. By far
    /// the loudest topic, and the only one that never looks at `first`.
    kEvent,
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
    /// The part of `lost` that belongs to HISTORY_LATENCY, whose reader is best-effort by
    /// design (#141). Its samples are independent observations reduced to a percentile, so
    /// losing them coarsens a number the tool still reports, while losing a counter sample
    /// costs the measurement of an entity. Kept apart so the warning can say which happened.
    std::atomic<uint64_t> lost_latency{0};
    /// The HISTORY_LATENCY reader, to tell its losses from the counters'. Assigned right
    /// after that reader is created: a sample cannot be lost before its reader matched a
    /// writer, and a loss before the first match of all lands in `lost_at_start` anyway.
    std::atomic<eprosima::fastdds::dds::DataReader *> event_reader{nullptr};
    std::atomic<uint64_t> lost_at_start{0};
    std::atomic<uint64_t> rejected{0};
    std::atomic<uint64_t> incompatible_qos{0};
    /// The grace period is measured from the last writer match, so it cannot run before the
    /// first one: discovery alone takes longer than it on some distributions.
    std::atomic<bool> any_match{false};
    std::atomic<int64_t> last_match_ticks{
      std::chrono::steady_clock::now().time_since_epoch().count()};
    /// Per reader, the writers currently matched (#168), from the match callbacks: Fast DDS
    /// 2.14 does not implement DataReader::get_matched_publications. A writer that goes away
    /// leaves the set, so a node that exits mid-run does not hold the settle rule.
    std::mutex matched_mutex;
    std::map<const eprosima::fastdds::dds::DataReader *, std::set<std::string>> matched;
    void on_sample_lost(
      eprosima::fastdds::dds::DataReader *,
      const eprosima::fastdds::dds::SampleLostStatus & status) override;
    void on_sample_rejected(
      eprosima::fastdds::dds::DataReader *,
      const eprosima::fastdds::dds::SampleRejectedStatus & status) override;
    void on_subscription_matched(
      eprosima::fastdds::dds::DataReader * reader,
      const eprosima::fastdds::dds::SubscriptionMatchedStatus & status) override;
    void on_requested_incompatible_qos(
      eprosima::fastdds::dds::DataReader *,
      const eprosima::fastdds::dds::RequestedIncompatibleQosStatus & status) override;
  };
  Reader create_reader(
    const std::string & topic_name, eprosima::fastdds::dds::TypeSupport type,
    ReaderKind kind);
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
  ReaderPorts reader_ports_;   // #179, under mutex_
  // reporter, src, kind, addr, port: two receivers of one multicast group report the same
  // (src, dst)
  using LostKey = std::tuple<std::string, std::string, int, std::string, uint32_t>;
  std::map<LostKey, TrafficSample> lost_;
  StatsData data_;
  /// Per counter reader, the writers a sample was taken from (#168). Under mutex_.
  std::map<const eprosima::fastdds::dds::DataReader *, std::set<std::string>> heard_;
  /// HISTORY_LATENCY per second of source timestamp (#143): per pair the count and the first
  /// and last timestamp, per reader-side participant the sequence numbers of its statistics
  /// writer. Pruned to the rate window under --watch, kept whole otherwise.
  struct PairSecond {int64_t sec; DeliveryWindow w;};
  struct WriterSecond {int64_t sec; uint64_t min_seq; uint64_t max_seq; uint64_t samples;};
  std::map<std::pair<std::string, std::string>, std::deque<PairSecond>> pair_seconds_;
  std::map<std::string, std::deque<WriterSecond>> writer_seconds_;   // participant prefix
  int64_t newest_second_{INT64_MIN};
  bool sliding_window_{false};
  void prune_rate_window();

  std::mutex drain_wait_mutex_;
  std::condition_variable drain_cv_;
  std::atomic<bool> drain_stop_{false};
  std::atomic<uint64_t> drain_errors_{0};
  // declared last so that it is joined before anything it touches goes away
  std::thread drain_thread_;
};

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__STATS_OBSERVER_HPP_
