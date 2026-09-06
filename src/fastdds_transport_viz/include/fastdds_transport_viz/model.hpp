// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Plain data model shared by the observers, the decision logic and the
// renderers. Deliberately free of Fast DDS / ROS types so that decision.cpp
// can be unit-tested without a DDS runtime.

#ifndef FASTDDS_TRANSPORT_VIZ__MODEL_HPP_
#define FASTDDS_TRANSPORT_VIZ__MODEL_HPP_

#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <string>
#include <tuple>
#include <vector>

namespace fastdds_transport_viz
{

/// Locator kinds as announced during discovery (Fast DDS LOCATOR_KIND_*).
enum class LocatorKind
{
  Invalid,
  UDPv4,
  UDPv6,
  TCPv4,
  TCPv6,
  SHM,
};

struct Locator
{
  LocatorKind kind{LocatorKind::Invalid};
  std::string address;   // dotted IPv4 / IPv6 text; empty for SHM
  uint32_t port{0};
};

enum class DataSharingKind
{
  Unknown,
  Off,
  On,
  Auto,
};

struct EndpointQos
{
  std::string reliability;   // "RELIABLE" / "BEST_EFFORT"
  std::string durability;    // "VOLATILE" / "TRANSIENT_LOCAL" / ...
  DataSharingKind data_sharing{DataSharingKind::Unknown};
  std::vector<uint64_t> data_sharing_domains;   // effective domain ids announced
  // Request/offer policies Fast DDS checks when matching (infinite = no limit)
  double deadline_s{std::numeric_limits<double>::infinity()};
  // "AUTOMATIC" / "MANUAL_BY_PARTICIPANT" / "MANUAL_BY_TOPIC"
  std::string liveliness{"AUTOMATIC"};
  double liveliness_lease_s{std::numeric_limits<double>::infinity()};
  std::string ownership{"SHARED"};              // "SHARED" / "EXCLUSIVE"
  std::vector<std::string> partitions;          // empty = the default partition
};

/// Host identity = first 4 bytes of the GUID prefix (what Fast DDS itself uses
/// to decide "same host").
using HostId = std::array<uint8_t, 4>;

struct Endpoint
{
  bool is_writer{false};
  std::string guid;               // 16 bytes as "xx.xx....|xx.xx.xx.xx"
  std::array<uint8_t, 16> guid_bytes{};
  HostId host_id{};
  std::string participant_guid_prefix;   // 12 bytes hex, dotted
  std::string dds_topic;
  std::string dds_type;
  std::string ros_topic;          // demangled; empty when not a ROS topic
  std::string ros_type;           // demangled; empty when not a ROS type
  std::string node_name;          // fully-qualified ROS node name, may be empty
  std::string host_name;          // from statistics PHYSICAL_DATA, may be empty
  std::string process;            // from statistics PHYSICAL_DATA, may be empty
  std::vector<Locator> unicast;
  std::vector<Locator> multicast;
  EndpointQos qos;
  // Fast DDS < 2.10: only the SHM locator of a same-host peer is visible
  bool same_host_locators_filtered{false};
  // a data-sharing history file of this writer exists in /dev/shm
  bool datasharing_history_available{false};
  uint64_t datasharing_history_bytes{0};
};

enum class Transport
{
  UDPv4,
  UDPv6,
  TCPv4,
  TCPv6,
  SHM,
  DataSharing,
  None,
};

enum class Confidence
{
  Certain,
  Likely,
};

struct Verdict
{
  Transport transport{Transport::None};
  Confidence confidence{Confidence::Certain};
  std::vector<std::string> reasons;    // machine-readable reason codes
  std::vector<std::string> warnings;   // machine-readable warning codes
};

/// HISTORY_LATENCY samples of one writer -> reader pair (write-to-notification, seconds).
struct LatencyStat
{
  double sum{0.0};
  double min{std::numeric_limits<double>::infinity()};
  double max{-std::numeric_limits<double>::infinity()};
  double last{0.0};
  size_t samples{0};
  double mean() const {return samples ? sum / static_cast<double>(samples) : 0.0;}
  void add(double v)
  {
    sum += v; last = v; ++samples;
    if (v < min) {min = v;}
    if (v > max) {max = v;}
  }
};

/// What the Fast DDS statistics module actually observed for a pair.
struct Measurement
{
  bool available{false};             // writer's participant publishes statistics
  std::vector<Transport> transports;  // locator kinds that carried packets to the reader
  uint64_t packets{0};               // RTPS packets/bytes to the reader during the observation
  double bytes{0.0};
  uint64_t packets_total{0};         // ... and since the writer's participant started
  double bytes_total{0.0};
  bool throughput_available{false};  // writer publishes PUBLICATION_THROUGHPUT
  double throughput{0.0};            // payload bytes per second (mean over the observation)
  bool latency_available{false};     // HISTORY_LATENCY values seen for this pair
  LatencyStat latency;               // write-to-notification latency (seconds) over the observation
  bool delivered{false};             // HISTORY_LATENCY sample seen for this writer->reader
  size_t delivered_samples{0};       // HISTORY_LATENCY samples seen for this pair
  bool data_count_available{false};  // the writer's participant publishes DATA_COUNT
  uint64_t data_submessages{0};      // DATA/DATA_FRAG the writer sent through a transport
                                     // during the observation (DATA_COUNT delta)
};

struct Pair
{
  const Endpoint * writer{nullptr};
  const Endpoint * reader{nullptr};
  Verdict verdict;
  Measurement measured;
};

struct TopicSummary
{
  std::string dds_topic;
  std::string display_topic;    // ROS name when available, else DDS name
  std::string display_type;
  bool is_ros_topic{false};
  std::vector<const Endpoint *> writers;
  std::vector<const Endpoint *> readers;
  std::vector<Pair> pairs;
  std::vector<std::string> unmatched_reasons;   // e.g. no-matching-reader
  bool throughput_available{false};  // at least one writer publishes PUBLICATION_THROUGHPUT
  double throughput{0.0};            // sum of the writers' payload bytes per second
  bool latency_available{false};     // at least one pair has HISTORY_LATENCY values
  double latency{0.0};               // the slowest pair: max of the pairs' mean latency (seconds)
};

// ---- statistics module data (--stats) ----------------------------------------

struct HostInfo
{
  std::string host;
  std::string user;
  std::string process;
};

/// Latest cumulative RTPS_SENT counter for (source participant, destination locator).
struct TrafficSample
{
  std::string src_participant_prefix;   // 12-byte prefix, dotted hex
  Locator dst;
  uint64_t packets{0};          // cumulative RTPS_SENT counters at the last sample
  double bytes{0.0};
  uint64_t packets_first{0};    // ... and at the first sample of the observation
  double bytes_first{0.0};
  size_t samples{0};
};

/// PUBLICATION_THROUGHPUT samples of one writer (payload bytes per second).
struct ThroughputStat
{
  double sum{0.0};
  double last{0.0};
  size_t samples{0};
  double mean() const {return samples ? sum / static_cast<double>(samples) : 0.0;}
};

/// DATA_COUNT samples of one writer: cumulative count at the first and the last sample.
struct DataCountSample
{
  uint64_t first{0};
  uint64_t last{0};
  size_t samples{0};
};

/// DDS name of the Fast DDS DATA_COUNT statistics topic.
inline constexpr const char * kStatsDataCountTopic = "_fastdds_statistics_data_count";

struct StatsData
{
  bool enabled{false};
  std::map<std::string, HostInfo> physical;   // participant prefix -> host info
  std::vector<TrafficSample> traffic;
  // (writer, reader) guid -> HISTORY_LATENCY samples
  std::map<std::pair<std::string, std::string>, size_t> delivered;
  // (writer, reader) guid -> HISTORY_LATENCY values
  std::map<std::pair<std::string, std::string>, LatencyStat> latency;
  std::map<std::string, DataCountSample> data_count;       // writer guid -> DATA_COUNT
  std::map<std::string, ThroughputStat> throughput;        // writer guid -> PUBLICATION_THROUGHPUT
  // (participant prefix, statistics topic) discovered
  std::set<std::pair<std::string, std::string>> statistics_writers;
  std::set<std::string> participants_with_stats;           // prefixes seen on any stats topic
  /// IP addresses of the tool's own host. Fast DDS shows the locators of participants on
  /// the same host as 127.0.0.1 / ::1, while a remote writer's RTPS_SENT names the real
  /// address, so both spellings must match.
  std::set<std::string> local_addresses;
  size_t samples{0};
};

// ---- shared memory of the tool's environment -----------------------------------

/// /dev/shm capacity and what Fast DDS keeps there (see shm_info.hpp).
struct ShmInfo
{
  bool available{false};             // the directory exists and statvfs() succeeded
  std::string path;                  // e.g. "/dev/shm"
  uint64_t total_bytes{0};
  uint64_t used_bytes{0};
  uint64_t free_bytes{0};
  uint64_t fastdds_bytes{0};         // size of every fastrtps_* / fast_datasharing_* file
  size_t segments{0};                // fastrtps_<hex>: one per participant
  size_t stale_segments{0};          // ... whose lock file nobody holds (owner gone)
  size_t ports{0};                   // fastrtps_port<N>: ring buffers of the SHM locators
  size_t stale_ports{0};             // ... without a holder of the lock file
  size_t datasharing_histories{0};   // fast_datasharing_<writer guid>
  size_t datasharing_unmatched{0};   // ... not belonging to a discovered writer
  std::vector<uint32_t> checked_ports;   // SHM ports of observed nodes with the tool's host id
  std::vector<uint32_t> missing_ports;   // ... not held by a living process here, or the tool's own
  size_t other_host_participants{0};  // observed participants with another host id
  bool nodes_visible{true};          // missing_ports.empty() && other_host_participants == 0
  std::vector<std::string> warnings;  // shm-stale-files, shm-nearly-full, shm-not-visible
  std::map<std::string, uint64_t> datasharing_by_writer;   // writer guid -> history bytes
};

// ---- frame-to-frame changes (--watch) ------------------------------------------

struct PairKey
{
  std::string topic;          // display topic name
  std::string writer_guid;
  std::string reader_guid;
  bool operator<(const PairKey & o) const
  {
    return std::tie(topic, writer_guid, reader_guid) <
           std::tie(o.topic, o.writer_guid, o.reader_guid);
  }
  bool operator==(const PairKey & o) const
  {
    return topic == o.topic && writer_guid == o.writer_guid && reader_guid == o.reader_guid;
  }
};

/// The part of a pair's verdict whose change is worth highlighting.
struct PairState
{
  Transport transport{Transport::None};
  Confidence confidence{Confidence::Certain};
  std::vector<Transport> measured;
  std::vector<std::string> warnings;
  bool operator==(const PairState & o) const
  {
    return transport == o.transport && confidence == o.confidence && measured == o.measured &&
           warnings == o.warnings;
  }
  bool operator!=(const PairState & o) const {return !(*this == o);}
};

struct PairChange
{
  PairKey key;
  PairState from;
  PairState to;
};

struct Changes
{
  std::vector<PairKey> added;
  std::vector<PairKey> removed;
  std::vector<PairChange> changed;
  bool empty() const {return added.empty() && removed.empty() && changed.empty();}
};

struct Snapshot
{
  int domain{0};
  std::string observed_at;      // ISO-8601 UTC
  double observation_seconds{0.0};
  HostId local_host_id{};
  std::vector<Endpoint> endpoints;
  std::vector<TopicSummary> topics;
  StatsData stats;
  ShmInfo shm;                  // shared memory of the environment the tool runs in
  bool has_changes{false};      // true in --watch mode: `changes` is meaningful
  Changes changes;              // relative to the previously rendered frame
};

// ---- small helpers -------------------------------------------------------

std::string to_string(LocatorKind kind);
std::string to_string(Transport transport);
std::string to_string(Confidence confidence);
std::string to_string(DataSharingKind kind);
std::string host_id_hex(const HostId & id);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__MODEL_HPP_
