// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdio>
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

size_t visible_width(const std::string & s)
{
  size_t n = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\033') {                 // skip CSI sequence "\033[...m"
      while (i < s.size() && s[i] != 'm') {++i;}
      continue;
    }
    if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {++n;}   // count UTF-8 lead bytes
  }
  return n;
}

std::string truncate_visible(const std::string & s, size_t width)
{
  if (width == 0 || visible_width(s) <= width) {return s;}
  std::string out;
  size_t n = 0;
  bool in_escape = false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\033') {in_escape = true;}
    if (in_escape) {
      out += s[i];
      if (s[i] == 'm') {in_escape = false;}
      continue;
    }
    bool lead = (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80;
    if (lead && n + 1 >= width) {break;}
    if (lead) {++n;}
    out += s[i];
  }
  return out + "\u2026\033[0m";
}

namespace
{

const char * ansi_for(Transport t)
{
  switch (t) {
    case Transport::UDPv4: return "\033[34m";
    case Transport::UDPv6: return "\033[36m";
    case Transport::TCPv4:
    case Transport::TCPv6: return "\033[35m";
    case Transport::SHM: return "\033[32m";
    case Transport::DataSharing: return "\033[33m";
    default: return "\033[2m";
  }
}

std::string paint(const std::string & text, const char * code, bool enabled)
{
  return enabled ? std::string(code) + text + "\033[0m" : text;
}

const char * const RED = "\033[31m";
const char * const GREEN = "\033[32m";
const char * const YELLOW = "\033[33m";
const char * const DIM = "\033[2m";
const char * const BOLD = "\033[1m";

std::string join(const std::vector<std::string> & v, const std::string & sep)
{
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i) {out += sep;}
    out += v[i];
  }
  return out;
}

std::string transport_label(const Verdict & v, bool color = false)
{
  std::string s = to_string(v.transport);
  if (v.confidence == Confidence::Likely) {
    s += "?";
  }
  return paint(s, ansi_for(v.transport), color);
}

std::string aggregate_transports(const TopicSummary & t, bool color)
{
  if (t.pairs.empty()) {
    return "-";
  }
  std::map<std::string, int> counts;
  std::vector<std::string> order;
  std::map<std::string, std::string> painted;
  for (const auto & p : t.pairs) {
    auto label = transport_label(p.verdict);
    if (!counts.count(label)) {order.push_back(label); painted[label] = transport_label(p.verdict, color);}
    counts[label]++;
  }
  std::vector<std::string> parts;
  for (const auto & label : order) {
    parts.push_back(painted[label] + " x" + std::to_string(counts[label]));
  }
  return join(parts, ", ");
}

std::string aggregate_reasons(const TopicSummary & t, bool color)
{
  std::vector<std::string> out;
  std::set<std::string> seen;
  auto add = [&](const std::string & c) {
      if (seen.insert(c).second) {out.push_back(c);}
    };
  for (const auto & r : t.unmatched_reasons) {add(r);}
  for (const auto & p : t.pairs) {
    for (const auto & r : p.verdict.reasons) {add(r);}
    for (const auto & w : p.verdict.warnings) {add(paint("!" + w, RED, color));}
  }
  return join(out, ",");
}

}  // namespace

std::string endpoint_label(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt)
{
  std::string who = e.node_name.empty() ? ("guid:" + e.guid.substr(0, 11) + "..") : e.node_name;
  std::string label = who + "@" + host_label(snap, e, opt);
  if (!e.process.empty()) {
    label += "(" + e.process + ")";
  }
  return label;
}

