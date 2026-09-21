// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// render_json(): every documented key, the --watch `changes` object, the `shm` object and
// the compact (JSON Lines) mode. The schema itself is checked by test_json_schema*.

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/model.hpp"
#include "fastdds_transport_viz/parse_json.hpp"
#include "fastdds_transport_viz/render.hpp"
#include "fastdds_transport_viz/stats_observer.hpp"

using namespace fastdds_transport_viz;  // NOLINT
using json = nlohmann::json;

namespace
{
Endpoint ep(bool writer, const std::string & guid, const std::string & node)
{
  Endpoint e;
  e.is_writer = writer;
  e.guid = guid;
  e.participant_guid_prefix = writer ? "P1" : "P2";
  e.node_name = node;
  e.host_id = {1, 2, 3, 4};
  e.dds_topic = "rt/chatter";
  e.dds_type = "std_msgs::msg::dds_::String_";
  e.ros_topic = "/chatter";
  e.ros_type = "std_msgs/msg/String";
  e.unicast.push_back(Locator{LocatorKind::SHM, "", writer ? 7411u : 7413u});
  e.unicast.push_back(Locator{LocatorKind::UDPv4, "10.0.0.1", 7411});
  e.qos.reliability = "RELIABLE";
  e.qos.durability = "VOLATILE";
  e.qos.data_sharing = DataSharingKind::Off;
  return e;
}

Snapshot snapshot()
{
  Snapshot s;
  s.domain = 3;
  s.observed_at = "2026-09-06T00:00:00Z";
  s.observation_seconds = 2.5;
  s.local_host_id = {1, 2, 3, 4};
  s.endpoints.push_back(ep(true, "W1", "/talker"));
  s.endpoints.push_back(ep(false, "R1", "/listener"));
  s.endpoints[0].host_name = "robot:1";   // what collect() copies from PHYSICAL_DATA
  s.endpoints[0].process = "42";
  s.endpoints[0].datasharing_history_available = true;
  s.endpoints[0].datasharing_history_bytes = 3928;
  s.endpoints[0].datasharing_segment_visibility = ShmVisibility::Visible;
  s.endpoints[1].datasharing_segment_visibility = ShmVisibility::NotVisible;
  s.endpoints[0].type_hash = "RIHS01_" + std::string(64, 'a');   // #85
  s.endpoints[1].type_hash = s.endpoints[0].type_hash;
  s.endpoints[0].type_information_hash = std::string(28, '1');   // #206
  s.endpoints[1].type_information_hash = s.endpoints[0].type_information_hash;
  s.endpoints[0].type_information_minimal_hash = std::string(28, '3');   // #213
  s.endpoints[1].type_information_minimal_hash = s.endpoints[0].type_information_minimal_hash;
  s.topics = summarize(s.endpoints);
  s.stats.enabled = true;
  s.stats.samples = 5;
  s.stats.samples_lost = 7;              // #134: what never reached the tool
  s.stats.samples_lost_at_start = 3;
  s.stats.samples_rejected = 1;
  s.stats.writers_incompatible_qos = 2;  // #141: writers the readers could not match
  s.stats.writers_announced = 16;   // #168: the settle rule of the one-shot
  s.stats.writers_heard = 16;
  s.stats.measured_instances = 640;   // #179: RTPS_SENT instances towards a reader port
  s.stats.settled = true;
  s.stats.settled_at_s = 6.25;
  s.stats.samples_lost_latency = 4;      // #141: the best-effort part of samples_lost
  s.stats.pairs_delivered = 9;           // #147: pairs with a delivery proof ...
  s.stats.pairs_delivered_absent = 2;
  s.stats.pairs_delivered_unmeasured = 5;   // ... and the measurements the loss cost
  s.stats.warnings = {"stats-samples-lost"};
  s.stats.participants_with_stats = {"P1"};
  s.stats.physical["P1"] = HostInfo{"robot:1", "user", "42"};
  s.stats.traffic.push_back(
    TrafficSample{"P1", Locator{LocatorKind::SHM, "", 7413}, 10, 1000.0, 4, 400.0, 3});
  s.stats.data_count["W1"] = DataCountSample{2, 5, 2};
  LatencyStat lat;
  lat.add(0.001); lat.add(0.003);
  s.stats.latency[{"W1", "R1"}] = lat;
  s.stats.delivery_window[{"W1", "R1"}] = DeliveryWindow{2, 10.0, 10.5};   // #143
  s.stats.latency_gaps["P2"] = 1;
  s.stats.resent_datas["W1"] = DataCountSample{1, 3, 2};
  s.stats.acknacks["R1"] = DataCountSample{0, 4, 2};
  // the reader's participant P2 missed nothing from P1 during the observation
  s.stats.lost.push_back(
    TrafficSample{"P1", Locator{LocatorKind::UDPv4, "10.0.0.1", 7411}, 5, 500.0, 5, 500.0, 2,
      "P2"});
  apply_stats(s.topics, s.stats);
  s.shm.available = true;
  s.shm.path = "/dev/shm";
  s.shm.total_bytes = 100;
  s.shm.used_bytes = 40;
  s.shm.free_bytes = 60;
  s.shm.segments = 2;
  s.shm.checked_ports = {7411};
  s.shm.warnings = {"shm-stale-files"};
  s.shm.stale_segments = 1;
  // #125: the writer's participant with its ports as the split verdicts saw them
  Participant p1;
  p1.guid_prefix = "P1";
  p1.host_id = {1, 2, 3, 4};
  p1.host_name = "robot:1";
  p1.shm_visibility = ShmVisibility::Visible;
  p1.shm_ports.push_back(Participant::ShmPort{7411, PortLock::Held, 1, true});
  p1.shm_ports.push_back(Participant::ShmPort{7001, PortLock::Held, 2, false});
  Participant p2;
  p2.guid_prefix = "P2";
  p2.host_id = {1, 2, 3, 4};
  p2.shm_visibility = ShmVisibility::NotVisible;
  p2.shm_ports.push_back(Participant::ShmPort{7413, PortLock::Absent, 1, true});
  Participant own;
  own.guid_prefix = "P9";
  own.host_id = {1, 2, 3, 4};
  own.own = true;
  own.shm_ports.push_back(Participant::ShmPort{7002, PortLock::Own, 1, false});
  // #86: p1 is a client of a Discovery Server that has no endpoint of its own
  p1.discovery_protocol = "CLIENT";
  p1.name = "/";
  p1.vendor = "eProsima";
  p1.metatraffic_locators = {Locator{LocatorKind::UDPv4, "127.0.0.1", 7410}};
  p1.discovery_server = "DS";
  Participant server;
  server.guid_prefix = "DS";
  server.host_id = {1, 2, 3, 4};
  server.discovery_protocol = "SERVER";
  server.name = "DiscoveryServerAuto";
  server.vendor = "eProsima";
  server.metatraffic_locators = {Locator{LocatorKind::UDPv4, "127.0.0.1", 11811}};
  s.participants = {p1, p2, own, server};
  s.discovery.complete = false;   // one announced endpoint never arrived (#133)
  s.discovery.stopped_on = "quiet";
  s.discovery.events = 12;
  s.discovery.endpoints = 2;
  s.discovery.announced_not_discovered = 1;
  s.discovery.observer_protocol = "SUPER_CLIENT";
  s.discovery.discovery_servers = {Locator{LocatorKind::UDPv4, "127.0.0.1", 11811}};
  s.discovery.discovery_server_env = "127.0.0.1";
  return s;
}
}  // namespace

