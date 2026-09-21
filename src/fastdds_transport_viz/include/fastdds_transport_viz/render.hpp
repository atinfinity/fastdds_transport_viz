// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#ifndef FASTDDS_TRANSPORT_VIZ__RENDER_HPP_
#define FASTDDS_TRANSPORT_VIZ__RENDER_HPP_

#include <map>
#include <string>
#include <vector>

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
  bool locators{false};    // add a "locators:" line under each pair row (implies verbose)
  bool advise{false};      // "fix <code>: ..." lines under each pair row and remedies in the
                           // legend (implies verbose and explain)
  bool compact{false};     // JSON: one line per document (JSON Lines), no indentation
  bool csv_no_header{false};   // CSV: rows only (the frames after the first of --watch --csv)
  bool color{false};       // ANSI colors for transports, warnings and marks
  size_t max_width{0};     // truncate table lines to this many visible columns (0 = never)
  const WatchDecorations * watch{nullptr};   // marks / ghosts / summary (nullptr = plain table)
  /// GUID-prefix host id -> label (hostname / "local"); missing => "host:<hex>"
  std::map<std::string, std::string> host_labels;
};

/// Label used for an endpoint in pair rows ("/node@host(pid)").
std::string endpoint_label(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt);

/// The ghost row of a pair that disappeared, taken from the frame that still had it.
GhostPair ghost_pair(
  const Snapshot & previous, const TopicSummary & topic, const Pair & pair,
  const RenderOptions & opt);

/// "+2 pairs  -1 pair  ~1 changed" for the `changes:` line, or "none".
std::string changes_summary(const Changes & changes);

/// Marks, ghosts and summary for one comparison (`transport_viz diff`): every added and
/// changed pair marked, every removed pair a ghost taken from `before`.
WatchDecorations decorations_for(
  const Changes & changes, const Snapshot & before, const RenderOptions & opt);

/// Frame-to-frame highlight state for --watch. A frame is diff(), render, keep(): the
/// previous frame is kept by move, not rebuilt, since summarizing and applying the statistics
/// a second time cost as much as the rest of the frame on a large graph (#135).
struct WatchState
{
  static constexpr int kHoldFrames = 3;
  std::map<PairKey, PairState> last_rendered;
  Snapshot last_snapshot;
  bool have_previous{false};
  std::map<PairKey, int> mark_ttl;
  std::map<PairKey, int> ghost_ttl;
  WatchDecorations deco;

  /// Set `snap.changes` and the decorations from the previously kept frame.
  void diff(Snapshot & snap, const RenderOptions & ropt);

  /// Keep the rendered frame for the ghost rows of the next one. `snap` is moved from:
  /// TopicSummary points into Snapshot::endpoints, and a moved vector keeps its elements.
  void keep(Snapshot && snap);

private:
  std::map<PairKey, PairState> current_;
};

/// Drop the topics that carry no mark and no ghost (`--changes-only`).
void keep_changed_topics(Snapshot & snap, const WatchDecorations & deco);

/// Visible width of a string, ignoring ANSI escape sequences.
size_t visible_width(const std::string & s);

/// Cut a string to `width` visible columns (appending "…"); escapes are preserved and reset.
std::string truncate_visible(const std::string & s, size_t width);

std::string host_label(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt);
std::string host_label(
  const Snapshot & snap, const HostId & host_id, const std::string & host_name,
  const RenderOptions & opt);

std::string render_table(const Snapshot & snap, const RenderOptions & opt);
std::string render_json(const Snapshot & snap, const RenderOptions & opt);
/// One row per pair (#83): RFC 4180 quoting, LF line ends, an empty cell for a null value.
std::string render_csv(const Snapshot & snap, const RenderOptions & opt);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__RENDER_HPP_
