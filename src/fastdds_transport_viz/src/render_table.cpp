// Copyright 2026 dandelion
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/render.hpp"

namespace fastdds_transport_viz
{

std::string host_label(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt)
{
  if (!e.host_name.empty()) {
    // PHYSICAL_DATA reports "<hostname>:<numeric host id>"; show the hostname.
    auto colon = e.host_name.find(':');
    return colon == std::string::npos ? e.host_name : e.host_name.substr(0, colon);
  }
  auto it = opt.host_labels.find(host_id_hex(e.host_id));
  if (it != opt.host_labels.end()) {
    return it->second;
  }
  if (e.host_id == snap.local_host_id) {
    return "local";
  }
  return "host:" + host_id_hex(e.host_id);
}

namespace
{

std::string join(const std::vector<std::string> & v, const std::string & sep)
{
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i) {out += sep;}
    out += v[i];
  }
  return out;
}

std::string transport_label(const Verdict & v)
{
  std::string s = to_string(v.transport);
  if (v.confidence == Confidence::Likely) {
    s += "?";
  }
  return s;
}

std::string aggregate_transports(const TopicSummary & t)
{
  if (t.pairs.empty()) {
    return "-";
  }
  std::map<std::string, int> counts;
  std::vector<std::string> order;
  for (const auto & p : t.pairs) {
    auto label = transport_label(p.verdict);
    if (!counts.count(label)) {order.push_back(label);}
    counts[label]++;
  }
  std::vector<std::string> parts;
  for (const auto & label : order) {
    parts.push_back(label + " x" + std::to_string(counts[label]));
  }
  return join(parts, ", ");
}

std::string aggregate_reasons(const TopicSummary & t)
{
  std::vector<std::string> out;
  std::set<std::string> seen;
  auto add = [&](const std::string & c) {
      if (seen.insert(c).second) {out.push_back(c);}
    };
  for (const auto & r : t.unmatched_reasons) {add(r);}
  for (const auto & p : t.pairs) {
    for (const auto & r : p.verdict.reasons) {add(r);}
    for (const auto & w : p.verdict.warnings) {add("!" + w);}
  }
  return join(out, ",");
}

std::string endpoint_label(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt)
{
  std::string who = e.node_name.empty() ? ("guid:" + e.guid.substr(0, 11) + "..") : e.node_name;
  std::string label = who + "@" + host_label(snap, e, opt);
  if (!e.process.empty()) {
    label += "(" + e.process + ")";
  }
  return label;
}

std::string measured_label(const Pair & p)
{
  if (!p.measured.available) {
    return "n/a";
  }
  if (p.measured.transports.empty()) {
    return p.measured.delivered ? "none(delivered)" : "none";
  }
  std::vector<std::string> parts;
  for (auto t : p.measured.transports) {
    parts.push_back(to_string(t));
  }
  std::string s = join(parts, "+");
  s += " " + std::to_string(p.measured.packets) + "pkt";
  return s;
}

void print_rows(std::ostringstream & os, const std::vector<std::vector<std::string>> & rows)
{
  if (rows.empty()) {return;}
  std::vector<size_t> widths(rows[0].size(), 0);
  for (const auto & r : rows) {
    for (size_t i = 0; i < r.size(); ++i) {
      widths[i] = std::max(widths[i], r[i].size());
    }
  }
  for (const auto & r : rows) {
    for (size_t i = 0; i < r.size(); ++i) {
      os << r[i];
      if (i + 1 < r.size()) {
        os << std::string(widths[i] - r[i].size() + 2, ' ');
      }
    }
    os << '\n';
  }
}

}  // namespace

std::string render_table(const Snapshot & snap, const RenderOptions & opt)
{
  std::ostringstream os;
  std::vector<std::vector<std::string>> rows;
  rows.push_back({"TOPIC", "TYPE", "PUBS", "SUBS", "TRANSPORT", "REASON"});
  for (const auto & t : snap.topics) {
    rows.push_back({
        t.display_topic, t.display_type,
        std::to_string(t.writers.size()), std::to_string(t.readers.size()),
        aggregate_transports(t), aggregate_reasons(t)});
  }
  if (!opt.verbose) {
    print_rows(os, rows);
  } else {
    // Print each topic row followed by its indented pair rows. Widths are
    // computed over the topic rows only so the table header stays aligned.
    std::vector<std::vector<std::string>> header_only = {rows[0]};
    std::vector<size_t> widths(rows[0].size(), 0);
    for (const auto & r : rows) {
      for (size_t i = 0; i < r.size(); ++i) {widths[i] = std::max(widths[i], r[i].size());}
    }
    auto emit = [&](const std::vector<std::string> & r) {
        for (size_t i = 0; i < r.size(); ++i) {
          os << r[i];
          if (i + 1 < r.size()) {os << std::string(widths[i] - r[i].size() + 2, ' ');}
        }
        os << '\n';
      };
    emit(rows[0]);
    size_t idx = 1;
    for (const auto & t : snap.topics) {
      emit(rows[idx++]);
      std::vector<std::vector<std::string>> pair_rows;
      for (const auto & p : t.pairs) {
        std::string reasons = join(p.verdict.reasons, ",");
        for (const auto & w : p.verdict.warnings) {reasons += ",!" + w;}
        std::vector<std::string> row = {
          "    " + endpoint_label(snap, *p.writer, opt) + " -> " +
          endpoint_label(snap, *p.reader, opt),
          transport_label(p.verdict)};
        if (snap.stats.enabled) {
          row.push_back("measured=" + measured_label(p));
        }
        row.push_back(reasons);
        pair_rows.push_back(row);
      }
      print_rows(os, pair_rows);
    }
  }

  if (snap.topics.empty()) {
    os << "(no endpoints discovered in domain " << snap.domain << ")\n";
  }
  if (snap.stats.enabled) {
    os << "\nstatistics: " << snap.stats.samples << " samples from "
       << snap.stats.participants_with_stats.size() << " participant(s)";
    if (snap.stats.participants_with_stats.empty()) {
      os << " - start the observed nodes with FASTDDS_STATISTICS=\""
         << "RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC\"";
    }
    os << "\n";
  }

  if (opt.explain) {
    std::set<std::string> used;
    for (const auto & t : snap.topics) {
      for (const auto & r : t.unmatched_reasons) {used.insert(r);}
      for (const auto & p : t.pairs) {
        for (const auto & r : p.verdict.reasons) {used.insert(r);}
        for (const auto & w : p.verdict.warnings) {used.insert(w);}
      }
    }
    if (!used.empty()) {
      os << "\nReason codes:\n";
      for (const auto & code : used) {
        os << "  " << code << "\n      " << explain(code) << "\n";
      }
    }
    os << "\nLegend: '?' after a transport = confidence 'likely' (see reason codes);"
       << " '!' prefix = warning.\n";
  }
  return os.str();
}

}  // namespace fastdds_transport_viz
