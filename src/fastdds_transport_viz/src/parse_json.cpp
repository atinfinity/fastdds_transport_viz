// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/parse_json.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <exception>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "fastdds_transport_viz/ros_names.hpp"

namespace fastdds_transport_viz
{

namespace
{
using json = nlohmann::json;

// ---- enumerations (inverse of model.cpp's to_string) ----------------------------

LocatorKind locator_kind(const std::string & s)
{
  if (s == "UDPv4") {return LocatorKind::UDPv4;}
  if (s == "UDPv6") {return LocatorKind::UDPv6;}
  if (s == "TCPv4") {return LocatorKind::TCPv4;}
  if (s == "TCPv6") {return LocatorKind::TCPv6;}
  if (s == "SHM") {return LocatorKind::SHM;}
  if (s == "INVALID") {return LocatorKind::Invalid;}
  throw ParseError("unknown locator kind '" + s + "'");
}

Transport transport(const std::string & s)
{
  if (s == "UDPv4") {return Transport::UDPv4;}
  if (s == "UDPv6") {return Transport::UDPv6;}
  if (s == "TCPv4") {return Transport::TCPv4;}
  if (s == "TCPv6") {return Transport::TCPv6;}
  if (s == "SHM") {return Transport::SHM;}
  if (s == "DATA_SHARING") {return Transport::DataSharing;}
  if (s == "NONE") {return Transport::None;}
  throw ParseError("unknown transport '" + s + "'");
}

Confidence confidence(const std::string & s)
{
  if (s == "certain") {return Confidence::Certain;}
  if (s == "likely") {return Confidence::Likely;}
  throw ParseError("unknown confidence '" + s + "'");
}

DataSharingKind data_sharing(const std::string & s)
{
  if (s == "OFF") {return DataSharingKind::Off;}
  if (s == "ON") {return DataSharingKind::On;}
  if (s == "AUTO") {return DataSharingKind::Auto;}
  return DataSharingKind::Unknown;
}

// ---- small helpers ------------------------------------------------------------------

/// "0a0b0c0d" -> {0x0a, 0x0b, 0x0c, 0x0d}
HostId host_id(const std::string & hex)
{
  HostId id{};
  if (hex.size() != 8) {throw ParseError("host id '" + hex + "' is not 8 hex digits");}
  for (size_t i = 0; i < 4; ++i) {
    id[i] = static_cast<uint8_t>(std::stoul(hex.substr(2 * i, 2), nullptr, 16));
  }
  return id;
}

/// "01.0f.40.ec.52.00.86.93.00.00.00.00|00.00.14.03" -> 16 bytes; zeros for any other form.
std::array<uint8_t, 16> guid_bytes(const std::string & guid)
{
  std::array<uint8_t, 16> out{};
  size_t n = 0;
  for (size_t i = 0; i + 1 < guid.size() && n < 16; ) {
    if (guid[i] == '.' || guid[i] == '|') {++i; continue;}
    if (!std::isxdigit(static_cast<unsigned char>(guid[i])) ||
      !std::isxdigit(static_cast<unsigned char>(guid[i + 1])))
    {
      return {};
    }
    out[n++] = static_cast<uint8_t>(std::stoul(guid.substr(i, 2), nullptr, 16));
    i += 2;
  }
  return n == 16 ? out : std::array<uint8_t, 16>{};
}

/// The document's value of a required key, with the key in the error message.
const json & at(const json & j, const char * key, const std::string & where)
{
  auto it = j.find(key);
  if (it == j.end()) {throw ParseError(where + ": missing '" + key + "'");}
  return *it;
}

double number_or(const json & j, double fallback)
{
  return j.is_number() ? j.get<double>() : fallback;
}

Locator locator(const json & j, const std::string & where)
{
  Locator l;
  l.kind = locator_kind(at(j, "kind", where).get<std::string>());
  l.address = at(j, "address", where).get<std::string>();
  l.port = at(j, "port", where).get<uint32_t>();
  return l;
}

std::vector<Locator> locators(const json & j, const std::string & where)
{
  std::vector<Locator> out;
  for (const auto & l : j) {
    out.push_back(locator(l, where));
  }
  return out;
}

Endpoint endpoint(const json & j, bool is_writer, const std::string & where)
{
  Endpoint e;
  e.is_writer = is_writer;
  e.guid = at(j, "guid", where).get<std::string>();
  e.guid_bytes = guid_bytes(e.guid);
  e.participant_guid_prefix = at(j, "participant_guid_prefix", where).get<std::string>();
  e.host_id = host_id(at(j, "host_id", where).get<std::string>());
  // documents before #112 carry the rmw's unknown name
  e.node_name = normalize_node_name(at(j, "node", where).get<std::string>());
  e.host_name = j.value("host_name", "");
  e.process = j.value("process", "");
  e.buffer_parent_guid = j.value("buffer_parent_guid", "");
  e.dds_topic = at(j, "dds_topic", where).get<std::string>();
  e.dds_type = at(j, "dds_type", where).get<std::string>();
  e.ros_topic = at(j, "ros_topic", where).get<std::string>();
  e.ros_type = at(j, "ros_type", where).get<std::string>();
  e.unicast = locators(at(j, "unicast_locators", where), where);
  e.multicast = locators(at(j, "multicast_locators", where), where);
  const auto ds = j.find("datasharing_history_bytes");
  if (ds != j.end() && ds->is_number()) {
    e.datasharing_history_available = true;
    e.datasharing_history_bytes = ds->get<uint64_t>();
  }
  const auto & qos = at(j, "qos", where);
  e.qos.reliability = at(qos, "reliability", where).get<std::string>();
  e.qos.durability = at(qos, "durability", where).get<std::string>();
  e.qos.data_sharing = data_sharing(at(qos, "data_sharing", where).get<std::string>());
  e.qos.data_sharing_domains =
    qos.value("data_sharing_domain_ids", std::vector<uint64_t>{});
  const double inf = std::numeric_limits<double>::infinity();
  e.qos.deadline_s = number_or(qos.value("deadline_s", json(nullptr)), inf);
  e.qos.liveliness = qos.value("liveliness", "AUTOMATIC");
  e.qos.liveliness_lease_s = number_or(qos.value("liveliness_lease_s", json(nullptr)), inf);
  e.qos.ownership = qos.value("ownership", "SHARED");
  e.qos.partitions = qos.value("partitions", std::vector<std::string>{});
  return e;
}

Measurement measurement(const json & j, const std::string & where)
{
  Measurement m;
  m.available = at(j, "available", where).get<bool>();
  for (const auto & t : at(j, "transports", where)) {
    m.transports.push_back(transport(t.get<std::string>()));
  }
  for (const auto & l : at(j, "locators", where)) {
    MeasuredLocator ml;
    ml.locator = locator(l, where);
    ml.packets = l.value("packets", 0ULL);
    ml.bytes = l.value("bytes", 0.0);
    m.locators.push_back(ml);
  }
  m.packets = at(j, "packets", where).get<uint64_t>();
  m.bytes = at(j, "bytes", where).get<double>();
  m.packets_total = j.value("packets_total", 0ULL);
  m.bytes_total = j.value("bytes_total", 0.0);
  const json latency = j.value("latency_s", json(nullptr));
  if (latency.is_object()) {
    m.latency_available = true;
    m.latency.samples = latency.value("samples", 0ULL);
    m.latency.sum = latency.value("mean", 0.0) * static_cast<double>(m.latency.samples);
    m.latency.min = latency.value("min", 0.0);
    m.latency.max = latency.value("max", 0.0);
    m.latency.last = latency.value("last", 0.0);
  }
  m.delivered = at(j, "delivered", where).get<bool>();
  m.delivered_samples = j.value("delivered_samples", 0ULL);
  const json reliability = j.value("reliability", json(nullptr));
  if (reliability.is_object()) {
    m.reliability.available = true;
    const json lost = reliability.value("lost_packets", json(nullptr));
    m.reliability.lost_available = lost.is_number();
    m.reliability.lost_packets = lost.is_number() ? lost.get<uint64_t>() : 0;
    m.reliability.resent = reliability.value("resent_datas", 0ULL);
    m.reliability.heartbeats = reliability.value("heartbeats", 0ULL);
    m.reliability.gaps = reliability.value("gaps", 0ULL);
    m.reliability.acknacks = reliability.value("acknacks", 0ULL);
    m.reliability.nackfrags = reliability.value("nackfrags", 0ULL);
  }
  const json submessages = j.value("data_submessages", json(nullptr));
  m.data_count_available = submessages.is_number();
  m.data_submessages = submessages.is_number() ? submessages.get<uint64_t>() : 0;
  return m;
}

/// A TrafficSample of stats.traffic / stats.lost (which name the prefix and locator keys
/// differently).
TrafficSample traffic_sample(
  const json & j, const char * prefix_key, const char * locator_key, const std::string & where)
{
  TrafficSample t;
  t.src_participant_prefix = at(j, prefix_key, where).get<std::string>();
  t.dst = locator(at(j, locator_key, where), where);
  t.packets = at(j, "packets", where).get<uint64_t>();
  t.bytes = at(j, "bytes", where).get<double>();
  t.packets_first = j.value("packets_first", 0ULL);
  t.bytes_first = j.value("bytes_first", 0.0);
  return t;
}

StatsData stats(const json & j)
{
  const std::string where = "stats";
  StatsData s;
  s.enabled = at(j, "enabled", where).get<bool>();
  s.samples = at(j, "samples", where).get<size_t>();
  for (const auto & p : at(j, "participants_with_stats", where)) {
    s.participants_with_stats.insert(p.get<std::string>());
  }
  for (const auto & kv : at(j, "physical", where).items()) {
    s.physical[kv.key()] = HostInfo{
      kv.value().value("host", ""), kv.value().value("user", ""),
      kv.value().value("process", "")};
  }
  for (const auto & t : at(j, "traffic", where)) {
    s.traffic.push_back(
      traffic_sample(t, "src_participant_guid_prefix", "dst_locator", where + ".traffic"));
  }
  // before #122 the keys were receiver_participant_guid_prefix / from_locator (misnamed:
  // they held the sender and the addressed locator) and the reporter was not written
  const json lost = j.value("lost", json::array());
  for (const auto & t : lost) {
    const bool old_keys = !t.contains("src_participant_guid_prefix") &&
      t.contains("receiver_participant_guid_prefix");
    TrafficSample sample = old_keys ?
      traffic_sample(t, "receiver_participant_guid_prefix", "from_locator", where + ".lost") :
      traffic_sample(t, "src_participant_guid_prefix", "dst_locator", where + ".lost");
    sample.reporter_participant_prefix = t.value("reporter_participant_guid_prefix", "");
    s.lost.push_back(sample);
  }
  // (a temporary's items() would dangle: bind the optional objects first)
  const json data_count = j.value("data_count", json::object());
  for (const auto & kv : data_count.items()) {
    s.data_count[kv.key()] = DataCountSample{
      kv.value().value("first", 0ULL), kv.value().value("last", 0ULL),
      kv.value().value("samples", 0ULL)};
  }
  // #134; absent in documents written before it
  s.samples_lost = j.value("samples_lost", 0ULL);
  s.samples_lost_at_start = j.value("samples_lost_at_start", 0ULL);
  s.samples_rejected = j.value("samples_rejected", 0ULL);
  for (const auto & w : j.value("warnings", json::array())) {
    s.warnings.push_back(w.get<std::string>());
  }
  const json writers = j.value("statistics_writers", json::array());
  for (const auto & w : writers) {
    s.statistics_writers.insert({w.value("participant_guid_prefix", ""), w.value("topic", "")});
  }
  return s;
}

ShmInfo shm(const json & j)
{
  const std::string where = "shm";
  ShmInfo s;
  s.available = at(j, "available", where).get<bool>();
  s.path = at(j, "path", where).get<std::string>();
  s.total_bytes = j.value("total_bytes", 0ULL);
  s.used_bytes = j.value("used_bytes", 0ULL);
  s.free_bytes = j.value("free_bytes", 0ULL);
  s.fastdds_bytes = j.value("fastdds_bytes", 0ULL);
  s.segments = j.value("segments", 0ULL);
  s.stale_segments = j.value("stale_segments", 0ULL);
  s.ports = j.value("ports", 0ULL);
  s.stale_ports = j.value("stale_ports", 0ULL);
  s.datasharing_histories = j.value("datasharing_histories", 0ULL);
  s.datasharing_unmatched = j.value("datasharing_unmatched", 0ULL);
  s.datasharing_notifications = j.value("datasharing_notifications", 0ULL);
  s.checked_ports = j.value("checked_ports", std::vector<uint32_t>{});
  s.missing_ports = j.value("missing_ports", std::vector<uint32_t>{});
  s.other_host_participants = j.value("other_host_participants", 0ULL);
  s.nodes_visible = j.value("nodes_visible", true);
  s.warnings = at(j, "warnings", where).get<std::vector<std::string>>();
  return s;
}

Snapshot snapshot(const json & doc)
{
  if (!doc.is_object()) {throw ParseError("not a JSON object");}
  const std::string root = "document";
  const auto & version = at(doc, "schema_version", root);
  if (!version.is_number_integer() || version.get<int>() != 1) {
    const std::string found = "schema_version " + version.dump();
    throw ParseError(found + " is not supported (this tool reads 1)");
  }
  Snapshot snap;
  snap.domain = at(doc, "domain", root).get<int>();
  snap.observed_at = at(doc, "observed_at", root).get<std::string>();
  snap.observation_seconds = at(doc, "observation_seconds", root).get<double>();
  snap.local_host_id = host_id(at(doc, "local_host_id", root).get<std::string>());

  // Endpoints first, so that the topics can point into the final vector.
  const auto & topics = at(doc, "topics", root);
  struct TopicRefs
  {
    std::vector<size_t> writers;
    std::vector<size_t> readers;
  };
  std::vector<TopicRefs> refs;
  for (const auto & tj : topics) {
    const std::string where = "topic " + tj.value("topic", std::string("?"));
    TopicRefs r;
    try {
      for (const auto & w : at(tj, "writers", where)) {
        r.writers.push_back(snap.endpoints.size());
        snap.endpoints.push_back(endpoint(w, true, where));
      }
      for (const auto & rd : at(tj, "readers", where)) {
        r.readers.push_back(snap.endpoints.size());
        snap.endpoints.push_back(endpoint(rd, false, where));
      }
    } catch (const ParseError &) {
      throw;
    } catch (const std::exception & e) {
      throw ParseError(where + ": endpoint: " + e.what());
    }
    refs.push_back(std::move(r));
  }
  std::map<std::string, Endpoint *> by_guid;
  for (auto & e : snap.endpoints) {
    by_guid[e.guid] = &e;
  }
  for (const auto & e : snap.endpoints) {
    auto parent = by_guid.find(e.buffer_parent_guid);
    if (!e.buffer_parent_guid.empty() && parent != by_guid.end()) {
      parent->second->buffer_companion_guids.push_back(e.guid);
    }
  }
  size_t i = 0;
  for (const auto & tj : topics) {
    const std::string where = "topic " + tj.value("topic", std::string("?"));
    TopicSummary t;
    t.display_topic = at(tj, "topic", where).get<std::string>();
    t.dds_topic = at(tj, "dds_topic", where).get<std::string>();
    t.display_type = at(tj, "type", where).get<std::string>();
    t.is_ros_topic = at(tj, "is_ros_topic", where).get<bool>();
    t.unmatched_reasons = at(tj, "unmatched_reasons", where).get<std::vector<std::string>>();
    const json latency = tj.value("latency_s", json(nullptr));
    t.latency_available = latency.is_number();
    t.latency = number_or(latency, 0.0);
    const json lost = tj.value("lost_packets", json(nullptr));
    const json resent = tj.value("resent_datas", json(nullptr));
    t.reliability_available = resent.is_number() || lost.is_number();
    t.lost_available = lost.is_number();
    t.lost_packets = lost.is_number() ? lost.get<uint64_t>() : 0;
    t.resent = resent.is_number() ? resent.get<uint64_t>() : 0;
    for (size_t w : refs[i].writers) {
      t.writers.push_back(&snap.endpoints[w]);
    }
    for (size_t r : refs[i].readers) {
      t.readers.push_back(&snap.endpoints[r]);
    }
    for (const auto & pj : at(tj, "pairs", where)) {
      try {
        Pair p;
        const std::string wguid = at(pj, "writer_guid", where).get<std::string>();
        const std::string rguid = at(pj, "reader_guid", where).get<std::string>();
        auto w = by_guid.find(wguid);
        auto r = by_guid.find(rguid);
        if (w == by_guid.end() || r == by_guid.end()) {
          throw ParseError(where + ": pair names an endpoint that is not listed");
        }
        p.writer = w->second;
        p.reader = r->second;
        p.verdict.transport = transport(at(pj, "transport", where).get<std::string>());
        p.verdict.confidence = confidence(at(pj, "confidence", where).get<std::string>());
        const auto & loc = at(pj, "locator", where);
        if (loc.is_object()) {
          p.verdict.locator = locator(loc, where);
          p.verdict.locator_multicast = loc.value("multicast", false);
        }
        p.verdict.reasons = at(pj, "reasons", where).get<std::vector<std::string>>();
        p.verdict.warnings = at(pj, "warnings", where).get<std::vector<std::string>>();
        p.measured = measurement(at(pj, "measured", where), where);
        t.pairs.push_back(std::move(p));
      } catch (const ParseError &) {
        throw;
      } catch (const std::exception & e) {
        throw ParseError(where + ": pair: " + e.what());
      }
    }
    snap.topics.push_back(std::move(t));
    ++i;
  }
  try {
    snap.stats = stats(at(doc, "stats", root));
  } catch (const ParseError &) {
    throw;
  } catch (const std::exception & e) {
    throw ParseError(std::string("stats: ") + e.what());
  }
  // #133; absent in documents of earlier versions, and never part of a diff comparison
  auto discovery_it = doc.find("discovery");
  if (discovery_it != doc.end() && discovery_it->is_object()) {
    const json & d = *discovery_it;
    const json complete = d.value("complete", json(nullptr));
    if (complete.is_boolean()) {snap.discovery.complete = complete.get<bool>();}
    snap.discovery.stopped_on = d.value("stopped_on", std::string());
    snap.discovery.events = d.value("events", 0ULL);
    snap.discovery.endpoints = d.value("endpoints", 0ULL);
    snap.discovery.announced_not_discovered = d.value("announced_not_discovered", 0ULL);
  }
  auto shm_it = doc.find("shm");
  if (shm_it != doc.end() && shm_it->is_object()) {
    try {
      snap.shm = shm(*shm_it);
    } catch (const std::exception & e) {
      throw ParseError(std::string("shm: ") + e.what());
    }
  }
  return snap;
}
}  // namespace

Snapshot parse_json(const std::string & text, size_t * documents)
{
  json doc;
  size_t count = 1;
  try {
    doc = json::parse(text);
  } catch (const json::parse_error &) {
    // JSON Lines (--watch --json): one document per line, the last one counts.
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
      size_t end = text.find('\n', start);
      if (end == std::string::npos) {end = text.size();}
      std::string line = text.substr(start, end - start);
      if (line.find_first_not_of(" \t\r") != std::string::npos) {lines.push_back(line);}
      start = end + 1;
    }
    if (lines.size() < 2) {throw ParseError("not valid JSON");}
    try {
      doc = json::parse(lines.back());
    } catch (const json::parse_error & e) {
      throw ParseError(std::string("last line is not valid JSON: ") + e.what());
    }
    count = lines.size();
  }
  if (documents) {*documents = count;}
  try {
    return snapshot(doc);
  } catch (const ParseError &) {
    throw;
  } catch (const std::exception & e) {   // nlohmann type errors, std::stoul on bad hex
    throw ParseError(std::string("unexpected value: ") + e.what());
  }
}

}  // namespace fastdds_transport_viz
