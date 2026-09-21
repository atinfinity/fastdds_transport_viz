// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// render_csv() (#83): the header, RFC 4180 quoting, one row per pair, and every cell the same
// value as the JSON document's field (an empty cell where that field is null).

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/model.hpp"
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

Snapshot snapshot(bool with_stats)
{
  Snapshot s;
  s.domain = 3;
  s.observed_at = "2026-09-06T00:00:00Z";
  s.observation_seconds = 2.5;
  s.local_host_id = {1, 2, 3, 4};
  s.endpoints.push_back(ep(true, "W1", "/talker"));
  s.endpoints.push_back(ep(false, "R1", "/listener"));
  s.topics = summarize(s.endpoints);
  if (with_stats) {
    s.stats.enabled = true;
    s.stats.participants_with_stats = {"P1"};
    s.stats.traffic.push_back(
      TrafficSample{"P1", Locator{LocatorKind::SHM, "", 7413}, 10, 1000.0, 4, 400.0, 3});
    LatencyStat lat;
    lat.add(0.001); lat.add(0.003);
    s.stats.latency[{"W1", "R1"}] = lat;
    s.stats.delivery_window[{"W1", "R1"}] = DeliveryWindow{2, 10.0, 10.5};
    s.stats.resent_datas["W1"] = DataCountSample{1, 3, 2};
    s.stats.lost.push_back(
      TrafficSample{"P1", Locator{LocatorKind::UDPv4, "10.0.0.1", 7411}, 5, 500.0, 5, 500.0, 2,
        "P2"});
    apply_stats(s.topics, s.stats);
  }
  return s;
}

/// RFC 4180 reader for the test: rows of cells, quoted fields may hold commas and quotes.
std::vector<std::vector<std::string>> parse_csv(const std::string & text)
{
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row;
  std::string cell;
  bool quoted = false;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (quoted) {
      if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') {
        cell += '"';
        ++i;
      } else if (c == '"') {
        quoted = false;
      } else {
        cell += c;
      }
    } else if (c == '"') {
      quoted = true;
    } else if (c == ',') {
      row.push_back(cell);
      cell.clear();
    } else if (c == '\n') {
      row.push_back(cell);
      cell.clear();
      rows.push_back(row);
      row.clear();
    } else {
      cell += c;
    }
  }
  EXPECT_TRUE(cell.empty() && row.empty()) << "the last row has no line end";
  return rows;
}

const std::vector<std::string> kColumns{
  "observed_at", "domain", "topic", "type",
  "writer_node", "writer_host", "writer_guid",
  "reader_node", "reader_host", "reader_guid",
  "transport", "confidence", "reasons", "warnings", "measured_transports",
  "packets", "bytes", "delivered_per_s", "delivered_per_s_lower_bound",
  "latency_mean_s", "latency_min_s", "latency_max_s", "latency_last_s",
  "lost_packets", "resent_datas"};

/// The cell a JSON value becomes: null empty, arrays joined with ';', numbers as JSON has them.
std::string cell_of(const json & v)
{
  if (v.is_null()) {return "";}
  if (v.is_string()) {return v.get<std::string>();}
  if (v.is_array()) {
    std::string out;
    for (const auto & item : v) {
      out += (out.empty() ? "" : ";") + item.get<std::string>();
    }
    return out;
  }
  return v.dump();
}

/// What each column should read, taken from the JSON document of the same snapshot.
std::vector<std::string> expected_row(const json & doc, const json & topic, const json & pair)
{
  const json & m = pair["measured"];
  const bool available = m["available"].get<bool>();
  const json & lat = m["latency_s"];
  const json & rel = m["reliability"];
  const json & rate = m["delivered_per_s"];
  return {
    cell_of(doc["observed_at"]), cell_of(doc["domain"]), cell_of(topic["topic"]),
    cell_of(topic["type"]),
    cell_of(pair["writer_node"]), cell_of(pair["writer_host"]), cell_of(pair["writer_guid"]),
    cell_of(pair["reader_node"]), cell_of(pair["reader_host"]), cell_of(pair["reader_guid"]),
    cell_of(pair["transport"]), cell_of(pair["confidence"]), cell_of(pair["reasons"]),
    cell_of(pair["warnings"]), cell_of(m["transports"]),
    available ? cell_of(m["packets"]) : "", available ? cell_of(m["bytes"]) : "",
    cell_of(rate), rate.is_null() ? "" : cell_of(m["delivered_per_s_lower_bound"]),
    lat.is_null() ? "" : cell_of(lat["mean"]), lat.is_null() ? "" : cell_of(lat["min"]),
    lat.is_null() ? "" : cell_of(lat["max"]), lat.is_null() ? "" : cell_of(lat["last"]),
    rel.is_null() ? "" : cell_of(rel["lost_packets"]),
    rel.is_null() ? "" : cell_of(rel["resent_datas"])};
}
}  // namespace

