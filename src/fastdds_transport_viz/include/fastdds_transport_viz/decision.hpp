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