namespace
{

/// 1234 -> "1.23 kB" (SI, 3 significant digits); suffix e.g. "B" or "B/s".
std::string human_bytes(double v, const char * unit)
{
  const char * prefixes[] = {"", "k", "M", "G", "T"};
  int i = 0;
  while (v >= 1000.0 && i < 4) {v /= 1000.0; ++i;}
  char buf[32];
  if (i == 0) {
    std::snprintf(buf, sizeof(buf), "%.0f %s%s", v, prefixes[i], unit);
  } else {
    std::snprintf(buf, sizeof(buf), v < 10.0 ? "%.2f %s%s" : v < 100.0 ? "%.1f %s%s" : "%.0f %s%s", v, prefixes[i], unit);
  }
  return buf;
}

std::string rate_label(bool available, double bytes_per_s)
{
  return available ? human_bytes(bytes_per_s, "B/s") : "-";
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
  if (p.measured.packets == 0) {
    return s + " (idle)";   // packets before the observation, none during it
  }
  s += " " + std::to_string(p.measured.packets) + "pkt " + human_bytes(p.measured.bytes, "B");
  return s;
}

std::vector<size_t> column_widths(const std::vector<std::vector<std::string>> & rows)
{
  std::vector<size_t> widths(rows.empty() ? 0 : rows[0].size(), 0);
  for (const auto & r : rows) {
    for (size_t i = 0; i < r.size() && i < widths.size(); ++i) {
      widths[i] = std::max(widths[i], visible_width(r[i]));
    }
  }
  return widths;
}

void emit_row(std::ostringstream & os, const std::vector<std::string> & r,
  const std::vector<size_t> & widths, size_t max_width)
{
  std::string line;
  for (size_t i = 0; i < r.size(); ++i) {
    line += r[i];
    if (i + 1 < r.size()) {
      size_t w = i < widths.size() ? widths[i] : visible_width(r[i]);
      line += std::string(w - std::min(w, visible_width(r[i])) + 2, ' ');
    }
  }
  os << truncate_visible(line, max_width) << '\n';
}

void print_rows(std::ostringstream & os, const std::vector<std::vector<std::string>> & rows,
  size_t max_width = 0)
{
  auto widths = column_widths(rows);
  for (const auto & r : rows) {emit_row(os, r, widths, max_width);}
}

/// Mark column for --watch: "+" / "~" / "-" painted, or a space.
std::string mark_cell(char mark, bool color)
{
  switch (mark) {
    case '+': return paint("+", GREEN, color);
    case '~': return paint("~", YELLOW, color);
    case '-': return paint("-", DIM, color);
    default: return " ";
  }
}

/// Priority of a watch mark on a topic row: an appearance beats a change beats a removal.
int mark_rank(char mark)
{
  switch (mark) {
    case '+': return 3;
    case '~': return 2;
    case '-': return 1;
    default: return 0;
  }
}

char topic_mark(const TopicSummary & t, const WatchDecorations & w)
{
  char best = ' ';
  for (const auto & p : t.pairs) {
    auto it = w.marks.find(pair_key(t, p));
    if (it == w.marks.end()) {continue;}
    if (mark_rank(it->second) > mark_rank(best)) {best = it->second;}
  }
  return best;
}

}  // namespace