TEST(RenderJson, DocumentKeys)
{
  auto s = snapshot();
  auto doc = json::parse(render_json(s, RenderOptions{}));
  EXPECT_EQ(doc["schema_version"], 1);
  EXPECT_EQ(doc["domain"], 3);
  EXPECT_EQ(doc["local_host_id"], "01020304");
  EXPECT_EQ(doc["discovery"]["complete"], false);
  EXPECT_EQ(doc["discovery"]["stopped_on"], "quiet");
  EXPECT_EQ(doc["discovery"]["events"], 12);
  EXPECT_EQ(doc["discovery"]["endpoints"], 2);
  EXPECT_EQ(doc["discovery"]["announced_not_discovered"], 1);
  // #86: how the tool took part in discovery
  EXPECT_EQ(doc["discovery"]["observer_protocol"], "SUPER_CLIENT");
  EXPECT_EQ(doc["discovery"]["discovery_server_env"], "127.0.0.1");
  EXPECT_EQ(doc["discovery"]["easy_mode"], "");
  ASSERT_EQ(doc["discovery"]["discovery_servers"].size(), 1u);
  EXPECT_EQ(doc["discovery"]["discovery_servers"][0]["port"], 11811);
  ASSERT_EQ(doc["topics"].size(), 1u);
  const auto & t = doc["topics"][0];
  EXPECT_EQ(t["topic"], "/chatter");
  // #137: the key stays for compatibility but is fixed to null
  EXPECT_TRUE(t["throughput_bytes_per_s"].is_null());
  EXPECT_EQ(t["writers"][0]["datasharing_history_bytes"], 3928);
  EXPECT_TRUE(t["readers"][0]["datasharing_history_bytes"].is_null());
  EXPECT_EQ(t["writers"][0]["datasharing_segment_visibility"], "visible");
  EXPECT_EQ(t["readers"][0]["datasharing_segment_visibility"], "not-visible");
  EXPECT_EQ(t["writers"][0]["type_hash"], "RIHS01_" + std::string(64, 'a'));
  EXPECT_EQ(t["writers"][0]["type_information_hash"], std::string(28, '1'));
  EXPECT_EQ(t["writers"][0]["type_information_minimal_hash"], std::string(28, '3'));
  EXPECT_EQ(t["writers"][0]["host"], "robot");
  EXPECT_EQ(t["writers"][0]["qos"]["data_sharing"], "OFF");
  ASSERT_EQ(t["pairs"].size(), 1u);
  const auto & p = t["pairs"][0];
  EXPECT_EQ(p["transport"], "SHM");
  EXPECT_EQ(p["confidence"], "certain");
  EXPECT_EQ(p["measured"]["transports"], json::array({"SHM"}));
  EXPECT_EQ(p["measured"]["packets"], 6);          // 10 - 4 during the observation
  EXPECT_EQ(p["measured"]["packets_total"], 10);
  EXPECT_TRUE(p["measured"]["throughput_bytes_per_s"].is_null());   // #137
  EXPECT_EQ(p["measured"]["data_submessages"], 3);
  EXPECT_EQ(p["measured"]["latency_s"]["mean"], 0.002);
  EXPECT_EQ(p["measured"]["latency_s"]["max"], 0.003);
  EXPECT_EQ(p["measured"]["latency_s"]["min"], 0.001);
  EXPECT_EQ(p["measured"]["latency_s"]["samples"], 2);
  EXPECT_EQ(t["latency_s"], 0.002);
  // #143: 2 samples 0.5 s apart, the reader's participant skipped one, one-shot window
  EXPECT_EQ(p["measured"]["delivered_per_s"], 2.0);
  EXPECT_EQ(p["measured"]["delivered_per_s_lower_bound"], true);
  EXPECT_EQ(p["measured"]["delivered_per_s_window_s"], 2.5);
  EXPECT_EQ(p["measured"]["reliability"]["resent_datas"], 2);
  EXPECT_EQ(p["measured"]["reliability"]["acknacks"], 4);
  EXPECT_EQ(p["measured"]["reliability"]["lost_packets"], 0);
  EXPECT_EQ(t["lost_packets"], 0);
  EXPECT_EQ(t["resent_datas"], 2);
  EXPECT_EQ(doc["stats"]["lost"][0]["reporter_participant_guid_prefix"], "P2");
  EXPECT_EQ(doc["stats"]["lost"][0]["src_participant_guid_prefix"], "P1");
  EXPECT_EQ(doc["stats"]["lost"][0]["dst_locator"]["port"], 7411);
  EXPECT_FALSE(doc.contains("changes"));
  // statistics block
  EXPECT_EQ(doc["stats"]["samples"], 5);
  EXPECT_EQ(doc["stats"]["physical"]["P1"]["process"], "42");
  EXPECT_EQ(doc["stats"]["traffic"][0]["packets_first"], 4);
  EXPECT_EQ(doc["stats"]["data_count"]["W1"]["last"], 5);
  EXPECT_TRUE(doc["stats"]["throughput"].empty());   // #137: no longer subscribed
  // what the tool itself missed (#134), next to `lost`, which is what the system missed
  EXPECT_EQ(doc["stats"]["samples_lost"], 7);
  EXPECT_EQ(doc["stats"]["samples_lost_at_start"], 3);
  EXPECT_EQ(doc["stats"]["samples_rejected"], 1);
  EXPECT_EQ(doc["stats"]["writers_incompatible_qos"], 2);
  EXPECT_EQ(doc["stats"]["writers_announced"], 16);
  EXPECT_EQ(doc["stats"]["writers_heard"], 16);
  EXPECT_EQ(doc["stats"]["measured_instances"], 640);
  EXPECT_EQ(doc["stats"]["settled"], true);
  EXPECT_DOUBLE_EQ(doc["stats"]["settled_at_s"].get<double>(), 6.25);
  // a part of samples_lost, not a number beside it
  EXPECT_EQ(doc["stats"]["samples_lost_latency"], 4);
  EXPECT_EQ(doc["stats"]["pairs_delivered"], 9);
  EXPECT_EQ(doc["stats"]["pairs_delivered_unmeasured"], 5);
  EXPECT_EQ(doc["stats"]["pairs_delivered_absent"], 2);
  EXPECT_EQ(doc["stats"]["warnings"], json::array({"stats-samples-lost"}));
  // the document-level code is described and advised like any pair code
  EXPECT_FALSE(doc["reason_code_descriptions"]["stats-samples-lost"].get<std::string>().empty());
  EXPECT_FALSE(doc["reason_code_remedies"]["stats-samples-lost"].is_null());
  // shm block and its warning's description
  EXPECT_EQ(doc["shm"]["available"], true);
  EXPECT_EQ(doc["shm"]["used_bytes"], 40);
  EXPECT_EQ(doc["shm"]["checked_ports"], json::array({7411}));
  EXPECT_EQ(doc["shm"]["unknown_ports"], json::array());
  EXPECT_EQ(doc["shm"]["warnings"], json::array({"shm-stale-files"}));
  EXPECT_TRUE(doc["reason_code_descriptions"].contains("shm-stale-files"));
  // #125: the participants with what the split verdicts saw of their ports
  ASSERT_EQ(doc["participants"].size(), 4u);
  const auto & p1 = doc["participants"][0];
  // #86: what the participant announced about discovery, and its server when certain
  EXPECT_EQ(p1["discovery_protocol"], "CLIENT");
  EXPECT_EQ(p1["name"], "/");
  EXPECT_EQ(p1["vendor"], "eProsima");
  ASSERT_EQ(p1["metatraffic_locators"].size(), 1u);
  EXPECT_EQ(p1["metatraffic_locators"][0]["kind"], "UDPv4");
  EXPECT_EQ(p1["metatraffic_locators"][0]["port"], 7410);
  EXPECT_EQ(p1["discovery_server"], "DS");
  const auto & server = doc["participants"][3];
  EXPECT_EQ(server["discovery_protocol"], "SERVER");
  EXPECT_EQ(server["name"], "DiscoveryServerAuto");
  EXPECT_TRUE(server["discovery_server"].is_null());
  EXPECT_EQ(server["shm_ports"], json::array());
  EXPECT_EQ(p1["guid_prefix"], "P1");
  EXPECT_EQ(p1["host_id"], "01020304");
  EXPECT_EQ(p1["host"], "robot");
  EXPECT_EQ(p1["host_name"], "robot:1");
  EXPECT_EQ(p1["own"], false);
  EXPECT_EQ(p1["shm_visibility"], "visible");
  ASSERT_EQ(p1["shm_ports"].size(), 2u);
  const json first_port = {{"port", 7411}, {"lock", "held"}, {"announced_by", 1}, {"proof", true}};
  EXPECT_EQ(p1["shm_ports"][0], first_port);
  EXPECT_EQ(p1["shm_ports"][1]["announced_by"], 2);
  EXPECT_EQ(p1["shm_ports"][1]["proof"], false);
  const auto & p2 = doc["participants"][1];
  EXPECT_EQ(p2["host"], "local");
  EXPECT_EQ(p2["shm_visibility"], "not-visible");
  EXPECT_EQ(p2["shm_ports"][0]["lock"], "absent");
  const auto & own = doc["participants"][2];
  EXPECT_EQ(own["own"], true);
  EXPECT_EQ(own["shm_visibility"], "unprobed");
  EXPECT_EQ(own["shm_ports"][0]["lock"], "own");
  EXPECT_TRUE(doc["reason_code_descriptions"].contains("measured-shm-traffic"));
  // remedies: same keys as the descriptions, null where there is nothing to change
  std::set<std::string> desc_keys, remedy_keys;
  for (const auto & kv : doc["reason_code_descriptions"].items()) {
    desc_keys.insert(kv.key());
  }
  for (const auto & kv : doc["reason_code_remedies"].items()) {
    remedy_keys.insert(kv.key());
  }
  EXPECT_EQ(desc_keys, remedy_keys);
  EXPECT_TRUE(doc["reason_code_remedies"]["shm-stale-files"].is_string());
  EXPECT_TRUE(doc["reason_code_remedies"]["measured-shm-traffic"].is_null());
}

