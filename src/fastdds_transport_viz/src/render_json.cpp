// Copyright 2026 dandelion
// SPDX-License-Identifier: Apache-2.0

#include <string>

#include <nlohmann/json.hpp>

#include "fastdds_transport_viz/render.hpp"

namespace fastdds_transport_viz
{

namespace
{
using json = nlohmann::json;

json locators_json(const std::vector<Locator> & ls)
{
  json arr = json::array();
  for (const auto & l : ls) {
    arr.push_back({{"kind", to_string(l.kind)}, {"address", l.address}, {"port", l.port}});
  }
  return arr;
}

json endpoint_json(const Snapshot & snap, const Endpoint & e, const RenderOptions & opt)
{
  return json{
    {"guid", e.guid},
    {"participant_guid_prefix", e.participant_guid_prefix},
    {"host_id", host_id_hex(e.host_id)},
    {"host", host_label(snap, e, opt)},
    {"node", e.node_name},
    {"dds_topic", e.dds_topic},
    {"dds_type", e.dds_type},
    {"ros_topic", e.ros_topic},
    {"ros_type", e.ros_type},
    {"unicast_locators", locators_json(e.unicast)},
    {"multicast_locators", locators_json(e.multicast)},
    {"qos", {
        {"reliability", e.qos.reliability},
        {"durability", e.qos.durability},
        {"data_sharing", to_string(e.qos.data_sharing)},
        {"data_sharing_domain_ids", e.qos.data_sharing_domains},
      }},
  };
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

  json topics = json::array();
  for (const auto & t : snap.topics) {
    json tj;
    tj["topic"] = t.display_topic;
    tj["dds_topic"] = t.dds_topic;
    tj["type"] = t.display_type;
    tj["is_ros_topic"] = t.is_ros_topic;
    tj["unmatched_reasons"] = t.unmatched_reasons;
    json writers = json::array();
    for (const auto * w : t.writers) {writers.push_back(endpoint_json(snap, *w, opt));}
    json readers = json::array();
    for (const auto * r : t.readers) {readers.push_back(endpoint_json(snap, *r, opt));}
    tj["writers"] = writers;
    tj["readers"] = readers;
    json pairs = json::array();
    for (const auto & p : t.pairs) {
      pairs.push_back({
          {"writer_guid", p.writer->guid},
          {"reader_guid", p.reader->guid},
          {"writer_node", p.writer->node_name},
          {"reader_node", p.reader->node_name},
          {"writer_host", host_label(snap, *p.writer, opt)},
          {"reader_host", host_label(snap, *p.reader, opt)},
          {"transport", to_string(p.verdict.transport)},
          {"confidence", to_string(p.verdict.confidence)},
          {"reasons", p.verdict.reasons},
          {"warnings", p.verdict.warnings},
        });
    }
    tj["pairs"] = pairs;
    topics.push_back(tj);
  }
  root["topics"] = topics;
  return root.dump(2) + "\n";
}

}  // namespace fastdds_transport_viz
