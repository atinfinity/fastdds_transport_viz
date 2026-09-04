// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#ifndef FASTDDS_TRANSPORT_VIZ__RENDER_HPP_
#define FASTDDS_TRANSPORT_VIZ__RENDER_HPP_

#include <map>
#include <string>

#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

/// A pair that disappeared but is still shown (dimmed, '-') for a few frames.
struct GhostPair
{
  PairKey key;
  std::string type;
  std::string writer_label;
  std::string reader_label;
  std::string transport_label;
};

/// Frame-to-frame decorations for --watch (owned by the watch loop).
struct WatchDecorations
{
  std::map<PairKey, char> marks;    // '+' added, '~' changed (persist a few frames)
  std::vector<GhostPair> ghosts;    // removed pairs still displayed with '-'
  std::string summary;              // e.g. "+2 pairs  -1 pair  ~1 changed"
};

struct RenderOptions
{
  bool verbose{false};     // expand writer->reader pairs under each topic
  bool explain{false};     // append a legend for every reason code used
  bool compact{false};     // JSON: one line per document (JSON Lines), no indentation
  bool color{false};       // ANSI colors for transports, warnings and marks
  size_t max_width{0};     // truncate table lines to this many visible columns (0 = never)
  const WatchDecorations * watch{nullptr};   // marks / ghosts / summary (nullptr = plain table)
  /// GUID-prefix host id -> label (hostname / "local"); missing => "host:<hex>"
  std::map<std::string, std::string> host_labels;
};

/// Label used for an endpoint in pair rows ("/node@host(pid)").
std::string endpoint_label(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt);

/// Visible width of a string, ignoring ANSI escape sequences.
size_t visible_width(const std::string & s);

/// Cut a string to `width` visible columns (appending "…"); escapes are preserved and reset.
std::string truncate_visible(const std::string & s, size_t width);

std::string host_label(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt);

std::string render_table(const Snapshot & snap, const RenderOptions & opt);
std::string render_json(const Snapshot & snap, const RenderOptions & opt);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__RENDER_HPP_