TEST(RenderJson, ShmUnavailableOmitsSizes)
{
  auto s = snapshot();
  s.shm = ShmInfo{};
  s.shm.path = "/dev/shm";
  auto doc = json::parse(render_json(s, RenderOptions{}));
  EXPECT_EQ(doc["shm"]["available"], false);
  EXPECT_TRUE(doc["topics"][0]["pairs"][0]["measured"]["latency_s"].is_object());
  s.stats.latency.clear();
  s.topics = summarize(s.endpoints);
  apply_stats(s.topics, s.stats);
  doc = json::parse(render_json(s, RenderOptions{}));
  EXPECT_TRUE(doc["topics"][0]["pairs"][0]["measured"]["latency_s"].is_null());
  EXPECT_TRUE(doc["topics"][0]["latency_s"].is_null());
  // #143: under two samples in the window the rate is null, the window still told
  s.stats.delivery_window.clear();
  s.stats.rate_window_s = 5.0;
  s.topics = summarize(s.endpoints);
  apply_stats(s.topics, s.stats);
  doc = json::parse(render_json(s, RenderOptions{}));
  EXPECT_TRUE(doc["topics"][0]["pairs"][0]["measured"]["delivered_per_s"].is_null());
  EXPECT_EQ(doc["topics"][0]["pairs"][0]["measured"]["delivered_per_s_lower_bound"], false);
  EXPECT_EQ(doc["topics"][0]["pairs"][0]["measured"]["delivered_per_s_window_s"], 5.0);
  s.stats.rate_window_s = 0.0;
  // counters, but no RTPS_LOST from the reader's participant
  s.stats.lost.clear();
  s.topics = summarize(s.endpoints);
  apply_stats(s.topics, s.stats);
  doc = json::parse(render_json(s, RenderOptions{}));
  EXPECT_TRUE(doc["topics"][0]["pairs"][0]["measured"]["reliability"]["lost_packets"].is_null());
  EXPECT_EQ(doc["topics"][0]["pairs"][0]["measured"]["reliability"]["resent_datas"], 2);
  EXPECT_TRUE(doc["topics"][0]["lost_packets"].is_null());
  EXPECT_EQ(doc["topics"][0]["resent_datas"], 2);
  s.stats.resent_datas.clear(); s.stats.acknacks.clear();
  s.topics = summarize(s.endpoints);
  apply_stats(s.topics, s.stats);
  doc = json::parse(render_json(s, RenderOptions{}));
  EXPECT_TRUE(doc["topics"][0]["pairs"][0]["measured"]["reliability"].is_null());
  EXPECT_TRUE(doc["topics"][0]["lost_packets"].is_null());
  EXPECT_FALSE(doc["shm"].contains("total_bytes"));
  EXPECT_EQ(doc["shm"]["warnings"], json::array());
}

