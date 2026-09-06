// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/model.hpp"
#include "fastdds_transport_viz/render.hpp"

using namespace fastdds_transport_viz;  // NOLINT

namespace
{
Endpoint ep(bool writer, const std::string & guid, const std::string & node, LocatorKind kind)
{
  Endpoint e;
  e.is_writer = writer;
  e.guid = guid;
  e.node_name = node;
  e.host_id = {1, 2, 3, 4};
  e.dds_topic = "rt/chatter";
  e.dds_type = "std_msgs::msg::dds_::String_";
  e.ros_topic = "/chatter";
  e.ros_type = "std_msgs/msg/String";
  e.unicast.push_back(Locator{kind, kind == LocatorKind::SHM ? "" : "10.0.0.1", 7411});
  e.qos.data_sharing = DataSharingKind::Off;
  return e;
}

Snapshot snapshot()
{
  Snapshot s;
  s.local_host_id = {1, 2, 3, 4};
  s.endpoints.push_back(ep(true, "W1", "/talker", LocatorKind::SHM));
  s.endpoints.push_back(ep(false, "R1", "/listener", LocatorKind::SHM));
  s.topics = summarize(s.endpoints);
  return s;
}
}  // namespace

TEST(VisibleWidth, IgnoresEscapesAndCountsUtf8)
{
  EXPECT_EQ(visible_width("abc"), 3u);
  EXPECT_EQ(visible_width("\033[32mabc\033[0m"), 3u);
  EXPECT_EQ(visible_width("a…b"), 3u);
}

TEST(TruncateVisible, CutsAndKeepsEscapes)
{
  EXPECT_EQ(truncate_visible("abcdef", 0), "abcdef");
  EXPECT_EQ(truncate_visible("abc", 5), "abc");
  auto t = truncate_visible("\033[32mabcdef\033[0m", 4);
  EXPECT_EQ(visible_width(t), 4u);
  EXPECT_NE(t.find("\033[32m"), std::string::npos);
  EXPECT_EQ(t.substr(t.size() - 4), "\033[0m");   // always reset at the end
}

TEST(RenderTable, PlainHasNoEscapes)
{
  auto out = render_table(snapshot(), RenderOptions{});
  EXPECT_EQ(out.find('\033'), std::string::npos);
  EXPECT_NE(out.find("/chatter"), std::string::npos);
  EXPECT_NE(out.find("SHM x1"), std::string::npos);
}

TEST(RenderTable, ColorPaintsTransports)
{
  RenderOptions opt;
  opt.color = true;
  auto out = render_table(snapshot(), opt);
  EXPECT_NE(out.find("\033[32mSHM\033[0m x1"), std::string::npos);
}

TEST(RenderTable, WatchMarksGhostsAndSummary)
{
  auto snap = snapshot();
  WatchDecorations w;
  w.marks[pair_key(snap.topics[0], snap.topics[0].pairs[0])] = '+';
  w.ghosts.push_back(
    GhostPair{PairKey{"/chatter", "W1", "R9"}, "std_msgs/msg/String",
      "/talker@local", "/old_listener@local", "UDPv4"});
  w.ghosts.push_back(
    GhostPair{PairKey{"/gone", "W5", "R5"}, "std_msgs/msg/Int32",
      "/a@local", "/b@local", "SHM"});
  w.summary = "+1 pair  -2 pairs";
  RenderOptions opt;
  opt.verbose = true;
  opt.watch = &w;
  auto out = render_table(snap, opt);
  EXPECT_NE(out.find("+  /chatter"), std::string::npos);          // topic row marked
  EXPECT_NE(out.find("/old_listener@local"), std::string::npos);  // ghost pair under its topic
  EXPECT_NE(out.find("(removed)"), std::string::npos);
  EXPECT_NE(out.find("/gone"), std::string::npos);                // orphan ghost topic row
  EXPECT_NE(out.find("changes: +1 pair  -2 pairs"), std::string::npos);
  EXPECT_EQ(out.find('\033'), std::string::npos);                  // no color requested
}

