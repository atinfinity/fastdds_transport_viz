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
#include <string>
#include <vector>

#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

/// Request/offer policies on which the writer's offer does not satisfy the reader's
/// request, in Fast DDS's matching terms: "reliability", "durability", "deadline",
/// "liveliness", "ownership", "partition". Empty when the pair matches.
std::vector<std::string> qos_incompatibilities(const Endpoint & writer, const Endpoint & reader);

/// Predict the transport for one writer -> reader pair. An incompatible QoS gives NONE
/// with qos-incompatible-<policy> reasons and the warning qos-incompatible.
Verdict decide(const Endpoint & writer, const Endpoint & reader);

/// Build topic summaries (writer x reader pairs + verdicts) from endpoints.
/// Endpoints are grouped by DDS topic name; writers and readers with
/// different type names are not paired and produce a warning on the topic.
std::vector<TopicSummary> summarize(const std::vector<Endpoint> & endpoints);

/// --node filter, applied after summarize(): keeps the pairs whose writer or reader
/// belongs to a node accepted by `node_matches`, every endpoint of such nodes (paired or
/// not) and the partner endpoints of the kept pairs. Topics left without endpoints are
/// dropped; the no-matching-* reasons are recomputed for topics left without pairs.
void filter_by_node(
  std::vector<TopicSummary> & topics,
  const std::function<bool(const Endpoint &)> & node_matches);

/// Overlay statistics-module measurements on the predicted verdicts:
/// fills Pair::measured, upgrades confidence, and adds reason / warning codes
/// (e.g. measured-transport-mismatch). Pure function.
void apply_stats(std::vector<TopicSummary> & topics, const StatsData & stats);

/// Key and highlight-relevant state of a pair.
PairKey pair_key(const TopicSummary & topic, const Pair & pair);
PairState pair_state(const Pair & pair);

/// All pairs of a snapshot keyed for comparison.
std::map<PairKey, PairState> pair_states(const Snapshot & snap);

/// Pairs added / removed / changed between two frames. Pure function.
Changes diff(const std::map<PairKey, PairState> & previous, const std::map<PairKey, PairState> & current);

/// Human readable explanation for a reason / warning code (English).
std::string explain(const std::string & code);

/// All known reason / warning codes (for --explain listing and tests).
std::vector<std::string> known_codes();

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__DECISION_HPP_