TEST(RenderJson, ChangesObjectAndCompactMode)
{
  auto s = snapshot();
  s.has_changes = true;
  PairKey k{"/chatter", "W1", "R1"};
  s.changes.added.push_back(PairKey{"/new", "W2", "R2"});
  s.changes.removed.push_back(PairKey{"/old", "W3", "R3"});
  PairState from; from.transport = Transport::UDPv4;
  PairState to; to.transport = Transport::SHM; to.measured = {Transport::SHM}; to.warnings = {"x"};
  s.changes.changed.push_back(PairChange{k, from, to, k});
  RenderOptions opt;
  opt.compact = true;
  auto text = render_json(s, opt);
  EXPECT_EQ(text.back(), '\n');
  EXPECT_EQ(text.find('\n'), text.size() - 1) << "JSON Lines: exactly one line";
  auto doc = json::parse(text);
  EXPECT_EQ(doc["changes"]["added_pairs"][0]["topic"], "/new");
  EXPECT_EQ(doc["changes"]["removed_pairs"][0]["writer_guid"], "W3");
  const auto & c = doc["changes"]["changed_pairs"][0];
  EXPECT_EQ(c["reader_guid"], "R1");
  EXPECT_EQ(c["from"]["transport"], "UDPv4");
  EXPECT_EQ(c["to"]["transport"], "SHM");
  EXPECT_EQ(c["to"]["measured"], json::array({"SHM"}));
  EXPECT_EQ(c["to"]["warnings"], json::array({"x"}));
  // --watch: GUID key, the before identity equals the key, no before document
  EXPECT_EQ(doc["changes"]["key"], "guid");
  EXPECT_EQ(c["from"]["writer_guid"], "W1");
  EXPECT_EQ(c["from"]["reader_guid"], "R1");
  EXPECT_FALSE(doc["changes"].contains("before"));
  EXPECT_EQ(doc["changes"]["added_pairs"][0]["writer_node"], "");
  // pretty mode spans lines
  EXPECT_GT(render_json(s, RenderOptions{}).find('\n'), 0u);
  EXPECT_NE(render_json(s, RenderOptions{}).find("\n  \"changes\""), std::string::npos);
}

TEST(StatsObserver, RequiredEnvValueIsTheDocumentedTenTopicList)
{
  EXPECT_EQ(
    StatsObserver::required_env_value(),
    "RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;"
    "DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;"
    "HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC");
}

TEST(RenderJson, DiffChangesCarryKeyModeNodesAndTheBeforeDocument)
{
  auto s = snapshot();
  s.has_changes = true;
  s.changes.key = KeyMode::Node;
  s.changes.before = SnapshotRef{"2026-09-05T23:00:00Z", 3};
  PairKey now = pair_key(s.topics[0], s.topics[0].pairs[0]);
  PairKey then = now;
  then.writer_guid = "W0";
  then.reader_guid = "R0";
  PairState from = pair_state(s.topics[0].pairs[0]);
  from.transport = Transport::UDPv4;
  s.changes.changed.push_back(PairChange{now, from, pair_state(s.topics[0].pairs[0]), then});
  s.changes.removed.push_back(then);
  auto doc = json::parse(render_json(s, RenderOptions{}));
  const auto & ch = doc["changes"];
  EXPECT_EQ(ch["key"], "node");
  EXPECT_EQ(ch["before"]["observed_at"], "2026-09-05T23:00:00Z");
  EXPECT_EQ(ch["before"]["domain"], 3);
  const auto & c = ch["changed_pairs"][0];
  EXPECT_EQ(c["writer_guid"], "W1");
  EXPECT_EQ(c["writer_node"], "/talker");
  EXPECT_EQ(c["reader_node"], "/listener");
  EXPECT_EQ(c["from"]["writer_guid"], "W0");
  EXPECT_EQ(c["from"]["reader_guid"], "R0");
  EXPECT_EQ(c["from"]["transport"], "UDPv4");
  EXPECT_EQ(c["to"]["transport"], "SHM");
  EXPECT_EQ(ch["removed_pairs"][0]["writer_guid"], "W0");
  EXPECT_EQ(ch["removed_pairs"][0]["reader_node"], "/listener");
}

// ---- parse_json(): the inverse of render_json() ---------------------------------------

namespace
{
/// Structural equality with a tolerance on numbers: a mean stored as sum/samples does not
/// survive a round trip bit for bit.
bool close(const json & a, const json & b, std::string path, std::string * where)
{
  if (a.is_number() && b.is_number()) {
    double x = a.get<double>(), y = b.get<double>();
    if (std::fabs(x - y) <= 1e-9 * std::max(1.0, std::fabs(x))) {return true;}
    *where = path + ": " + a.dump() + " vs " + b.dump();
    return false;
  }
  if (a.type() != b.type()) {
    *where = path + ": " + a.dump() + " vs " + b.dump();
    return false;
  }
  if (a.is_object()) {
    for (const auto & kv : a.items()) {
      if (!b.contains(kv.key())) {*where = path + "." + kv.key() + " missing"; return false;}
      if (!close(kv.value(), b[kv.key()], path + "." + kv.key(), where)) {return false;}
    }
    for (const auto & kv : b.items()) {
      if (!a.contains(kv.key())) {*where = path + "." + kv.key() + " extra"; return false;}
    }
    return true;
  }
  if (a.is_array()) {
    if (a.size() != b.size()) {*where = path + ": size"; return false;}
    for (size_t i = 0; i < a.size(); ++i) {
      if (!close(a[i], b[i], path + "[" + std::to_string(i) + "]", where)) {return false;}
    }
    return true;
  }
  if (a == b) {return true;}
  *where = path + ": " + a.dump() + " vs " + b.dump();
  return false;
}
}  // namespace

