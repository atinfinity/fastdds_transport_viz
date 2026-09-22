// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Pure decision logic: given a discovered writer and reader, predict which
// transport Fast DDS 2.14 will use for user data between them and explain why
// with machine-readable reason codes.

#ifndef FASTDDS_TRANSPORT_VIZ__DECISION_HPP_
#define FASTDDS_TRANSPORT_VIZ__DECISION_HPP_

#include <map>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "fastdds_transport_viz/model.hpp"
#include "fastdds_transport_viz/ros_names.hpp"

namespace fastdds_transport_viz
{

/// Request/offer policies on which the writer's offer does not satisfy the reader's
/// request, in Fast DDS's matching terms: "reliability", "durability", "deadline",
/// "liveliness", "ownership", "partition". Empty when the pair matches.
std::vector<std::string> qos_incompatibilities(const Endpoint & writer, const Endpoint & reader);

/// Predict the transport for one writer -> reader pair. An incompatible QoS gives NONE
/// with qos-incompatible-<policy> reasons and the warning qos-incompatible; a same-host SHM
/// pair whose participants listen in different IPC namespaces (the same SHM port number, or
/// Endpoint::participant_shm_visibility Visible on one side and NotVisible on the other)
/// gives NONE with the warning shm-ipc-namespace-split.
Verdict decide(const Endpoint & writer, const Endpoint & reader);

/// Whether Fast DDS delivers this pair inside one process (#201), which is what
/// RTPSDomainImpl::should_intraprocess_between() asks: the first 8 bytes of the two GUID
/// prefixes are equal - eProsima's layout is [0-1] vendor id, [2-3] host id, [4-5] the low
/// bytes of the pid, [6-7] a value std::random_device gives each process once, the same in
/// 2.6, 2.14 and 3.x - and the vendor is eProsima, because only its prefixes carry that
/// layout. Intra-process delivery hands the change over inside the process: it beats
/// data-sharing (ReaderLocator::start clears is_datasharing for a local reader) and publishes
/// neither RTPS_SENT nor DATA_COUNT, while HISTORY_LATENCY and PUBLICATION_THROUGHPUT still
/// flow. A node started with a custom <prefix>, or a Fast DDS built with intra-process
/// delivery off, makes this false where it holds - the cases where the tool keeps its older,
/// stricter behaviour. Pure function.
bool intra_process_pair(const Endpoint & writer, const Endpoint & reader);

/// How many pairs of the snapshot could ever show a measured packet (#201): writer and reader
/// on the same DDS topic, in two different processes, neither of them the tool's own
/// (`own_prefixes`, as for reader_destinations). QoS and type compatibility are deliberately
/// not checked - the count decides whether a --stats one-shot waits for RTPS_SENT at all, so
/// it over-approximates and any doubt keeps the strict rule. Zero means every delivery in the
/// system is intra-process (or there is nothing to deliver), and no RTPS_SENT will ever
/// arrive. Pure function; published as StatsData::measurable_pairs.
size_t count_measurable_pairs(
  const std::vector<Endpoint> & endpoints, const std::set<std::string> & own_prefixes);

/// Suffix of the companion topic rmw_fastrtps_cpp (Lyrical and later) creates for a type
/// with an unbounded uint8[] field: the samples go there whenever every subscription of
/// the topic supports native buffers, and the parent topic stays silent.
inline constexpr const char * kBufferCompanionSuffix = "/_buf_cpu";

/// Link every endpoint on <topic>/_buf_cpu to the writer (reader) of <topic> it belongs to:
/// same participant, same kind, same type. Several candidates are told apart by the entity
/// key, which the rmw allocates right after the parent's; a companion that stays ambiguous
/// or has no parent is left unlinked. Sets Endpoint::buffer_parent_guid and
/// Endpoint::buffer_companion_guids; call it on every discovered endpoint, before filtering.
void link_buffer_companions(std::vector<Endpoint> & endpoints);

/// Build topic summaries (writer x reader pairs + verdicts) from endpoints.
/// Endpoints are grouped by DDS topic name; writers and readers with
/// different type names are not paired and produce a warning on the topic.
/// Pairs (or, without pairs, topics) of linked endpoints get buffer-companion-folded
/// (parent) or buffer-companion (companion), unlinked companions buffer-companion-unmatched.
std::vector<TopicSummary> summarize(const std::vector<Endpoint> & endpoints);

/// Fills TopicSummary::kind / group / direction (#84). summarize() calls it; parse_json()
/// calls it for a document written before the keys existed, so a saved snapshot groups too.
///
/// A topic is an action member only when every member found under that name carries the DDS
/// type it has to carry and at least one of them names the action: "/_action/" is not
/// reserved, so a plain service can wear an action's exact DDS names.
void classify_topics(std::vector<TopicSummary> & topics);

/// One line of the table (#84): either one plain topic, or one service/action group between
/// a client participant and a server participant.
///
/// The rule is total -- every endpoint of a service or action appears in exactly one row.
/// Endpoints that are in no pair (an offered but never called service is the common case: a
/// node's seven parameter services are 82 % of its rows under --all) get a half-open row for
/// their own participant, so nothing disappears just because the other side is not running.
struct DisplayRow
{
  TopicKind kind{TopicKind::Topic};
  std::string name;        // the plain topic's display name, or the group's ROS name
  std::string type;        // display type; the service or action type for a group
  const TopicSummary * topic{nullptr};    // non-null only for a plain (ungrouped) row
  const Endpoint * client{nullptr};       // a representative endpoint of each side, null
  const Endpoint * server{nullptr};       // when that side was not discovered
  std::string requester;   // client participant GUID prefix, "" when absent
  std::string replier;     // server participant GUID prefix, "" when absent
  // Members split by direction of travel, not by request and reply: an action's three
  // requests go one way, its three replies plus feedback and status the other.
  std::vector<std::pair<const TopicSummary *, const Pair *>> to_server;
  std::vector<std::pair<const TopicSummary *, const Pair *>> to_client;
  std::vector<const TopicSummary *> members;           // every member topic of this row
  std::vector<const TopicSummary *> unpaired_members;  // ... those that contributed no pair
  std::vector<const Endpoint *> unpaired_endpoints;
};

/// Groups `topics` into table rows. Plain topics pass through one for one.
std::vector<DisplayRow> display_rows(const std::vector<TopicSummary> & topics);

/// The member's short name inside its group, for the per-member lines under -v:
/// "request" / "reply" for a service, the rcl_action suffix for an action.
std::string member_label(const TopicSummary & topic);

/// Whether the default view (without --all) shows the topic: a ROS topic ("rt/" prefix)
/// that is not a companion topic whose every endpoint is folded into its parent.
bool in_default_view(const TopicSummary & topic);

/// --node filter, applied after summarize(): keeps the pairs whose writer or reader
/// belongs to a node accepted by `node_matches`, every endpoint of such nodes (paired or
/// not) and the partner endpoints of the kept pairs. Topics left without endpoints are
/// dropped; the no-matching-* reasons are recomputed for topics left without pairs.
void filter_by_node(
  std::vector<TopicSummary> & topics,
  const std::function<bool(const Endpoint &)> & node_matches);

/// Overlay statistics-module measurements on the predicted verdicts:
/// fills Pair::measured, upgrades confidence, and adds reason / warning codes
/// (e.g. measured-transport-mismatch). The per-entity counters of a writer or reader
/// include those of its native-buffer companions (Endpoint::buffer_companion_guids).
/// Pure function.
void apply_stats(std::vector<TopicSummary> & topics, const StatsData & stats);

/// How complete the observation was (#133): every endpoint gid a live participant announced
/// in `ros_discovery_info` must have been discovered. `announced` comes from
/// NodeNameTable::announced_by_participant(), `live_participants` from
/// DiscoveryObserver::live_participants() (a participant that left keeps its row in the
/// table, and its endpoints are gone for good), and `discovered` must be the raw discovery
/// snapshot, before --all / --topic / --node drop anything. The comparison is one-directional:
/// an endpoint that discovery delivered but no sample announced (a non-ROS DDS writer, a node
/// whose sample has not arrived) proves nothing. DiscoveryStatus::complete stays unset when no
/// live participant announced anything. Pure function.
DiscoveryStatus discovery_completeness(
  const std::map<ParticipantPrefix, std::vector<EndpointGid>> & announced,
  const std::set<ParticipantPrefix> & live_participants,
  const std::vector<Endpoint> & discovered);

/// The servers a ROS_DISCOVERY_SERVER value names (#86), with Fast DDS's rules: entries
/// separated by `;` (an empty one is a placeholder), `ip`, `ip:port`, `[ipv6]:port`,
/// `UDPv4:[host]:port`, `TCPv4:[host]:port`, `host:port`; the port defaults to 11811 and a
/// number below 420 is a domain id (port 7400 + 250 * domain + 2, Fast DDS 3.x). A host
/// name stays as the address text; the caller resolves it. Entries Fast DDS would reject
/// are skipped. Pure function.
std::vector<Locator> parse_discovery_server_env(const std::string & value);

/// Which server serves each participant, when that can be told (#86): with exactly one
/// SERVER discovered every CLIENT / SUPER_CLIENT is its client; under Easy Mode
/// (`easy_mode` non-empty) a client's server is the DiscoveryServerAuto on its own host.
/// Anything else leaves `discovery_server` unset. Servers, simple participants and the
/// tool's own never get one. Pure function.
void attribute_discovery_servers(std::vector<Participant> & participants, bool easy_mode);

/// The one stderr line an incomplete observation earns (#133), or "" when the view was
/// complete or could not be judged. `quiet`, `timeout` and `stats` are the settings this run
/// used: the advice is always longer than they are, and never below what a large system
/// needed in docs/verification-log.md "Scale results". With --stats only --timeout is advised,
/// because the quiet window is disabled there. Pure function.
std::string incomplete_discovery_warning(
  const DiscoveryStatus & status, double quiet, double timeout, bool stats);

/// How long after the last statistics writer matched a reader the samples it reports lost are
/// still counted as the late-join burst rather than as the tool falling behind (#134). Five
/// seconds, because the burst is announced late: a writer says what its history dropped with
/// one of its next heartbeats, not at the match. Measured on a quiet five-node system, the
/// notifications arrived up to 1.2 s (Jazzy) and 3.9 s (Lyrical) after the match.
inline constexpr double kStatisticsLateJoinGraceSeconds = 5.0;

/// Whether a statistics sample reported lost now still belongs to a late-join burst (#134):
/// a reader that has just matched a writer is told about everything the writer's keep-last
/// history dropped before the match, which says nothing about the tool keeping up. Every new
/// writer brings such a burst, so the window follows the matches: it is open for
/// kStatisticsLateJoinGraceSeconds after each one, and before the first match of all
/// (`any_writer_matched`), which the grace period has nothing to measure from. A node joining
/// mid-run therefore excuses five seconds of loss, never the losses that keep coming after it.
/// Pure function.
bool statistics_late_join_window_open(
  bool any_writer_matched, double seconds_since_last_writer_match);

/// The least a --stats one-shot observes (#168): the counters are reported as last - first, so
/// every instance needs a second sample after the transient-local one, and small systems
/// took this long before the settle rule existed.
inline constexpr double kStatsSettleMinSeconds = 5.0;
/// The least a --stats one-shot waits for the measured RTPS_SENT instances to stop growing
/// before it ends (#168): the transient-local handoffs at 20 processes pause for up to about
/// 2 s between one participant's and the next's, which a 1 s --quiet would take for the end.
inline constexpr double kStatsMeasuredQuietSeconds = 3.0;

/// Whether a --stats one-shot has seen what it waits for (#168): discovery is quiet, at least
/// kStatsSettleMinSeconds have passed, every RTPS_SENT writer the reader matched has been
/// heard from once, and the number of RTPS_SENT instances that measure a pair (see
/// measures_a_pair, #179) has not grown for `quiet_window_seconds` (`measured_quiet_seconds`
/// is how long it has stood still). The
/// transient-local handoff of the counter writers is what a fixed window cut short: at 20
/// processes with 100 pairs each the last writer is first heard from at 16-20 s and measured
/// instances keep coming until about 25 s, with pauses of up to 2 s in between. What the
/// handoffs produce is measured instances, so the run ends when those stop coming, the way
/// discovery ends when events do. Nothing matched means nothing to wait for. Pure function.
/// `measured_instances` must be non-zero once anything was announced: on Fast DDS 3.6 every
/// writer's first sample can be in before a single entry has its second, and "zero for 3 s"
/// is not quiet, it is nothing measured yet. Unless nothing can be measured at all
/// (`measurable_pairs`, #201): an intra-process-only system publishes no RTPS_SENT by
/// construction, and waiting for one burns --timeout on every run. A publisher-only system
/// is the same case, for the same reason - there is no reader to send to.
bool stats_settled(
  bool discovery_quiet, double elapsed_seconds, size_t writers_announced, size_t writers_heard,
  size_t measurable_pairs, size_t measured_instances, double measured_quiet_seconds,
  double quiet_window_seconds);

/// Where the packets of a pair arrive: every locator a discovered reader outside the tool's
/// own participants receives on (#179, #196). Unicast is keyed (kind, port) - the port is
/// that one reader's - while a multicast group is keyed whole, because the group IS the
/// address and the port alone is shared with every other group in the domain. Metatraffic
/// needs no filter here: 239.255.0.1:7400 is a participant locator, and only the unicast
/// half of those ever reaches a snapshot, so no endpoint announces it. Pure function.
struct ReaderDestinations
{
  std::set<std::pair<LocatorKind, uint32_t>> unicast_ports;
  std::set<std::tuple<LocatorKind, std::string, uint32_t>> multicast_locators;
  bool empty() const {return unicast_ports.empty() && multicast_locators.empty();}
};
ReaderDestinations reader_destinations(
  const std::vector<Endpoint> & endpoints, const std::set<std::string> & own_prefixes);

/// Whether an RTPS_SENT instance is a measured pair packet (#179, #196): its counter moved
/// since its first sample AND its destination is one of the reader destinations above. The
/// settle rule of a --stats one-shot counts these; the metatraffic and own-port instances
/// that move first (the Lyrical medium run settled at 8 s with 47 of them and no pair
/// measured) do not. A multicast group counts for the run although one such instance serves
/// every reader of the group: the rule asks whether measurement is still arriving, not how
/// many pairs it covers, and a pair that receives on a group only (#130) is measured through
/// this instance and through no other. Pure function.
bool measures_a_pair(const TrafficSample & traffic, const ReaderDestinations & readers);

/// Whether --watch may draw its first frame (#177): discovery is quiet and, with --stats, at
/// least kStatsSettleMinSeconds have passed so the first frame has a counter window. The
/// settle rule is for one-shot output: under --watch the frames keep coming and every
/// second spent waiting is a frame the run does not measure (the harness got 16-22 frames out
/// of 60 s, and the p95 of that few is the worst frame). --timeout still caps the wait, in the
/// caller. Pure function.
bool watch_ready(bool discovery_quiet, double elapsed_seconds, bool stats);

/// Whether losing statistics samples cost a measurement (#134, #147): counter samples were lost
/// or rejected inside the observation window AND a pair with a delivery proof shows no measured
/// packet (StatsData::pairs_delivered_unmeasured). Loss alone is harmless - the counters are
/// cumulative and the tool prints `last - first` - and an unmeasured pair without any loss is
/// not the tool falling behind. The late-join burst (StatsData::samples_lost_at_start) and the
/// HISTORY_LATENCY gaps never count. Pure function.
bool statistics_samples_were_lost(const StatsData & stats);

/// The one stderr line a run earns whose unmeasured pairs no lost sample explains (#152), or
/// "" when StatsData::pairs_delivered_absent is 0. Pure function.
std::string rtps_sent_absent_warning(const StatsData & stats);

/// Also counts pairs_delivered_absent and raises rtps-sent-absent when it is above 0 (#152).
/// Counts StatsData::pairs_delivered / pairs_delivered_unmeasured over `topics` (after
/// apply_stats) and raises the document-level stats-samples-lost when
/// statistics_samples_were_lost() then holds (#147). Replaces an earlier verdict, so it can be
/// called on every --watch frame.
void note_unmeasured_pairs(const std::vector<TopicSummary> & topics, StatsData & stats);

/// The losses that cost a measurement: samples_lost without the best-effort HISTORY_LATENCY
/// part (#141). Saturates at 0, because a document written before #141 carries no
/// samples_lost_latency and every document may be edited by hand. Pure function.
uint64_t statistics_counter_samples_lost(const StatsData & stats);

/// The one stderr line a run whose losses cost a measurement earns (#134, #147), or "" when
/// statistics_samples_were_lost() is false. Names the loss against everything the readers
/// should have had (received + lost) and the pairs left without a measurement. A
/// longer --timeout is deliberately not advised: it does not lower the loss rate, it only
/// collects more of it. Pure function.
std::string statistics_loss_warning(const StatsData & stats);

/// Key and highlight-relevant state of a pair.
PairKey pair_key(const TopicSummary & topic, const Pair & pair);
PairState pair_state(const Pair & pair);

/// All pairs of a snapshot keyed for comparison.
std::map<PairKey, PairState> pair_states(const Snapshot & snap);

/// Pairs added / removed / changed between two frames. Pure function.
Changes diff(
  const std::map<PairKey, PairState> & previous,
  const std::map<PairKey, PairState> & current);

/// Compare two snapshots (`transport_viz diff`). KeyMode::Guid is pair_states() + diff().
/// KeyMode::Node matches the pairs by (topic, writer node, reader node) instead, so the
/// comparison survives a restart of the nodes; a node with several writers (or readers)
/// on one topic gets them matched in GUID order, and an endpoint without a node name is
/// matched by its GUID; the port numbers of the selected and measured locators are ignored
/// (a restart renumbers them), their kinds and addresses are not. The keys of `added` and
/// `changed` are the after snapshot's, those of `removed` the before snapshot's;
/// PairChange::before_key carries the before identity. Pure function; `Changes::before` is
/// left for the caller.
Changes diff_snapshots(const Snapshot & before, const Snapshot & after, KeyMode mode);

/// Human readable explanation for a reason / warning code (English): what happened.
std::string explain(const std::string & code);

/// What to change to get past a reason / warning code (English, one sentence naming the
/// environment variable, XML element or QoS policy). Empty for codes that describe a normal
/// state or ask for a bug report, and for unknown codes. Shown by --advise, --list-codes,
/// the JSON `reason_code_remedies` object and the web viewer.
std::optional<std::string> remedy(const std::string & code);

/// All known reason / warning codes (for --explain listing and tests).
std::vector<std::string> known_codes();

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__DECISION_HPP_
