// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#ifndef FASTDDS_TRANSPORT_VIZ__RENDER_HPP_
#define FASTDDS_TRANSPORT_VIZ__RENDER_HPP_

#include <map>
#include <string>

#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

struct RenderOptions
{
  bool verbose{false};     // expand writer->reader pairs under each topic
  bool explain{false};     // append a legend for every reason code used
  bool compact{false};     // JSON: one line per document (JSON Lines), no indentation
  /// GUID-prefix host id -> label (hostname / "local"); missing => "host:<hex>"
  std::map<std::string, std::string> host_labels;
};

std::string host_label(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt);

std::string render_table(const Snapshot & snap, const RenderOptions & opt);
std::string render_json(const Snapshot & snap, const RenderOptions & opt);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__RENDER_HPP_