TEST(ParseJson, RoundTripsEverythingTheRenderersShow)
{
  auto s = snapshot();
  const auto text = render_json(s, RenderOptions{});
  auto parsed = parse_json(text);
  EXPECT_EQ(parsed.domain, 3);
  EXPECT_EQ(parsed.observed_at, "2026-09-06T00:00:00Z");
  EXPECT_EQ(parsed.local_host_id, (HostId{1, 2, 3, 4}));
  ASSERT_TRUE(parsed.discovery.complete.has_value());
  EXPECT_FALSE(*parsed.discovery.complete);
  EXPECT_EQ(parsed.discovery.stopped_on, "quiet");
  EXPECT_EQ(parsed.discovery.events, 12u);
  EXPECT_EQ(parsed.discovery.endpoints, 2u);
  EXPECT_EQ(parsed.discovery.announced_not_discovered, 1u);
  EXPECT_EQ(parsed.discovery.observer_protocol, "SUPER_CLIENT");   // #86
  EXPECT_EQ(parsed.discovery.discovery_server_env, "127.0.0.1");
  ASSERT_EQ(parsed.discovery.discovery_servers.size(), 1u);
  EXPECT_EQ(parsed.discovery.discovery_servers[0].port, 11811u);
  ASSERT_EQ(parsed.participants.size(), 4u);
  EXPECT_EQ(parsed.participants[0].discovery_protocol, "CLIENT");
  EXPECT_EQ(parsed.participants[0].name, "/");
  EXPECT_EQ(parsed.participants[0].vendor, "eProsima");
  ASSERT_EQ(parsed.participants[0].metatraffic_locators.size(), 1u);
  EXPECT_EQ(parsed.participants[0].metatraffic_locators[0].address, "127.0.0.1");
  EXPECT_EQ(parsed.participants[0].discovery_server, "DS");
  EXPECT_FALSE(parsed.participants[3].discovery_server.has_value());
  EXPECT_EQ(parsed.participants[3].name, "DiscoveryServerAuto");
  ASSERT_EQ(parsed.topics.size(), 1u);
  ASSERT_EQ(parsed.topics[0].pairs.size(), 1u);
  const auto & p = parsed.topics[0].pairs[0];
  EXPECT_EQ(p.writer->guid, "W1");
  EXPECT_EQ(p.writer->node_name, "/talker");
  EXPECT_EQ(p.writer->host_name, "robot:1");
  EXPECT_EQ(p.writer->process, "42");
  EXPECT_TRUE(p.writer->datasharing_history_available);
  EXPECT_EQ(p.writer->datasharing_history_bytes, 3928u);
  EXPECT_EQ(p.writer->datasharing_segment_visibility, ShmVisibility::Visible);
  EXPECT_EQ(p.reader->datasharing_segment_visibility, ShmVisibility::NotVisible);
  EXPECT_EQ(p.writer->type_hash, "RIHS01_" + std::string(64, 'a'));
  EXPECT_EQ(p.reader->type_hash, p.writer->type_hash);
  EXPECT_EQ(p.writer->type_information_hash, std::string(28, '1'));
  EXPECT_EQ(p.reader->type_information_hash, p.writer->type_information_hash);
  EXPECT_EQ(p.writer->type_information_minimal_hash, std::string(28, '3'));
  EXPECT_EQ(p.reader->type_information_minimal_hash, p.writer->type_information_minimal_hash);
  EXPECT_EQ(p.reader->guid, "R1");
  EXPECT_FALSE(p.reader->is_writer);
  EXPECT_TRUE(p.measured.rate_available);   // #143
  EXPECT_DOUBLE_EQ(p.measured.delivered_per_s, 2.0);
  EXPECT_TRUE(p.measured.rate_lower_bound);
  EXPECT_DOUBLE_EQ(p.measured.rate_window_s, 2.5);
  EXPECT_EQ(p.verdict.transport, Transport::SHM);
  EXPECT_EQ(p.verdict.locator.kind, LocatorKind::SHM);
  EXPECT_EQ(p.measured.transports, std::vector<Transport>{Transport::SHM});
  EXPECT_EQ(p.measured.packets, 6u);
  EXPECT_TRUE(p.measured.latency_available);
  EXPECT_EQ(p.measured.latency.samples, 2u);
  EXPECT_TRUE(p.measured.reliability.available);
  EXPECT_EQ(p.measured.reliability.acknacks, 4u);
  EXPECT_TRUE(p.measured.reliability.lost_available);
  EXPECT_TRUE(parsed.topics[0].lost_available);
  ASSERT_EQ(parsed.stats.lost.size(), 1u);
  EXPECT_EQ(parsed.stats.lost[0].reporter_participant_prefix, "P2");
  EXPECT_EQ(parsed.stats.lost[0].src_participant_prefix, "P1");
  EXPECT_TRUE(parsed.stats.enabled);
  EXPECT_EQ(parsed.stats.samples, 5u);
  EXPECT_EQ(parsed.stats.samples_lost, 7u);
  EXPECT_EQ(parsed.stats.samples_lost_at_start, 3u);
  EXPECT_EQ(parsed.stats.samples_rejected, 1u);
  EXPECT_EQ(parsed.stats.writers_incompatible_qos, 2u);
  EXPECT_EQ(parsed.stats.writers_announced, 16u);
  EXPECT_EQ(parsed.stats.writers_heard, 16u);
  EXPECT_EQ(parsed.stats.measured_instances, 640u);
  EXPECT_TRUE(parsed.stats.settled);
  EXPECT_DOUBLE_EQ(parsed.stats.settled_at_s, 6.25);
  EXPECT_EQ(parsed.stats.samples_lost_latency, 4u);
  EXPECT_EQ(parsed.stats.pairs_delivered, 9u);
  EXPECT_EQ(parsed.stats.pairs_delivered_unmeasured, 5u);
  EXPECT_EQ(parsed.stats.pairs_delivered_absent, 2u);
  EXPECT_EQ(parsed.stats.warnings, std::vector<std::string>{"stats-samples-lost"});
  EXPECT_EQ(parsed.stats.participants_with_stats, std::set<std::string>{"P1"});
  EXPECT_TRUE(parsed.shm.available);
  EXPECT_EQ(parsed.shm.warnings, std::vector<std::string>{"shm-stale-files"});
  // #125: the participants come back as written (the `again` comparison below covers the rest)
  ASSERT_EQ(parsed.participants.size(), 4u);
  EXPECT_EQ(parsed.participants[0].shm_visibility, ShmVisibility::Visible);
  EXPECT_EQ(parsed.participants[0].host_name, "robot:1");
  ASSERT_EQ(parsed.participants[0].shm_ports.size(), 2u);
  EXPECT_EQ(parsed.participants[0].shm_ports[1].port, 7001u);
  EXPECT_EQ(parsed.participants[0].shm_ports[1].lock, PortLock::Held);
  EXPECT_EQ(parsed.participants[0].shm_ports[1].announced_by, 2u);
  EXPECT_FALSE(parsed.participants[0].shm_ports[1].proof);
  EXPECT_EQ(parsed.participants[1].shm_visibility, ShmVisibility::NotVisible);
  EXPECT_EQ(parsed.participants[1].shm_ports[0].lock, PortLock::Absent);
  EXPECT_TRUE(parsed.participants[2].own);
  EXPECT_EQ(parsed.participants[2].shm_ports[0].lock, PortLock::Own);
  // both renderers produce the same output from the parsed snapshot
  std::string where;
  const json again = json::parse(render_json(parsed, RenderOptions{}));
  EXPECT_TRUE(close(json::parse(text), again, "$", &where)) << where;
  RenderOptions verbose;
  verbose.verbose = true;
  verbose.explain = true;
  verbose.locators = true;
  EXPECT_EQ(render_table(parsed, verbose), render_table(s, verbose));
  // the pair states compare equal, so a diff of a document with itself is empty
  EXPECT_TRUE(diff(pair_states(s), pair_states(parsed)).empty());
}

