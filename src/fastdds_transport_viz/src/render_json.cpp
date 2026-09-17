// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <cmath>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/render.hpp"

namespace fastdds_transport_viz
{

namespace
{
using json = nlohmann::json;

json locator_json(const Locator & l)
{
  return {{"kind", to_string(l.kind)}, {"address", l.address}, {"port", l.port}};
}

json locators_json(const std::vector<Locator> & ls)
{
  json arr = json::array();
  for (const auto & l : ls) {
    arr.push_back(locator_json(l));
  }
  return arr;
}

/// The locator the verdict selected, or null when none was (a DATA_SHARING or NONE
/// verdict, or Fast DDS < 2.10 hiding a same-host peer's locators).
json selected_locator_json(const Verdict & v)
{
  if (v.locator.kind == LocatorKind::Invalid) {
    return json(nullptr);
  }
  json l = locator_json(v.locator);
  l["multicast"] = v.locator_multicast;
  return l;
}

json measured_locators_json(const std::vector<MeasuredLocator> & ls)
{
  json arr = json::array();
  for (const auto & ml : ls) {
    json l = locator_json(ml.locator);
    l["packets"] = ml.packets;
    l["bytes"] = ml.bytes;
    arr.push_back(l);
  }
  return arr;
}

json endpoint_json(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt)
{
  json j{
    {"guid", e.guid},
    {"participant_guid_prefix", e.participant_guid_prefix},
    {"host_id", host_id_hex(e.host_id)},
    {"host", host_label(snap, e, opt)},
    {"node", e.node_name},
    {"host_name", e.host_name},
    {"process", e.process},
    {"dds_topic", e.dds_topic},
    {"dds_type", e.dds_type},
    {"ros_topic", e.ros_topic},
    {"ros_type", e.ros_type},
    {"unicast_locators", locators_json(e.unicast)},
    {"multicast_locators", locators_json(e.multicast)},
    {"datasharing_history_bytes", e.datasharing_history_available ?
      json(e.datasharing_history_bytes) : json(nullptr)},
    {"qos", {
        {"reliability", e.qos.reliability},
        {"durability", e.qos.durability},
        {"data_sharing", to_string(e.qos.data_sharing)},
        {"data_sharing_domain_ids", e.qos.data_sharing_domains},
        {"deadline_s", std::isinf(e.qos.deadline_s) ? json(nullptr) : json(e.qos.deadline_s)},
        {"liveliness", e.qos.liveliness},
        {"liveliness_lease_s",
          std::isinf(e.qos.liveliness_lease_s) ? json(nullptr) : json(e.qos.liveliness_lease_s)},
        {"ownership", e.qos.ownership},
        {"partitions", e.qos.partitions},
      }},
  };
  if (!e.buffer_parent_guid.empty()) {
    j["buffer_parent_guid"] = e.buffer_parent_guid;
  }
  return j;
}
}  // namespace

std::string render_json(const Snapshot & snap, const RenderOptions & opt)
{
  json root;
  root["schema_version"] = 1;
  root["domain"] = snap.domain;
  root["observed_at"] = snap.observed_at;
  root["observation_seconds"] = snap.observation_seconds;
  root["local_host_id"] = host_id_hex(snap.local_host_id);
  // How complete the observation behind this document is (#133): `complete` is null when
  // no live participant announced its endpoints in ros_discovery_info. `stopped_on` is empty
  // only when there was no observation - `transport_viz diff` rendering documents whose own
  // `discovery` is absent - and then there is nothing to say.
  if (!snap.discovery.stopped_on.empty()) {
    root["discovery"] = {
      {"complete",
        snap.discovery.complete.has_value() ? json(*snap.discovery.complete) : json(nullptr)},
      {"stopped_on", snap.discovery.stopped_on},
      {"events", snap.discovery.events},
      {"endpoints", snap.discovery.endpoints},
      {"announced_not_discovered", snap.discovery.announced_not_discovered},
    };
  }

  json topics = json::array();
  for (const auto & t : snap.topics) {
    json tj;
    tj["topic"] = t.display_topic;
    tj["dds_topic"] = t.dds_topic;
    tj["type"] = t.display_type;
    tj["is_ros_topic"] = t.is_ros_topic;
    tj["unmatched_reasons"] = t.unmatched_reasons;
    // #137: PUBLICATION_THROUGHPUT was not a rate and is no longer subscribed; the key
    // stays, fixed to null, so documents keep their shape and schema_version stays 1
    tj["throughput_bytes_per_s"] = json(nullptr);
    // slowest pair's mean
    tj["latency_s"] = t.latency_available ? json(t.latency) : json(nullptr);
    tj["lost_packets"] = t.lost_available ? json(t.lost_packets) : json(nullptr);
    tj["resent_datas"] = t.reliability_available ? json(t.resent) : json(nullptr);
    json writers = json::array();
    for (const auto * w : t.writers) {
      writers.push_back(endpoint_json(snap, *w, opt));
    }
    json readers = json::array();
    for (const auto * r : t.readers) {
      readers.push_back(endpoint_json(snap, *r, opt));
    }
    tj["writers"] = writers;
    tj["readers"] = readers;
    json pairs = json::array();
    for (const auto & p : t.pairs) {
      pairs.push_back(
        {
          {"writer_guid", p.writer->guid},
          {"reader_guid", p.reader->guid},
          {"writer_node", p.writer->node_name},
          {"reader_node", p.reader->node_name},
          {"writer_host", host_label(snap, *p.writer, opt)},
          {"reader_host", host_label(snap, *p.reader, opt)},
          {"transport", to_string(p.verdict.transport)},
          {"locator", selected_locator_json(p.verdict)},
          {"confidence", to_string(p.verdict.confidence)},
          {"reasons", p.verdict.reasons},
          {"warnings", p.verdict.warnings},
          {"measured", {
              {"available", p.measured.available},
              {"transports", [&]() {
                  json arr = json::array();
                  for (auto t : p.measured.transports) {
                    arr.push_back(to_string(t));
                  }
                  return arr;
                }()},
              {"locators", measured_locators_json(p.measured.locators)},
              {"packets", p.measured.packets},
              {"bytes", p.measured.bytes},
              {"packets_total", p.measured.packets_total},
              {"bytes_total", p.measured.bytes_total},
              {"throughput_bytes_per_s", json(nullptr)},   // #137: removed, fixed to null
              {"latency_s", p.measured.latency_available ? json{
                  {"mean", p.measured.latency.mean()}, {"min", p.measured.latency.min},
                  {"max", p.measured.latency.max}, {"last", p.measured.latency.last},
                  {"samples", p.measured.latency.samples}} : json(nullptr)},
              {"reliability", p.measured.reliability.available ? json{
                  {"lost_packets", p.measured.reliability.lost_available ?
                    json(p.measured.reliability.lost_packets) : json(nullptr)},
                  {"resent_datas", p.measured.reliability.resent},
                  {"heartbeats", p.measured.reliability.heartbeats},
                  {"gaps", p.measured.reliability.gaps},
                  {"acknacks", p.measured.reliability.acknacks},
                  {"nackfrags", p.measured.reliability.nackfrags}} : json(nullptr)},
              {"delivered", p.measured.delivered},
              {"delivered_samples", p.measured.delivered_samples},
              {"data_submessages", p.measured.data_count_available ?
                json(p.measured.data_submessages) : json(nullptr)},
            }},
        });
    }
    tj["pairs"] = pairs;
    topics.push_back(tj);
  }
  root["topics"] = topics;

  // Descriptions for every reason / warning code that appears in this document,
  // so that front-ends never have to duplicate the texts.
  std::set<std::string> codes;
  for (const auto & t : snap.topics) {
    for (const auto & r : t.unmatched_reasons) {
      codes.insert(r);
    }
    for (const auto & p : t.pairs) {
      for (const auto & r : p.verdict.reasons) {
        codes.insert(r);
      }
      for (const auto & w : p.verdict.warnings) {
        codes.insert(w);
      }
    }
  }
  for (const auto & w : snap.shm.warnings) {
    codes.insert(w);
  }
  for (const auto & w : snap.stats.warnings) {
    codes.insert(w);
  }
  json descriptions = json::object();
  json remedies = json::object();   // same keys; null where there is nothing to change
  for (const auto & c : codes) {
    descriptions[c] = explain(c);
    auto r = remedy(c);
    remedies[c] = r ? json(*r) : json(nullptr);
  }
  root["reason_code_descriptions"] = descriptions;
  root["reason_code_remedies"] = remedies;

  if (snap.has_changes) {
    auto key_json = [](const PairKey & k) {
        return json{
        {"topic", k.topic}, {"writer_guid", k.writer_guid}, {"reader_guid", k.reader_guid},
        {"writer_node", k.writer_node}, {"reader_node", k.reader_node}};
      };
    auto state_json = [](const PairState & st) {
        json measured = json::array();
        for (auto t : st.measured) {
          measured.push_back(to_string(t));
        }
        return json{
        {"transport", to_string(st.transport)}, {"confidence", to_string(st.confidence)},
        {"measured", measured},
        {"locator", st.locator.kind == LocatorKind::Invalid ?
          json(nullptr) : locator_json(st.locator)},
        {"measured_locators", locators_json(st.measured_locators)},
        {"warnings", st.warnings}};
      };
    json changes;
    changes["key"] = to_string(snap.changes.key);
    if (snap.changes.before) {
      changes["before"] = {
        {"observed_at", snap.changes.before->observed_at},
        {"domain", snap.changes.before->domain}};
    }
    changes["added_pairs"] = json::array();
    for (const auto & k : snap.changes.added) {
      changes["added_pairs"].push_back(key_json(k));
    }
    changes["removed_pairs"] = json::array();
    for (const auto & k : snap.changes.removed) {
      changes["removed_pairs"].push_back(key_json(k));
    }
    changes["changed_pairs"] = json::array();
    for (const auto & c : snap.changes.changed) {
      json cj = key_json(c.key);
      cj["from"] = state_json(c.from);
      // the pair's identity in the before frame: differs from the key's with `--key node`
      cj["from"]["writer_guid"] = c.before_key.writer_guid;
      cj["from"]["reader_guid"] = c.before_key.reader_guid;
      cj["to"] = state_json(c.to);
      changes["changed_pairs"].push_back(cj);
    }
    root["changes"] = changes;
  }

  json stats;
  stats["enabled"] = snap.stats.enabled;
  stats["samples"] = snap.stats.samples;
  {
    json dc = json::object();
    for (const auto & kv : snap.stats.data_count) {
      dc[kv.first] = {
        {"first", kv.second.first}, {"last", kv.second.last}, {"samples", kv.second.samples}};
    }
    stats["data_count"] = dc;   // writer guid -> cumulative DATA_COUNT at first/last sample
    stats["throughput"] = json::object();   // #137: always empty, no longer subscribed
  }
  stats["participants_with_stats"] = snap.stats.participants_with_stats;
  {
    // statistics writers seen in discovery (participant prefix, topic): tells whether a
    // node has statistics enabled even before any sample arrives
    json sw = json::array();
    for (const auto & kv : snap.stats.statistics_writers) {
      sw.push_back({{"participant_guid_prefix", kv.first}, {"topic", kv.second}});
    }
    stats["statistics_writers"] = sw;
  }
  json physical = json::object();
  for (const auto & kv : snap.stats.physical) {
    physical[kv.first] = {{"host", kv.second.host}, {"user", kv.second.user},
      {"process", kv.second.process}};
  }
  stats["physical"] = physical;
  json traffic = json::array();
  for (const auto & t : snap.stats.traffic) {
    traffic.push_back(
      {{"src_participant_guid_prefix", t.src_participant_prefix},
        {"dst_locator",
          {{"kind", to_string(t.dst.kind)}, {"address", t.dst.address}, {"port", t.dst.port}}},
        {"packets", t.packets}, {"bytes", t.bytes},
        {"packets_first", t.packets_first}, {"bytes_first", t.bytes_first}});
  }
  stats["traffic"] = traffic;
  json lost = json::array();
  for (const auto & t : snap.stats.lost) {
    lost.push_back(
      {{"reporter_participant_guid_prefix", t.reporter_participant_prefix},
        {"src_participant_guid_prefix", t.src_participant_prefix},
        {"dst_locator",
          {{"kind", to_string(t.dst.kind)}, {"address", t.dst.address}, {"port", t.dst.port}}},
        {"packets", t.packets}, {"bytes", t.bytes},
        {"packets_first", t.packets_first}, {"bytes_first", t.bytes_first}});
  }
  // RTPS_LOST: what the reporter missed from src, per its own locator src addressed
  stats["lost"] = lost;
  // What never reached the tool itself (#134) - not to be confused with "lost" above, which
  // is what the observed system lost. Cumulative, and always written so that a document
  // without --stats has the same shape.
  stats["samples_lost"] = snap.stats.samples_lost;
  stats["samples_lost_at_start"] = snap.stats.samples_lost_at_start;
  stats["samples_rejected"] = snap.stats.samples_rejected;
  // Writers, not samples: what never matched never published anything to count (#141).
  stats["writers_incompatible_qos"] = snap.stats.writers_incompatible_qos;
  // Part of samples_lost, named apart because it costs no measurement (#141).
  stats["samples_lost_latency"] = snap.stats.samples_lost_latency;
  stats["warnings"] = snap.stats.warnings;
  root["stats"] = stats;

  // Shared memory of the environment the tool runs in (see docs/how-it-works.md).
  json shm;
  shm["available"] = snap.shm.available;
  shm["path"] = snap.shm.path;
  if (snap.shm.available) {
    shm["total_bytes"] = snap.shm.total_bytes;
    shm["used_bytes"] = snap.shm.used_bytes;
    shm["free_bytes"] = snap.shm.free_bytes;
    shm["fastdds_bytes"] = snap.shm.fastdds_bytes;
    shm["segments"] = snap.shm.segments;
    shm["stale_segments"] = snap.shm.stale_segments;
    shm["ports"] = snap.shm.ports;
    shm["stale_ports"] = snap.shm.stale_ports;
    shm["datasharing_histories"] = snap.shm.datasharing_histories;
    shm["datasharing_unmatched"] = snap.shm.datasharing_unmatched;
    shm["datasharing_notifications"] = snap.shm.datasharing_notifications;
    shm["checked_ports"] = snap.shm.checked_ports;
    shm["missing_ports"] = snap.shm.missing_ports;
    shm["other_host_participants"] = snap.shm.other_host_participants;
    shm["nodes_visible"] = snap.shm.nodes_visible;
  }
  shm["warnings"] = snap.shm.warnings;
  root["shm"] = shm;
  return (opt.compact ? root.dump() : root.dump(2)) + "\n";
}

}  // namespace fastdds_transport_viz
