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
#include <optional>
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
  bool operator==(const Locator & o) const
  {
    return kind == o.kind && address == o.address && port == o.port;
  }
  bool operator!=(const Locator & o) const {return !(*this == o);}
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

/// Whether a participant with the tool's host id listens for SHM in the tool's IPC
/// namespace, from the locks of its SHM ports in the tool's /dev/shm.
enum class ShmVisibility
{
  Unprobed,     // not decidable: no probe, a lock that could not be read, only shared numbers
  Visible,      // every SHM port held here, one of them announced by no other participant
  NotVisible,   // a port nobody holds here, or one that collides with the tool's own ports
};

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
  // SHM unicast ports of every endpoint of this endpoint's participant, filtered or not
  // (ros_discovery_info included), and where they are seen from the tool: two participants
  // in different IPC namespaces lose every sample they send each other over SHM
  std::set<uint32_t> participant_shm_ports;
  ShmVisibility participant_shm_visibility{ShmVisibility::Unprobed};
  // this endpoint's data-sharing segment (a writer's history, a reader's notification) as
  // seen in the tool's /dev/shm: Visible / NotVisible only for the tool's host id and a
  // listed directory. Internal, not in the JSON.
  ShmVisibility datasharing_segment_visibility{ShmVisibility::Unprobed};
  // rmw_fastrtps_cpp native buffers (Lyrical and later): the companion endpoint on
  // <topic>/_buf_cpu names the writer / reader of the parent topic it carries the data of,
  // and the parent lists its companions (internal, not in the JSON)
  std::string buffer_parent_guid;
  std::vector<std::string> buffer_companion_guids;
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
  // The reader locator the decision selected: the address (or, for SHM, the /dev/shm
  // port) the writer will deliver to. Kind Invalid when none was selected: a
  // DATA_SHARING or NONE verdict, or Fast DDS < 2.10 hiding the locators of a same-host
  // peer (reason same-host-locators-hidden).
  Locator locator;
  bool locator_multicast{false};
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

/// Reliability counters of a pair over the observation (window deltas).
struct Reliability
{
  bool available{false};        // at least one of the counters below was reported
  // RTPS_LOST can be read for this pair: the reader's participant publishes it (only on a
  // sequence-number gap, so no sample means nothing lost)
  bool lost_available{false};
  // RTPS_LOST packets the reader's participant missed from the writer's participant on the
  // reader's unicast locators (participant level: shared by every pair of the two)
  uint64_t lost_packets{0};
  uint64_t resent{0};           // RESENT_DATAS of the writer
  uint64_t heartbeats{0};       // HEARTBEAT_COUNT of the writer
  uint64_t gaps{0};             // GAP_COUNT of the writer
  uint64_t acknacks{0};         // ACKNACK_COUNT of the reader
  uint64_t nackfrags{0};        // NACKFRAG_COUNT of the reader
};

/// One destination locator that carried packets of a pair, with that locator's share of
/// the pair's traffic.
struct MeasuredLocator
{
  Locator locator;
  uint64_t packets{0};   // window deltas, so the entries sum to Measurement::packets
  double bytes{0.0};     // ... and to Measurement::bytes
};

/// What the Fast DDS statistics module actually observed for a pair.
struct Measurement
{
  bool available{false};             // writer's participant publishes statistics
  std::vector<Transport> transports;  // locator kinds that carried packets to the reader
  std::vector<MeasuredLocator> locators;   // ... and the locators behind those kinds
  uint64_t packets{0};               // RTPS packets/bytes to the reader during the observation
  double bytes{0.0};
  uint64_t packets_total{0};         // ... and since the writer's participant started
  double bytes_total{0.0};
  bool latency_available{false};     // HISTORY_LATENCY values seen for this pair
  LatencyStat latency;               // write-to-notification latency (seconds) over the observation
  bool delivered{false};             // HISTORY_LATENCY sample seen for this writer->reader
  size_t delivered_samples{0};       // HISTORY_LATENCY samples seen for this pair
  Reliability reliability;           // losses, resends, heartbeats, acknacks (window deltas)
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
  bool latency_available{false};     // at least one pair has HISTORY_LATENCY values
  double latency{0.0};               // the slowest pair: max of the pairs' mean latency (seconds)
  bool reliability_available{false};  // at least one pair has reliability counters
  bool lost_available{false};        // at least one pair has lost_available
  uint64_t lost_packets{0};          // over the pairs, each RTPS_LOST entry counted once
  uint64_t resent{0};
};

// ---- statistics module data (--stats) ----------------------------------------

struct HostInfo
{
  std::string host;
  std::string user;
  std::string process;
};