TEST(ParseJson, ReadsOldRtpsLostKeysAndUnknownLostPackets)
{
  auto doc = json::parse(render_json(snapshot(), RenderOptions{}));
  // a document written before #122: other key names, no reporter
  auto & l = doc["stats"]["lost"][0];
  l["receiver_participant_guid_prefix"] = l["src_participant_guid_prefix"];
  l["from_locator"] = l["dst_locator"];
  l.erase("src_participant_guid_prefix");
  l.erase("dst_locator");
  l.erase("reporter_participant_guid_prefix");
  // and the reader's participant without RTPS_LOST
  doc["topics"][0]["pairs"][0]["measured"]["reliability"]["lost_packets"] = nullptr;
  doc["topics"][0]["lost_packets"] = nullptr;
  auto parsed = parse_json(doc.dump());
  ASSERT_EQ(parsed.stats.lost.size(), 1u);
  EXPECT_EQ(parsed.stats.lost[0].src_participant_prefix, "P1");
  EXPECT_EQ(parsed.stats.lost[0].dst.port, 7411u);
  EXPECT_EQ(parsed.stats.lost[0].reporter_participant_prefix, "");
  const auto & rel = parsed.topics[0].pairs[0].measured.reliability;
  EXPECT_TRUE(rel.available);
  EXPECT_FALSE(rel.lost_available);
  EXPECT_EQ(rel.resent, 2u);
  EXPECT_TRUE(parsed.topics[0].reliability_available);
  EXPECT_FALSE(parsed.topics[0].lost_available);
  EXPECT_EQ(parsed.topics[0].resent, 2u);
}

TEST(ParseJson, ReadsTheLastDocumentOfJsonLinesAndIgnoresChanges)
{
  auto s = snapshot();
  s.has_changes = true;
  s.changes.added.push_back(PairKey{"/x", "W9", "R9"});
  RenderOptions compact;
  compact.compact = true;
  auto first = render_json(s, compact);
  s.observed_at = "2026-09-06T00:00:02Z";
  auto second = render_json(s, compact);
  size_t documents = 0;
  auto parsed = parse_json(first + second + "\n", &documents);
  EXPECT_EQ(documents, 2u);
  EXPECT_EQ(parsed.observed_at, "2026-09-06T00:00:02Z");
  EXPECT_FALSE(parsed.has_changes);
  EXPECT_TRUE(parsed.changes.empty());
  parsed = parse_json(first, &documents);
  EXPECT_EQ(documents, 1u);
}

TEST(ParseJson, ReadsTheRmwUnknownNodeNameAsEmpty)
{
  // documents written before #112 carry the rmw's name for a node it knows nothing about
  auto doc = json::parse(render_json(snapshot(), RenderOptions{}));
  doc["topics"][0]["writers"][0]["node"] = "_NODE_NAMESPACE_UNKNOWN_/_NODE_NAME_UNKNOWN_";
  doc["topics"][0]["pairs"][0]["writer_node"] = "_NODE_NAMESPACE_UNKNOWN_/_NODE_NAME_UNKNOWN_";
  const auto parsed = parse_json(doc.dump());
  ASSERT_EQ(parsed.topics[0].pairs.size(), 1u);
  EXPECT_EQ(parsed.topics[0].pairs[0].writer->node_name, "");
  EXPECT_EQ(parsed.topics[0].pairs[0].reader->node_name, "/listener");
}

TEST(ParseJson, RejectsForeignDocuments)
{
  EXPECT_THROW(parse_json("not json"), ParseError);
  EXPECT_THROW(parse_json("[1, 2]"), ParseError);
  EXPECT_THROW(parse_json("{\"schema_version\": 2}"), ParseError);
  auto s = snapshot();
  auto doc = json::parse(render_json(s, RenderOptions{}));
  doc.erase("topics");
  EXPECT_THROW(parse_json(doc.dump()), ParseError);
  doc = json::parse(render_json(s, RenderOptions{}));
  doc["topics"][0]["pairs"][0]["transport"] = "CARRIER_PIGEON";
  EXPECT_THROW(parse_json(doc.dump()), ParseError);
  doc = json::parse(render_json(s, RenderOptions{}));
  doc["topics"][0]["pairs"][0]["writer_guid"] = "unknown";
  EXPECT_THROW(parse_json(doc.dump()), ParseError);
  try {
    parse_json("{\"schema_version\": 2}");
  } catch (const ParseError & e) {
    EXPECT_NE(std::string(e.what()).find("schema_version 2"), std::string::npos) << e.what();
  }
}

