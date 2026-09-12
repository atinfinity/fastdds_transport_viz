// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// The inverse of render_json(): a Snapshot from a saved --json document, for
// `transport_viz diff` (no DDS participant involved).

#ifndef FASTDDS_TRANSPORT_VIZ__PARSE_JSON_HPP_
#define FASTDDS_TRANSPORT_VIZ__PARSE_JSON_HPP_

#include <stdexcept>
#include <string>

#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

/// A document that is not a transport_viz --json document (bad JSON, another
/// schema_version, a missing or mistyped key).
class ParseError : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

/// Rebuild a Snapshot from a document written by render_json(): a single (pretty or
/// compact) document, or a JSON Lines stream as written by `--watch --json`, of which the
/// last document is used (`documents` receives how many the text held). Everything the
/// table and JSON renderers show is restored: endpoints, pairs with their verdicts and
/// measurements, the statistics summary and the shared-memory report; a `changes` object
/// in the input is ignored. Throws ParseError.
Snapshot parse_json(const std::string & text, size_t * documents = nullptr);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__PARSE_JSON_HPP_
