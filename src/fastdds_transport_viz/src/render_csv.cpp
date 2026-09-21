// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/render.hpp"

namespace fastdds_transport_viz
{

namespace
{
/// RFC 4180 quoting: a field with a comma, a double quote or a line break is enclosed in
/// double quotes, and its double quotes are doubled. Everything else is written as is.
std::string field(const std::string & s)
{
  if (s.find_first_of(",\"\r\n") == std::string::npos) {
    return s;
  }
  std::string out = "\"";
  for (char c : s) {
    if (c == '"') {
      out += '"';
    }
    out += c;
  }
  return out + "\"";
}

/// Numbers are spelled as the JSON document spells them (shortest round-trip form).
template<typename T>
std::string number(T v)
{
  return nlohmann::json(v).dump();
}

std::string joined(const std::vector<std::string> & items)
{
  std::string out;
  for (const auto & s : items) {
    out += (out.empty() ? "" : ";") + s;
  }
  return out;
}

const std::vector<std::string> & columns()
{
  static const std::vector<std::string> names{
    "observed_at", "domain", "topic", "type",
    "writer_node", "writer_host", "writer_guid",
    "reader_node", "reader_host", "reader_guid",
    "transport", "confidence", "reasons", "warnings", "measured_transports",
    "packets", "bytes", "delivered_per_s", "delivered_per_s_lower_bound",
    "latency_mean_s", "latency_min_s", "latency_max_s", "latency_last_s",
    "lost_packets", "resent_datas"};
  return names;
}

void row(std::string & out, const std::vector<std::string> & cells)
{
  for (size_t i = 0; i < cells.size(); ++i) {
    out += (i ? "," : "") + field(cells[i]);
  }
  out += '\n';
}
}  // namespace

std::string render_csv(const Snapshot & snap, const RenderOptions & opt)
{
  std::string out;
  if (!opt.csv_no_header) {
    row(out, columns());   // also when there is no pair: scripts can rely on the names
  }
  for (const auto & t : snap.topics) {
    for (const auto & p : t.pairs) {
      const auto & m = p.measured;
      std::vector<std::string> transports;
      for (auto k : m.transports) {
        transports.push_back(to_string(k));
      }
      // an empty cell is the JSON document's null: nothing was measured for the value
      const bool latency = m.latency_available;
      const bool lost = m.reliability.available && m.reliability.lost_available;
      row(
        out, {
          snap.observed_at, number(snap.domain), t.display_topic, t.display_type,
          p.writer->node_name, host_label(snap, *p.writer, opt), p.writer->guid,
          p.reader->node_name, host_label(snap, *p.reader, opt), p.reader->guid,
          to_string(p.verdict.transport), to_string(p.verdict.confidence),
          joined(p.verdict.reasons), joined(p.verdict.warnings), joined(transports),
          m.available ? number(m.packets) : "", m.available ? number(m.bytes) : "",
          m.rate_available ? number(m.delivered_per_s) : "",
          m.rate_available ? (m.rate_lower_bound ? "true" : "false") : "",
          latency ? number(m.latency.mean()) : "", latency ? number(m.latency.min) : "",
          latency ? number(m.latency.max) : "", latency ? number(m.latency.last) : "",
          lost ? number(m.reliability.lost_packets) : "",
          m.reliability.available ? number(m.reliability.resent) : ""});
    }
  }
  return out;
}

}  // namespace fastdds_transport_viz