TEST(ParseJson, KeepsTheBufferParentOfNativeBufferCompanions)
{
  auto s = snapshot();
  Endpoint companion = ep(true, "W3", "/talker");
  companion.dds_topic = "rt/chatter/_buf_cpu";
  companion.ros_topic = "/chatter/_buf_cpu";
  companion.buffer_parent_guid = "W1";
  s.endpoints.push_back(companion);
  s.endpoints[0].buffer_companion_guids = {"W3"};
  s.topics = summarize(s.endpoints);   // the push invalidated the topics' pointers
  apply_stats(s.topics, s.stats);
  const auto text = render_json(s, RenderOptions{});
  const json doc = json::parse(text);
  ASSERT_EQ(doc["topics"].size(), 2u);
  EXPECT_FALSE(doc["topics"][0]["writers"][0].contains("buffer_parent_guid"));   // /chatter
  EXPECT_EQ(doc["topics"][1]["writers"][0]["buffer_parent_guid"], "W1");

  auto parsed = parse_json(text);
  ASSERT_EQ(parsed.topics.size(), 2u);
  EXPECT_EQ(parsed.topics[1].writers[0]->buffer_parent_guid, "W1");
  EXPECT_EQ(parsed.topics[0].writers[0]->buffer_companion_guids, std::vector<std::string>{"W3"});
  EXPECT_TRUE(in_default_view(parsed.topics[0]));
  EXPECT_FALSE(in_default_view(parsed.topics[1]));
  std::string where;
  EXPECT_TRUE(close(doc, json::parse(render_json(parsed, RenderOptions{})), "$", &where)) << where;
}

TEST(RenderJson, DiscoveryCompleteIsNullWhenNothingCouldBeJudged)
{
  auto s = snapshot();
  s.discovery.complete.reset();   // no ros_discovery_info sample from a live participant
  const auto text = render_json(s, RenderOptions{});
  EXPECT_TRUE(json::parse(text)["discovery"]["complete"].is_null());
  EXPECT_FALSE(parse_json(text).discovery.complete.has_value());
}

TEST(ParseJson, ADocumentWithoutTheDiscoveryObjectLeavesItUnset)
{
  // written before the field existed (#133): nothing is known about its completeness
  auto s = snapshot();
  auto doc = json::parse(render_json(s, RenderOptions{}));
  doc.erase("discovery");
  const auto parsed = parse_json(doc.dump());
  EXPECT_FALSE(parsed.discovery.complete.has_value());
  EXPECT_EQ(parsed.discovery.stopped_on, "");
}

TEST(ParseJson, ADocumentWithoutParticipantsRendersNone)
{
  // written before #125: nothing is known about the participants' ports, and the
  // re-rendered document (`diff`) does not invent an empty section
  auto s = snapshot();
  auto doc = json::parse(render_json(s, RenderOptions{}));
  doc.erase("participants");
  doc["shm"].erase("unknown_ports");
  const auto parsed = parse_json(doc.dump());
  EXPECT_TRUE(parsed.participants.empty());
  EXPECT_TRUE(parsed.shm.unknown_ports.empty());
  EXPECT_FALSE(json::parse(render_json(parsed, RenderOptions{})).contains("participants"));
}

TEST(ParseJson, EndpointsWithoutSegmentVisibilityReadAsUnprobed)
{
  // written before #163, or a value this version does not know
  auto s = snapshot();
  auto doc = json::parse(render_json(s, RenderOptions{}));
  doc["topics"][0]["writers"][0].erase("datasharing_segment_visibility");
  doc["topics"][0]["readers"][0]["datasharing_segment_visibility"] = "later";
  const auto parsed = parse_json(doc.dump());
  for (const auto & e : parsed.endpoints) {
    EXPECT_EQ(e.datasharing_segment_visibility, ShmVisibility::Unprobed) << e.guid;
  }
  EXPECT_TRUE(parsed.endpoints[0].datasharing_history_available);
  const auto again = json::parse(render_json(parsed, RenderOptions{}));
  EXPECT_EQ(again["topics"][0]["writers"][0]["datasharing_segment_visibility"], "unprobed");
}

TEST(ParseJson, EndpointsWithoutATypeHashReadAsEmpty)
{
  // written before #85, or by a tool on ROS 2 Humble, whose rmw announces no type hash
  auto s = snapshot();
  auto doc = json::parse(render_json(s, RenderOptions{}));
  doc["topics"][0]["writers"][0].erase("type_hash");
  const auto parsed = parse_json(doc.dump());
  EXPECT_EQ(parsed.endpoints[0].type_hash, "");
  EXPECT_EQ(parsed.endpoints[1].type_hash, "RIHS01_" + std::string(64, 'a'));
  const auto again = json::parse(render_json(parsed, RenderOptions{}));
  EXPECT_EQ(again["topics"][0]["writers"][0]["type_hash"], "");
}

TEST(ParseJson, EndpointsWithoutATypeInformationHashReadAsEmpty)
{
  // written before #206, or by a tool on Fast DDS 2.x, which announces no TypeInformation
  auto s = snapshot();
  auto doc = json::parse(render_json(s, RenderOptions{}));
  doc["topics"][0]["writers"][0].erase("type_information_hash");
  const auto parsed = parse_json(doc.dump());
  EXPECT_EQ(parsed.endpoints[0].type_information_hash, "");
  EXPECT_EQ(parsed.endpoints[1].type_information_hash, std::string(28, '1'));
  const auto again = json::parse(render_json(parsed, RenderOptions{}));
  EXPECT_EQ(again["topics"][0]["writers"][0]["type_information_hash"], "");
}

TEST(ParseJson, EndpointsWithoutAMinimalTypeInformationHashReadAsEmpty)
{
  // written before #213
  auto s = snapshot();
  auto doc = json::parse(render_json(s, RenderOptions{}));
  doc["topics"][0]["writers"][0].erase("type_information_minimal_hash");
  const auto parsed = parse_json(doc.dump());
  EXPECT_EQ(parsed.endpoints[0].type_information_minimal_hash, "");
  EXPECT_EQ(parsed.endpoints[0].type_information_hash, std::string(28, '1'));
  EXPECT_EQ(parsed.endpoints[1].type_information_minimal_hash, std::string(28, '3'));
  const auto again = json::parse(render_json(parsed, RenderOptions{}));
  EXPECT_EQ(again["topics"][0]["writers"][0]["type_information_minimal_hash"], "");
}