/// Latest cumulative RTPS_SENT (RTPS_LOST) counter for (source participant, destination
/// locator).
struct TrafficSample
{
  std::string src_participant_prefix;   // 12-byte prefix, dotted hex
  Locator dst;
  uint64_t packets{0};          // cumulative counters at the last sample
  double bytes{0.0};
  uint64_t packets_first{0};    // ... and at the first sample of the observation
  double bytes_first{0.0};
  size_t samples{0};
  // RTPS_LOST only: the participant that published the sample, i.e. the one that missed
  // the packets (the sender of RTPS_SENT is src itself); empty in older JSON documents
  std::string reporter_participant_prefix{};
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
/// DDS name of the Fast DDS RTPS_LOST statistics topic.
inline constexpr const char * kStatsRtpsLostTopic = "_fastdds_statistics_rtps_lost";

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
  // RTPS_LOST: reporter = the receiving participant (the sample's publisher), src = the
  // remote sending participant, dst = the reporter's own locator that src addressed
  std::vector<TrafficSample> lost;
  std::map<std::string, DataCountSample> resent_datas;     // writer guid -> RESENT_DATAS
  std::map<std::string, DataCountSample> heartbeats;       // writer guid -> HEARTBEAT_COUNT
  std::map<std::string, DataCountSample> gaps;             // writer guid -> GAP_COUNT
  std::map<std::string, DataCountSample> acknacks;         // reader guid -> ACKNACK_COUNT
  std::map<std::string, DataCountSample> nackfrags;        // reader guid -> NACKFRAG_COUNT
  // (participant prefix, statistics topic) discovered
  std::set<std::pair<std::string, std::string>> statistics_writers;
  // prefixes of the participants that published a statistics sample (not those named in one)
  std::set<std::string> participants_with_stats;
  /// IP addresses of the tool's own host. Fast DDS shows the locators of participants on
  /// the same host as 127.0.0.1 / ::1, while a remote writer's RTPS_SENT names the real
  /// address, so both spellings must match.
  std::set<std::string> local_addresses;
  /// Whether the observed nodes' statistics DataWriters keep the default limit of 10
  /// instances (Fast DDS before 3.5), taken from the Fast DDS the tool is built with.
  /// When false, apply_stats never reports stats-writer-instance-limit-suspected.
  bool writer_instance_limit{true};
  size_t samples{0};
  /// Statistics samples that never reached the tool (#134). `samples_lost` counts those the
  /// writers' keep-last history overwrote inside the observation window, `samples_rejected`
  /// those a reader resource limit refused; both mean a pair can show no measurement although
  /// it carries traffic. `samples_lost_at_start` counts the burst a reader sees while the
  /// statistics writers are still matching it (their history had already moved on): it says
  /// nothing about the tool keeping up, so it stays out of the warning. All three are
  /// cumulative over the run, `--watch` included.
  uint64_t samples_lost{0};
  /// The part of samples_lost that HISTORY_LATENCY's reader reported (#141). That reader is
  /// best-effort by design, so it sees every sequence gap; the samples it misses coarsen the
  /// LATENCY of a pair instead of costing its measurement, which is why the warning leaves
  /// them out.
  uint64_t samples_lost_latency{0};
  uint64_t samples_lost_at_start{0};
  uint64_t samples_rejected{0};
  /// Statistics DataWriters the readers could not match because their QoS is incompatible
  /// (#141): their samples never arrive and are never counted as lost either.
  uint64_t writers_incompatible_qos{0};
  /// Pairs in this document whose delivery HISTORY_LATENCY proves (#147), and those of them
  /// RTPS_SENT shows no packet for: the lost measurements. Only a delivery proof tells a
  /// starved RTPS_SENT instance from an idle one - the instance itself looks the same, one
  /// sample from before the run and nothing to subtract it from. Data-sharing pairs (no RTPS
  /// trace by design), pairs that should carry nothing (qos-incompatible, IPC split) and
  /// stats-writer-instance-limit-suspected pairs (the writer side's limit, not the tool's
  /// loss) are in neither number. Set by note_unmeasured_pairs().
  uint64_t pairs_delivered{0};
  uint64_t pairs_delivered_unmeasured{0};
  /// The unmeasured pairs no lost sample explains (#152): no RTPS_SENT instance was ever seen
  /// for the pair (packets_total 0), or no counter sample was lost at all. They raise
  /// rtps-sent-absent; stats-samples-lost counts only the rest.
  uint64_t pairs_delivered_absent{0};
  /// Document-level warning codes, like ShmInfo::warnings: stats-samples-lost, rtps-sent-absent.
  std::vector<std::string> warnings;
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
  size_t datasharing_unmatched{0};   // ... not belonging to a discovered writer or reader
  size_t datasharing_notifications{0};   // fast_datasharing_<reader guid> of discovered readers
  std::vector<uint32_t> checked_ports;   // SHM ports of observed nodes with the tool's host id
  std::vector<uint32_t> missing_ports;   // ... not held here by a living node process
  std::vector<uint32_t> unknown_ports;   // ... of those, the lock is free or could not be probed
  size_t other_host_participants{0};  // observed participants with another host id
  bool nodes_visible{true};          // missing_ports.empty() && other_host_participants == 0
  std::vector<std::string> warnings;  // shm-stale-files, shm-nearly-full, shm-not-visible
  std::map<std::string, uint64_t> datasharing_by_writer;   // writer guid -> history bytes
  std::set<std::string> datasharing_notification_readers;   // reader guids with a segment here
  bool listed{false};                // the directory listing succeeded (internal)
};

