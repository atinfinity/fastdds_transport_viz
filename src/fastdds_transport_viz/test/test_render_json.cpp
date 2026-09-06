// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// render_json(): every documented key, the --watch `changes` object, the `shm` object and
// the compact (JSON Lines) mode. The schema itself is checked by test_json_schema*.

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/model.hpp"
#include "fastdds_transport_viz/render.hpp"

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
  e.unicast.push_back(Locator{LocatorKind::SHM, "", 7411});
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
  s.topics = summarize(s.endpoints);
  s.stats.enabled = true;
  s.stats.samples = 5;
  s.stats.participants_with_stats = {"P1"};
  s.stats.physical["P1"] = HostInfo{"robot:1", "user", "42"};
  s.stats.traffic.push_back(TrafficSample{"P1", Locator{LocatorKind::SHM, "", 7411}, 10, 1000.0, 4, 400.0, 3});
  s.stats.data_count["W1"] = DataCountSample{2, 5, 2};
  s.stats.throughput["W1"] = ThroughputStat{60.0, 20.0, 3};
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
  ASSERT_EQ(doc["topics"].size(), 1u);
  const auto & t = doc["topics"][0];
  EXPECT_EQ(t["topic"], "/chatter");
  EXPECT_EQ(t["throughput_bytes_per_s"], 20.0);
  EXPECT_EQ(t["writers"][0]["datasharing_history_bytes"], 3928);
  EXPECT_TRUE(t["readers"][0]["datasharing_history_bytes"].is_null());
  EXPECT_EQ(t["writers"][0]["host"], "robot");
  EXPECT_EQ(t["writers"][0]["qos"]["data_sharing"], "OFF");
  ASSERT_EQ(t["pairs"].size(), 1u);
  const auto & p = t["pairs"][0];
  EXPECT_EQ(p["transport"], "SHM");
  EXPECT_EQ(p["confidence"], "certain");
  EXPECT_EQ(p["measured"]["transports"], json::array({"SHM"}));
  EXPECT_EQ(p["measured"]["packets"], 6);          // 10 - 4 during the observation
  EXPECT_EQ(p["measured"]["packets_total"], 10);
  EXPECT_EQ(p["measured"]["throughput_bytes_per_s"], 20.0);
  EXPECT_EQ(p["measured"]["data_submessages"], 3);
  EXPECT_FALSE(doc.contains("changes"));
  // statistics block
  EXPECT_EQ(doc["stats"]["samples"], 5);
  EXPECT_EQ(doc["stats"]["physical"]["P1"]["process"], "42");
  EXPECT_EQ(doc["stats"]["traffic"][0]["packets_first"], 4);
  EXPECT_EQ(doc["stats"]["data_count"]["W1"]["last"], 5);
  EXPECT_EQ(doc["stats"]["throughput"]["W1"]["mean"], 20.0);
  // shm block and its warning's description
  EXPECT_EQ(doc["shm"]["available"], true);
  EXPECT_EQ(doc["shm"]["used_bytes"], 40);
  EXPECT_EQ(doc["shm"]["checked_ports"], json::array({7411}));
  EXPECT_EQ(doc["shm"]["warnings"], json::array({"shm-stale-files"}));
  EXPECT_TRUE(doc["reason_code_descriptions"].contains("shm-stale-files"));
  EXPECT_TRUE(doc["reason_code_descriptions"].contains("measured-shm-traffic"));
}

TEST(RenderJson, ShmUnavailableOmitsSizes)
{
  auto s = snapshot();
  s.shm = ShmInfo{};
  s.shm.path = "/dev/shm";
  auto doc = json::parse(render_json(s, RenderOptions{}));
  EXPECT_EQ(doc["shm"]["available"], false);
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
  s.changes.changed.push_back(PairChange{k, from, to});
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
  // pretty mode spans lines
  EXPECT_GT(render_json(s, RenderOptions{}).find('\n'), 0u);
  EXPECT_NE(render_json(s, RenderOptions{}).find("\n  \"changes\""), std::string::npos);
}