TEST(RenderCsv, HeaderThenOneRowPerPairMatchingTheJsonDocument)
{
  for (bool with_stats : {true, false}) {
    SCOPED_TRACE(with_stats ? "with statistics" : "without statistics");
    Snapshot s = snapshot(with_stats);
    RenderOptions opt;
    const auto rows = parse_csv(render_csv(s, opt));
    const json doc = json::parse(render_json(s, opt));
    std::vector<std::vector<std::string>> expected{kColumns};
    for (const auto & t : doc["topics"]) {
      for (const auto & p : t["pairs"]) {
        expected.push_back(expected_row(doc, t, p));
      }
    }
    ASSERT_EQ(expected.size(), 2u);
    EXPECT_EQ(rows, expected);
  }
}

TEST(RenderCsv, MeasuredValuesAndNullsSpelledOut)
{
  const auto with = parse_csv(render_csv(snapshot(true), RenderOptions{}));
  ASSERT_EQ(with.size(), 2u);
  auto col = [&](const std::vector<std::string> & row, const std::string & name) {
      for (size_t i = 0; i < kColumns.size(); ++i) {
        if (kColumns[i] == name) {return row.at(i);}
      }
      ADD_FAILURE() << name;
      return std::string();
    };
  EXPECT_EQ(col(with[1], "measured_transports"), "SHM");
  EXPECT_EQ(col(with[1], "packets"), "6");   // 10 - 4: the delta over the observation
  EXPECT_EQ(col(with[1], "latency_min_s"), "0.001");
  EXPECT_EQ(col(with[1], "delivered_per_s_lower_bound"), "false");
  EXPECT_EQ(col(with[1], "lost_packets"), "0");
  // without statistics every measured cell is empty, as every JSON value is null
  const auto without = parse_csv(render_csv(snapshot(false), RenderOptions{}));
  ASSERT_EQ(without.size(), 2u);
  for (const char * name : {"packets", "bytes", "delivered_per_s", "delivered_per_s_lower_bound",
      "latency_mean_s", "lost_packets", "resent_datas", "measured_transports"})
  {
    EXPECT_EQ(col(without[1], name), "") << name;
  }
  EXPECT_EQ(col(without[1], "writer_host"), "local");
}

TEST(RenderCsv, QuotesFieldsWithCommasQuotesAndLineBreaks)
{
  Snapshot s = snapshot(false);
  s.endpoints[0].node_name = "/odd,\"name\"";
  s.endpoints[1].node_name = "/two\nlines";
  s.topics = summarize(s.endpoints);
  const std::string out = render_csv(s, RenderOptions{});
  EXPECT_NE(out.find(",\"/odd,\"\"name\"\"\","), std::string::npos) << out;
  EXPECT_NE(out.find(",\"/two\nlines\","), std::string::npos) << out;
  EXPECT_EQ(out.find('\r'), std::string::npos) << "LF line ends";
  const auto rows = parse_csv(out);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[1][4], "/odd,\"name\"");
  EXPECT_EQ(rows[1][7], "/two\nlines");
}

TEST(RenderCsv, HeaderAloneWithoutPairsAndRowsAloneAfterTheFirstFrame)
{
  Snapshot empty;
  empty.observed_at = "2026-09-06T00:00:00Z";
  RenderOptions opt;
  EXPECT_EQ(parse_csv(render_csv(empty, opt)), std::vector<std::vector<std::string>>{kColumns});
  opt.csv_no_header = true;
  EXPECT_EQ(render_csv(empty, opt), "");
  const auto rows = parse_csv(render_csv(snapshot(false), opt));
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0][0], "2026-09-06T00:00:00Z");
}