TEST(ParseJson, ParticipantsWithUnknownValuesReadAsUnprobed)
{
  // a newer document with a lock state or visibility this version does not know
  auto s = snapshot();
  auto doc = json::parse(render_json(s, RenderOptions{}));
  doc["participants"][0]["shm_visibility"] = "later";
  doc["participants"][0]["shm_ports"][0]["lock"] = "later";
  const auto parsed = parse_json(doc.dump());
  EXPECT_EQ(parsed.participants[0].shm_visibility, ShmVisibility::Unprobed);
  EXPECT_EQ(parsed.participants[0].shm_ports[0].lock, PortLock::Unprobed);
  doc["participants"][0].erase("guid_prefix");
  EXPECT_THROW(parse_json(doc.dump()), ParseError);
}

// ---- service and action grouping (#84)

namespace
{
/// A service endpoint, spelled as rmw_fastrtps announces one.
Endpoint service_ep(
  bool writer, const std::string & guid, const std::string & node,
  const std::string & dds_topic, const std::string & dds_type, const std::string & prefix)
{
  Endpoint e = ep(writer, guid, node);
  e.participant_guid_prefix = prefix;
  e.dds_topic = dds_topic;
  e.dds_type = dds_type;
  e.ros_topic = "/add_two_ints";
  e.ros_type = dds_type.find("Request") != std::string::npos ?
    "example_interfaces/srv/AddTwoInts_Request" :
    "example_interfaces/srv/AddTwoInts_Response";
  return e;
}

Snapshot service_snapshot()
{
  Snapshot s;
  s.domain = 3;
  s.observed_at = "2026-09-06T00:00:00Z";
  s.local_host_id = {1, 2, 3, 4};
  const std::string req_type = "example_interfaces::srv::dds_::AddTwoInts_Request_";
  const std::string res_type = "example_interfaces::srv::dds_::AddTwoInts_Response_";
  s.endpoints.push_back(
    service_ep(true, "W1", "/caller", "rq/add_two_intsRequest", req_type, "P1"));
  s.endpoints.push_back(
    service_ep(false, "R1", "/server", "rq/add_two_intsRequest", req_type, "P2"));
  s.endpoints.push_back(
    service_ep(true, "W2", "/server", "rr/add_two_intsReply", res_type, "P2"));
  s.endpoints.push_back(
    service_ep(false, "R2", "/caller", "rr/add_two_intsReply", res_type, "P1"));
  return s;
}

const TopicSummary & topic_by_dds(const Snapshot & s, const std::string & dds)
{
  for (const auto & t : s.topics) {
    if (t.dds_topic == dds) {return t;}
  }
  throw std::runtime_error("no topic " + dds);
}
}  // namespace

TEST(RenderJson, TopicsCarryTheirKindGroupAndDirection)
{
  auto s = service_snapshot();
  s.topics = summarize(s.endpoints);
  const auto doc = json::parse(render_json(s, RenderOptions{}));
  std::map<std::string, json> by_dds;
  for (const auto & t : doc["topics"]) {
    const std::string dds = t["dds_topic"];
    by_dds[dds] = t;
  }
  ASSERT_EQ(by_dds.size(), 2u);
  EXPECT_EQ(by_dds["rq/add_two_intsRequest"]["kind"], "service");
  EXPECT_EQ(by_dds["rq/add_two_intsRequest"]["group"], "/add_two_ints");
  EXPECT_EQ(by_dds["rq/add_two_intsRequest"]["direction"], "to_server");
  EXPECT_EQ(by_dds["rr/add_two_intsReply"]["direction"], "to_client");
  // the raw member topics stay in the document: the table replaces rows, the JSON loses
  // nothing (#84 Q4)
  EXPECT_EQ(by_dds["rr/add_two_intsReply"]["topic"], "/add_two_ints");
}

TEST(ParseJson, RoundTripsTheKindGroupAndDirection)
{
  auto s = service_snapshot();
  s.topics = summarize(s.endpoints);
  const auto parsed = parse_json(render_json(s, RenderOptions{}));
  const auto request = topic_by_dds(parsed, "rq/add_two_intsRequest");
  EXPECT_EQ(request.kind, TopicKind::Service);
  EXPECT_EQ(request.group, "/add_two_ints");
  EXPECT_EQ(request.direction, GroupDirection::ToServer);
  EXPECT_EQ(topic_by_dds(parsed, "rr/add_two_intsReply").direction, GroupDirection::ToClient);
}

TEST(ParseJson, ADocumentWithoutTheGroupingKeysIsClassifiedAgain)
{
  // written before #84: the keys are re-derived from the DDS names, so an old document
  // still groups in diff
  auto s = service_snapshot();
  s.topics = summarize(s.endpoints);
  auto doc = json::parse(render_json(s, RenderOptions{}));
  for (auto & t : doc["topics"]) {
    t.erase("kind");
    t.erase("group");
    t.erase("direction");
  }
  const auto parsed = parse_json(doc.dump());
  const auto request = topic_by_dds(parsed, "rq/add_two_intsRequest");
  EXPECT_EQ(request.kind, TopicKind::Service);
  EXPECT_EQ(request.group, "/add_two_ints");
  EXPECT_EQ(request.direction, GroupDirection::ToServer);
  const auto again = json::parse(render_json(parsed, RenderOptions{}));
  EXPECT_EQ(again["topics"][0]["kind"], "service");
}

TEST(ParseJson, AKindOrDirectionThisVersionDoesNotKnowIsDerivedAgain)
{
  // a newer document whose spelling this build has never heard of: fall back to the rule
  // rather than carry a value nothing can render
  auto s = service_snapshot();
  s.topics = summarize(s.endpoints);
  auto doc = json::parse(render_json(s, RenderOptions{}));
  doc["topics"][0]["kind"] = "later";
  const auto parsed = parse_json(doc.dump());
  EXPECT_EQ(topic_by_dds(parsed, "rq/add_two_intsRequest").kind, TopicKind::Service);
  EXPECT_EQ(topic_by_dds(parsed, "rr/add_two_intsReply").group, "/add_two_ints");
}