TEST(RenderTable, MaxWidthTruncatesEveryLine)
{
  RenderOptions opt;
  opt.verbose = true;
  opt.explain = true;
  opt.max_width = 30;
  auto snap = snapshot();
  snap.stats.enabled = true;               // footer lines must be cut as well
  snap.shm.available = true;
  snap.shm.path = "/dev/shm";
  snap.shm.total_bytes = 1000000;
  snap.shm.warnings = {"shm-stale-files"};
  snap.shm.stale_segments = 3;
  WatchDecorations w;
  w.summary = "+1 pair  -1 pair  ~1 changed and a very long summary";
  opt.watch = &w;
  auto out = render_table(snap, opt);
  EXPECT_NE(out.find("shared memory:"), std::string::npos);
  size_t start = 0;
  while (start < out.size()) {
    size_t end = out.find('\n', start);
    if (end == std::string::npos) {end = out.size();}
    EXPECT_LE(visible_width(out.substr(start, end - start)), 30u);
    start = end + 1;
  }
}

// ---- statistics cells, footers and legend ------------------------------------------

namespace
{
Snapshot stats_snapshot()
{
  Snapshot s = snapshot();
  s.stats.enabled = true;
  s.stats.samples = 12;
  s.stats.participants_with_stats.insert("P1");
  s.endpoints[0].participant_guid_prefix = "P1";
  s.topics = summarize(s.endpoints);
  return s;
}
Pair & only_pair(Snapshot & s) {return s.topics[0].pairs[0];}
}  // namespace

TEST(RenderTable, MeasuredCellValues)
{
  RenderOptions opt;
  opt.verbose = true;
  auto s = stats_snapshot();
  // n/a: the writer's participant publishes no statistics
  only_pair(s).measured.available = false;
  EXPECT_NE(render_table(s, opt).find("measured=n/a"), std::string::npos);
  // none / none(delivered)
  only_pair(s).measured.available = true;
  EXPECT_NE(render_table(s, opt).find("measured=none "), std::string::npos);
  only_pair(s).measured.delivered = true;
  EXPECT_NE(render_table(s, opt).find("measured=none(delivered)"), std::string::npos);
  // idle: transports known, no packets in the window
  only_pair(s).measured.transports = {Transport::SHM};
  EXPECT_NE(render_table(s, opt).find("measured=SHM (idle)"), std::string::npos);
  // packets and bytes with SI formatting, RATE column
  only_pair(s).measured.packets = 148;
  only_pair(s).measured.bytes = 7.63e6;
  only_pair(s).measured.throughput_available = true;
  only_pair(s).measured.throughput = 1.31e6;
  s.topics[0].throughput_available = true;
  s.topics[0].throughput = 23.0;
  auto out = render_table(s, opt);
  EXPECT_NE(out.find("measured=SHM 148pkt 7.63 MB"), std::string::npos);
  EXPECT_NE(out.find("1.31 MB/s"), std::string::npos);
  EXPECT_NE(out.find("23 B/s"), std::string::npos);
  EXPECT_NE(out.find("statistics: 12 samples from 1 participant(s)"), std::string::npos);
}

TEST(RenderTable, StatisticsHintWhenNoParticipantPublishes)
{
  auto s = stats_snapshot();
  s.stats.participants_with_stats.clear();
  auto out = render_table(s, RenderOptions{});
  EXPECT_NE(out.find("start the observed nodes with FASTDDS_STATISTICS"), std::string::npos);
}