// ---- frame-to-frame changes (--watch) ------------------------------------------

struct PairKey
{
  std::string topic;          // display topic name
  std::string writer_guid;
  std::string reader_guid;
  // The nodes behind the GUIDs, carried for the reader of a `changes` object and for
  // `diff --key node`; not part of the identity (a GUID names one endpoint of one node).
  std::string writer_node;
  std::string reader_node;
  PairKey() = default;
  PairKey(
    std::string topic_, std::string writer_guid_, std::string reader_guid_,
    std::string writer_node_ = "", std::string reader_node_ = "")
  : topic(std::move(topic_)), writer_guid(std::move(writer_guid_)),
    reader_guid(std::move(reader_guid_)), writer_node(std::move(writer_node_)),
    reader_node(std::move(reader_node_)) {}
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
  // Locator identities only, never MeasuredLocator: its counters grow every frame and
  // would report every active pair as changed.
  Locator locator;
  std::vector<Locator> measured_locators;
  std::vector<std::string> warnings;
  bool operator==(const PairState & o) const
  {
    return transport == o.transport && confidence == o.confidence && measured == o.measured &&
           locator == o.locator && measured_locators == o.measured_locators &&
           warnings == o.warnings;
  }
  bool operator!=(const PairState & o) const {return !(*this == o);}
};

struct PairChange
{
  PairKey key;          // the pair in the current frame / the after snapshot
  PairState from;
  PairState to;
  // The same pair in the previous frame. Equal to `key` in --watch and with `diff --key
  // guid`; with `--key node` the GUIDs differ when the nodes were restarted in between.
  PairKey before_key;
  PairChange() = default;
  PairChange(PairKey key_, PairState from_, PairState to_)
  : key(key_), from(std::move(from_)), to(std::move(to_)), before_key(std::move(key_)) {}
  PairChange(PairKey key_, PairState from_, PairState to_, PairKey before_key_)
  : key(std::move(key_)), from(std::move(from_)), to(std::move(to_)),
    before_key(std::move(before_key_)) {}
};

/// How two snapshots' pairs are matched (see diff_snapshots()).
enum class KeyMode
{
  Guid,   // (topic, writer GUID, reader GUID): --watch, `diff --key guid`
  Node,   // (topic, writer node, reader node): `diff --key node`, survives node restarts
};

/// Where the previous snapshot of a comparison came from (`diff` only).
struct SnapshotRef
{
  std::string observed_at;
  int domain{0};
};

struct Changes
{
  std::vector<PairKey> added;
  std::vector<PairKey> removed;
  std::vector<PairChange> changed;
  KeyMode key{KeyMode::Guid};
  std::optional<SnapshotRef> before;   // the before document of `transport_viz diff`
  bool empty() const {return added.empty() && removed.empty() && changed.empty();}
};

/// Whether discovery had settled when the observation stopped: the endpoints the nodes
/// announce in `ros_discovery_info` against the endpoints discovery actually delivered
/// (#133). Nothing here is a property of the observed system, only of the observation.
struct DiscoveryStatus
{
  /// true when every announced endpoint was discovered, false when some were missed, unset
  /// when no live participant announced anything (no `ros_discovery_info` sample, or the
  /// reader could not be created): then nothing can be said either way.
  std::optional<bool> complete;
  std::string stopped_on;        // "quiet" (a silent --quiet window) or "timeout"
  size_t events{0};              // discovery callbacks the observation saw
  size_t endpoints{0};           // remote endpoints discovered, before any view filter
  size_t announced_not_discovered{0};   // announced by a live participant, never discovered
  // ... spread over how many participants, which together announced how many endpoints
  // (for the message; not in the JSON)
  size_t participants_incomplete{0};
  size_t announced_by_incomplete{0};
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
  DiscoveryStatus discovery;    // how complete the view behind this snapshot is
  bool has_changes{false};      // true in --watch mode: `changes` is meaningful
  Changes changes;              // relative to the previously rendered frame
};

// ---- small helpers -------------------------------------------------------

std::string to_string(LocatorKind kind);
std::string to_string(Transport transport);
std::string to_string(Confidence confidence);
std::string to_string(DataSharingKind kind);
std::string to_string(KeyMode mode);
std::string host_id_hex(const HostId & id);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__MODEL_HPP_
