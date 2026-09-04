// Copyright 2026 dandelion
// SPDX-License-Identifier: Apache-2.0
//
// Pure decision logic: given a discovered writer and reader, predict which
// transport Fast DDS 2.14 will use for user data between them and explain why
// with machine-readable reason codes.

#ifndef FASTDDS_TRANSPORT_VIZ__DECISION_HPP_
#define FASTDDS_TRANSPORT_VIZ__DECISION_HPP_

#include <string>
#include <vector>

#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

/// Predict the transport for one writer -> reader pair.
Verdict decide(const Endpoint & writer, const Endpoint & reader);

/// Build topic summaries (writer x reader pairs + verdicts) from endpoints.
/// Endpoints are grouped by DDS topic name; writers and readers with
/// different type names are not paired and produce a warning on the topic.
std::vector<TopicSummary> summarize(const std::vector<Endpoint> & endpoints);

/// Overlay statistics-module measurements on the predicted verdicts:
/// fills Pair::measured, upgrades confidence, and adds reason / warning codes
/// (e.g. measured-transport-mismatch). Pure function.
void apply_stats(std::vector<TopicSummary> & topics, const StatsData & stats);

/// Human readable explanation for a reason / warning code (English).
std::string explain(const std::string & code);

/// All known reason / warning codes (for --explain listing and tests).
std::vector<std::string> known_codes();

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__DECISION_HPP_