TEST(RenderTable, SharedMemoryFooterAndWarnings)
{
  auto s = snapshot();
  s.shm.available = true;
  s.shm.path = "/dev/shm";
  s.shm.total_bytes = 16668618752ull;
  s.shm.used_bytes = 396000000ull;
  s.shm.free_bytes = s.shm.total_bytes - s.shm.used_bytes;
  s.shm.fastdds_bytes = 63400000ull;
  s.shm.segments = 114; s.shm.stale_segments = 110;
  s.shm.ports = 14; s.shm.stale_ports = 7;
  s.shm.datasharing_histories = 1; s.shm.datasharing_unmatched = 1;
  s.shm.checked_ports = {7411, 7413}; s.shm.missing_ports = {7413};
  s.shm.other_host_participants = 2;
  s.shm.nodes_visible = false;
  s.shm.warnings = {"shm-not-visible", "shm-stale-files", "shm-nearly-full"};
  RenderOptions opt;
  opt.color = true;
  opt.explain = true;
  auto out = render_table(s, opt);
  EXPECT_NE(
    out.find("shared memory: \033[0m/dev/shm 396 MB used of 16.7 GB (16.3 GB free)"),
    std::string::npos);
  EXPECT_NE(
    out.find(
      "63.4 MB in 114 segment(s) (110 stale), 14 port(s) (7 stale), "
      "1 data-sharing history (1 unmatched)"),
    std::string::npos);
  EXPECT_NE(
    out.find(
      "\033[31m!shm-stale-files\033[0m: 117 file(s) without a living owner, "
      "run 'fastdds shm clean'"),
    std::string::npos);
  EXPECT_NE(
    out.find(
      "!shm-not-visible\033[0m: 1 of 2 SHM port(s) of the nodes not open here "
      "(other IPC namespace), 2 participant(s) on another host id"),
    std::string::npos);
  EXPECT_NE(
    out.find("!shm-nearly-full\033[0m: Fast DDS cannot create segments when /dev/shm is full"),
    std::string::npos);
  // the legend lists the shm warnings and the pair's reason codes
  EXPECT_NE(out.find("Reason codes:"), std::string::npos);
  EXPECT_NE(out.find("  shm-stale-files\n"), std::string::npos);
  EXPECT_NE(out.find("  both-shm-locators\n"), std::string::npos);
  EXPECT_NE(out.find("Legend: '?' after a transport"), std::string::npos);

  s.shm.available = false;
  EXPECT_EQ(render_table(s, RenderOptions{}).find("shared memory:"), std::string::npos);
}

TEST(RenderTable, GhostRowsCarryStatsCellAndEmptyTopicsMessage)
{
  auto s = stats_snapshot();
  WatchDecorations w;
  w.ghosts.push_back(
    GhostPair{PairKey{"/chatter", "W1", "R9"}, "std_msgs/msg/String",
      "/talker@local", "/old@local", "SHM"});
  RenderOptions opt;
  opt.verbose = true;
  opt.watch = &w;
  auto out = render_table(s, opt);
  EXPECT_NE(out.find("/old@local"), std::string::npos);
  EXPECT_NE(out.find("(removed)"), std::string::npos);

  Snapshot empty;
  empty.domain = 7;
  EXPECT_NE(
    render_table(empty, RenderOptions{}).find("(no endpoints discovered in domain 7)"),
    std::string::npos);
}

TEST(RenderTable, HostLabelsAndWarningsInColor)
{
  auto s = snapshot();
  s.endpoints[0].host_id = {5, 5, 5, 5};   // writer on an unnamed other host
  s.endpoints[1].host_id = {9, 9, 9, 9};   // reader on a host known from PHYSICAL_DATA
  s.endpoints[1].host_name = "robot:123456";
  s.endpoints[1].process = "42";
  s.topics = summarize(s.endpoints);
  s.topics[0].pairs[0].verdict.warnings.push_back("some-warning");
  RenderOptions opt;
  opt.verbose = true;
  opt.color = true;
  opt.host_labels["09090909"] = "unused-when-host-name-known";
  auto out = render_table(s, opt);
  EXPECT_NE(out.find("/listener@robot(42)"), std::string::npos);
  EXPECT_NE(out.find("/talker@host:05050505"), std::string::npos);
  EXPECT_NE(out.find("\033[31m!some-warning\033[0m"), std::string::npos) <<
    "a warning painted red";
  opt.host_labels["05050505"] = "named-host";
  EXPECT_NE(render_table(s, opt).find("/talker@named-host"), std::string::npos);
}