std::string render_table(const Snapshot & snap, const RenderOptions & opt)
{
  std::ostringstream os;
  const bool color = opt.color;
  const WatchDecorations * watch = opt.watch;
  const std::string indent = watch ? "     " : "    ";

  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> header = {"TOPIC", "TYPE", "PUBS", "SUBS", "TRANSPORT", "RATE", "REASON"};
  if (watch) {header.insert(header.begin(), " ");}
  rows.push_back(header);
  for (const auto & t : snap.topics) {
    std::vector<std::string> row = {
      t.display_topic, t.display_type,
      std::to_string(t.writers.size()), std::to_string(t.readers.size()),
      aggregate_transports(t, color),
      rate_label(snap.stats.enabled && t.throughput_available, t.throughput),
      aggregate_reasons(t, color)};
    if (watch) {row.insert(row.begin(), mark_cell(topic_mark(t, *watch), color));}
    rows.push_back(row);
  }
  // ghosts whose topic disappeared entirely get a topic row of their own
  std::vector<const GhostPair *> orphan_ghosts;
  if (watch) {
    for (const auto & g : watch->ghosts) {
      bool found = std::any_of(snap.topics.begin(), snap.topics.end(),
          [&](const TopicSummary & t) {return t.display_topic == g.key.topic;});
      if (!found) {orphan_ghosts.push_back(&g);}
    }
    for (const auto * g : orphan_ghosts) {
      rows.push_back({mark_cell('-', color), paint(g->key.topic, DIM, color), paint(g->type, DIM, color),
          "-", "-", paint(g->transport_label, DIM, color), "-", paint("(removed)", DIM, color)});
    }
  }

  auto ghost_rows_for = [&](const std::string & topic) {
      std::vector<std::vector<std::string>> out;
      if (!watch) {return out;}
      for (const auto & g : watch->ghosts) {
        if (g.key.topic != topic) {continue;}
        std::vector<std::string> row = {
          mark_cell('-', color),
          paint(indent.substr(1) + g.writer_label + " -> " + g.reader_label, DIM, color),
          paint(g.transport_label, DIM, color), "-"};
        if (snap.stats.enabled) {row.push_back("");}
        row.push_back(paint("(removed)", DIM, color));
        out.push_back(row);
      }
      return out;
    };

  if (!opt.verbose) {
    print_rows(os, rows, opt.max_width);
  } else {
    // Widths are computed over the topic rows only so the header stays aligned.
    auto widths = column_widths(rows);
    emit_row(os, rows[0], widths, opt.max_width);
    size_t idx = 1;
    for (const auto & t : snap.topics) {
      emit_row(os, rows[idx++], widths, opt.max_width);
      std::vector<std::vector<std::string>> pair_rows;
      for (const auto & p : t.pairs) {
        std::string reasons = join(p.verdict.reasons, ",");
        for (const auto & w : p.verdict.warnings) {reasons += "," + paint("!" + w, RED, color);}
        std::vector<std::string> row;
        if (watch) {
          auto it = watch->marks.find(pair_key(t, p));
          row.push_back(mark_cell(it == watch->marks.end() ? ' ' : it->second, color));
        }
        row.push_back(indent.substr(watch ? 1 : 0) + endpoint_label(snap, *p.writer, opt) + " -> " +
          endpoint_label(snap, *p.reader, opt));
        row.push_back(transport_label(p.verdict, color));
        row.push_back(rate_label(snap.stats.enabled && p.measured.throughput_available, p.measured.throughput));
        if (snap.stats.enabled) {
          row.push_back("measured=" + measured_label(p));
        }
        row.push_back(reasons);
        pair_rows.push_back(row);
      }
      for (auto & g : ghost_rows_for(t.display_topic)) {pair_rows.push_back(g);}
      print_rows(os, pair_rows, opt.max_width);
    }
    for (; idx < rows.size(); ++idx) {emit_row(os, rows[idx], widths, opt.max_width);}
  }
  if (watch && !watch->summary.empty()) {
    os << "\n" << paint("changes: ", BOLD, color) << watch->summary << "\n";
  }

  if (snap.topics.empty()) {
    os << "(no endpoints discovered in domain " << snap.domain << ")\n";
  }
  if (snap.stats.enabled) {
    os << "\nstatistics: " << snap.stats.samples << " samples from "
       << snap.stats.participants_with_stats.size() << " participant(s)";
    if (snap.stats.participants_with_stats.empty()) {
      os << " - start the observed nodes with FASTDDS_STATISTICS=\""
         << "RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC\"";
    }
    os << "\n";
  }

  if (snap.shm.available) {
    const auto & shm = snap.shm;
    os << "\n" << paint("shared memory: ", BOLD, color) << shm.path << " "
       << human_bytes(static_cast<double>(shm.used_bytes), "B") << " used of "
       << human_bytes(static_cast<double>(shm.total_bytes), "B") << " ("
       << human_bytes(static_cast<double>(shm.free_bytes), "B") << " free)"
       << " | Fast DDS " << human_bytes(static_cast<double>(shm.fastdds_bytes), "B") << " in "
       << shm.segments << " segment(s)";
    if (shm.stale_segments) {os << " (" << shm.stale_segments << " stale)";}
    os << ", " << shm.ports << " port(s)";
    if (shm.stale_ports) {os << " (" << shm.stale_ports << " stale)";}
    os << ", " << shm.datasharing_histories << " data-sharing histor" << (shm.datasharing_histories == 1 ? "y" : "ies");
    if (shm.datasharing_unmatched) {os << " (" << shm.datasharing_unmatched << " unmatched)";}
    os << "\n";
    for (const auto & w : shm.warnings) {
      os << "  " << paint("!" + w, RED, color);
      if (w == "shm-stale-files") {
        os << ": " << (shm.stale_segments + shm.stale_ports)
           << " file(s) without a living owner, run 'fastdds shm clean'";
      } else if (w == "shm-not-visible") {
        os << ":";
        if (!shm.missing_ports.empty()) {
          os << " " << shm.missing_ports.size() << " of " << shm.checked_ports.size()
             << " SHM port(s) of the nodes not open here (other IPC namespace)";
        }
        if (shm.other_host_participants) {
          os << (shm.missing_ports.empty() ? " " : ", ") << shm.other_host_participants
             << " participant(s) on another host id";
        }
      } else if (w == "shm-nearly-full") {
        os << ": Fast DDS cannot create segments when " << shm.path << " is full";
      }
      os << "\n";
    }
  }

  if (opt.explain) {
    std::set<std::string> used;
    for (const auto & w : snap.shm.warnings) {used.insert(w);}
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
  if (opt.max_width == 0) {
    return os.str();
  }
  // The table rows were cut while being emitted; cut the footer lines (changes,
  // statistics, shared memory, legend) the same way so that nothing wraps on a terminal.
  std::string out;
  std::istringstream lines(os.str());
  std::string line;
  while (std::getline(lines, line)) {
    out += truncate_visible(line, opt.max_width);
    out += '\n';
  }
  return out;
}

}  // namespace fastdds_transport_viz