TEST(RenderTable, TransportColorsLikelyMarkAndTopicMarkPriority)
{
  Snapshot s;
  s.local_host_id = {1, 2, 3, 4};
  auto w = ep(true, "W1", "/talker", LocatorKind::UDPv6);
  w.unicast.push_back(Locator{LocatorKind::TCPv4, "10.0.0.1", 7411});
  w.unicast.push_back(Locator{LocatorKind::SHM, "", 7411});
  w.qos.data_sharing = DataSharingKind::On;
  w.qos.data_sharing_domains = {1};
  auto r6 = ep(false, "R6", "/l6", LocatorKind::UDPv6);
  r6.host_id = {9, 9, 9, 9};
  auto rt = ep(false, "RT", "/lt", LocatorKind::TCPv4);
  rt.host_id = {8, 8, 8, 8};
  auto rd = ep(false, "RD", "/ld", LocatorKind::SHM);
  rd.qos.data_sharing = DataSharingKind::On;
  rd.qos.data_sharing_domains = {1};
  s.endpoints = {w, r6, rt, rd};
  s.topics = summarize(s.endpoints);
  ASSERT_EQ(s.topics[0].pairs.size(), 3u);
  WatchDecorations deco;
  deco.marks[pair_key(s.topics[0], s.topics[0].pairs[0])] = '~';
  deco.marks[pair_key(s.topics[0], s.topics[0].pairs[1])] = '-';
  RenderOptions opt;
  opt.color = true;
  opt.verbose = true;
  opt.watch = &deco;
  auto out = render_table(s, opt);
  EXPECT_NE(out.find("\033[36mUDPv6\033[0m"), std::string::npos);
  EXPECT_NE(out.find("\033[35mTCPv4\033[0m"), std::string::npos);
  EXPECT_NE(out.find("\033[33mDATA_SHARING?\033[0m"), std::string::npos);   // likely
  EXPECT_NE(out.find("\033[33m~\033[0m"), std::string::npos);               // changed mark
  EXPECT_NE(out.find("\033[2m-\033[0m"), std::string::npos);                // removed mark
  // the topic row carries '~' (no '+' among its pairs)
  auto topic_row = out.substr(out.find("\n", out.find("TOPIC")) + 1);
  EXPECT_EQ(topic_row.find("\033[33m~\033[0m"), 0u);
  deco.marks[pair_key(s.topics[0], s.topics[0].pairs[2])] = '+';
  out = render_table(s, opt);
  topic_row = out.substr(out.find("\n", out.find("TOPIC")) + 1);
  EXPECT_EQ(topic_row.find("\033[32m+\033[0m"), 0u);
}

TEST(RenderTable, LatencyColumn)
{
  RenderOptions opt;
  opt.verbose = true;
  auto s = stats_snapshot();
  auto out = render_table(s, opt);
  EXPECT_NE(out.find("RATE  LATENCY  LOSS  REASON"), std::string::npos);
  EXPECT_NE(out.find("  -  -  "), std::string::npos) << "no values: dashes";
  only_pair(s).measured.latency_available = true;
  only_pair(s).measured.latency.add(0.0004);
  only_pair(s).measured.latency.add(0.0013);
  s.topics[0].latency_available = true;
  s.topics[0].latency = 0.00085;
  out = render_table(s, opt);
  // topic: slowest pair's mean
  EXPECT_NE(out.find("850 µs"), std::string::npos) << out;
  EXPECT_NE(out.find("850 µs (max 1.30 ms)"), std::string::npos) << out;  // pair: mean (max)
  only_pair(s).measured.latency = LatencyStat{};
  only_pair(s).measured.latency.add(-0.002);
  only_pair(s).measured.latency.add(2.5);
  out = render_table(s, opt);
  EXPECT_NE(out.find("1.25 s (max 2.50 s)"), std::string::npos) << out;
  only_pair(s).measured.latency = LatencyStat{};
  only_pair(s).measured.latency.add(-15e-9);
  out = render_table(s, opt);
  EXPECT_NE(out.find("-15.0 ns (max -15.0 ns)"), std::string::npos) << out;
}

TEST(RenderTable, LossColumn)
{
  RenderOptions opt;
  opt.verbose = true;
  auto s = stats_snapshot();
  auto out = render_table(s, opt);
  EXPECT_NE(out.find("LATENCY  LOSS  REASON"), std::string::npos);
  only_pair(s).measured.reliability.available = true;
  s.topics[0].reliability_available = true;
  out = render_table(s, opt);
  // counters present, nothing lost
  EXPECT_NE(out.find("  0  "), std::string::npos) << out;
  only_pair(s).measured.reliability.lost_packets = 3;
  only_pair(s).measured.reliability.resent = 2;
  s.topics[0].lost_packets = 3;
  out = render_table(s, opt);
  EXPECT_NE(out.find("3 lost, 2 resent"), std::string::npos) << out;
  EXPECT_NE(out.find("  3 lost  "), std::string::npos) << out;      // topic row: sums
  only_pair(s).measured.reliability.lost_packets = 0;
  out = render_table(s, opt);
  EXPECT_NE(out.find("  2 resent  "), std::string::npos) << out;
}
