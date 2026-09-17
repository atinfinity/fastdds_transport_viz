// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <sstream>
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
  s.endpoints[1].unicast[0].port = 7413;   // another participant, so another SHM port
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
  // packets and bytes with SI formatting
  only_pair(s).measured.packets = 148;
  only_pair(s).measured.bytes = 7.63e6;
  auto out = render_table(s, opt);
  EXPECT_NE(out.find("measured=SHM 148pkt 7.63 MB"), std::string::npos);
  EXPECT_NE(out.find("statistics: 12 samples from 1 participant(s)"), std::string::npos);
}

TEST(RenderTable, StatisticsFooterCountsWhatTheToolLost)
{
  auto s = stats_snapshot();
  s.stats.samples_lost = 40;
  s.stats.samples_rejected = 2;
  // not the tool falling behind: out of the count and out of the warning
  s.stats.samples_lost_at_start = 900;
  s.stats.pairs_delivered = 40;
  s.stats.pairs_delivered_unmeasured = 3;
  s.stats.warnings = {"stats-samples-lost"};
  auto out = render_table(s, RenderOptions{});
  EXPECT_NE(
    out.find("statistics: 12 samples from 1 participant(s), 42 sample(s) lost"),
    std::string::npos) << out;
  EXPECT_NE(
    out.find(
      "!stats-samples-lost: 3 of 40 pairs with proven deliveries show no measured packet"),
    std::string::npos) << out;
  EXPECT_EQ(out.find("900"), std::string::npos) << out;

  // #152: the pairs no lost sample explains are named by their own warning
  s.stats.pairs_delivered_absent = 2;
  s.stats.warnings = {"stats-samples-lost", "rtps-sent-absent"};
  out = render_table(s, RenderOptions{});
  EXPECT_NE(out.find("!stats-samples-lost: 1 of 40 pairs"), std::string::npos) << out;
  EXPECT_NE(
    out.find(
      "!rtps-sent-absent: 2 of 40 pairs with proven deliveries show no measured packet and "
      "no lost sample explains it"),
    std::string::npos) << out;
  s.stats.pairs_delivered_absent = 0;
  s.stats.warnings = {"stats-samples-lost"};

  // a document written before #147 warns without the count
  s.stats.pairs_delivered_unmeasured = 0;
  out = render_table(s, RenderOptions{});
  EXPECT_NE(
    out.find("!stats-samples-lost: a pair can show no measurement although it carries traffic"),
    std::string::npos) << out;

  // #147: the loss is still reported when it cost no measurement, the warning is not
  s.stats.warnings.clear();
  out = render_table(s, RenderOptions{});
  EXPECT_NE(out.find("42 sample(s) lost"), std::string::npos) << out;
  EXPECT_EQ(out.find("stats-samples-lost"), std::string::npos) << out;

  // nothing lost: no number, no warning line
  s.stats.samples_lost = 0;
  s.stats.samples_rejected = 0;
  s.stats.warnings.clear();
  out = render_table(s, RenderOptions{});
  EXPECT_NE(out.find("statistics: 12 samples from 1 participant(s)"), std::string::npos) << out;
  EXPECT_EQ(out.find("sample(s) lost"), std::string::npos) << out;
  EXPECT_EQ(out.find("stats-samples-lost"), std::string::npos) << out;
}

TEST(RenderTable, StatisticsFooterNamesTheLatencyLossApart)
{
  // #141: best-effort by design, so the number is reported but is not the tool falling behind
  auto s = stats_snapshot();
  s.stats.samples_lost = 1040;
  s.stats.samples_lost_latency = 1000;
  s.stats.samples_rejected = 2;
  auto out = render_table(s, RenderOptions{});
  EXPECT_NE(
    out.find(
      "statistics: 12 samples from 1 participant(s), 42 sample(s) lost, "
      "1000 latency sample(s) lost"),
    std::string::npos) << out;
  // no warning: nothing here says a pair went unmeasured
  EXPECT_EQ(out.find("stats-samples-lost"), std::string::npos) << out;
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
  s.shm.datasharing_notifications = 2;
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
      "1 data-sharing history (1 unmatched), 2 data-sharing notification(s)"),
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
  EXPECT_NE(out.find("LATENCY  LOSS  REASON"), std::string::npos);
  EXPECT_EQ(out.find("RATE"), std::string::npos) << "#137: the RATE column is gone";
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
  only_pair(s).measured.reliability.lost_available = true;
  s.topics[0].reliability_available = true;
  s.topics[0].lost_available = true;
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
  // the reader's participant does not publish RTPS_LOST: only the lost part is unknown
  only_pair(s).measured.reliability.lost_available = false;
  s.topics[0].lost_available = false;
  out = render_table(s, opt);
  EXPECT_NE(out.find("- lost, 2 resent"), std::string::npos) << out;
  EXPECT_NE(out.find("  - lost  "), std::string::npos) << out;      // topic row
}

TEST(RenderTable, LocatorLineShapes)
{
  Snapshot s;
  s.local_host_id = {1, 2, 3, 4};
  auto w = ep(true, "W1", "/talker", LocatorKind::UDPv4);
  auto r = ep(false, "R1", "/listener", LocatorKind::UDPv4);
  r.host_id = {9, 9, 9, 9};                                  // different host: no SHM path
  r.unicast[0] = Locator{LocatorKind::UDPv4, "10.0.0.2", 7413};
  s.endpoints = {w, r};
  s.topics = summarize(s.endpoints);
  ASSERT_EQ(s.topics[0].pairs.size(), 1u);

  RenderOptions opt;
  opt.verbose = true;
  EXPECT_EQ(render_table(s, opt).find("locators:"), std::string::npos) <<
    "the line only appears with --locators";

  // no --stats: the selected locator alone, without the word "selected"
  opt.locators = true;
  EXPECT_NE(render_table(s, opt).find("locators: UDPv4 10.0.0.2:7413\n"), std::string::npos);

  // measured on the selected locator: said once
  auto & m = s.topics[0].pairs[0].measured;
  m.available = true;
  m.transports = {Transport::UDPv4};
  m.locators = {MeasuredLocator{Locator{LocatorKind::UDPv4, "10.0.0.2", 7413}, 55, 5500.0}};
  EXPECT_NE(
    render_table(s, opt).find("locators: UDPv4 10.0.0.2:7413 (selected = measured, 55 pkt)"),
    std::string::npos);

  // measured elsewhere: both sides, with the word "selected"
  m.locators = {MeasuredLocator{Locator{LocatorKind::UDPv4, "192.168.1.5", 7413}, 55, 5500.0}};
  EXPECT_NE(
    render_table(s, opt).find(
      "locators: selected UDPv4 10.0.0.2:7413 | measured UDPv4 192.168.1.5:7413 (55 pkt)"),
    std::string::npos);
}

TEST(RenderTable, LocatorLineForShmDataSharingMulticastAndHiddenLocators)
{
  // An SHM locator names a /dev/shm port rather than an address.
  auto s = snapshot();
  RenderOptions opt;
  opt.verbose = true;
  opt.locators = true;
  EXPECT_NE(render_table(s, opt).find("locators: SHM port 7411\n"), std::string::npos);
  auto & m = s.topics[0].pairs[0].measured;
  m.available = true;
  m.transports = {Transport::SHM};
  m.locators = {MeasuredLocator{Locator{LocatorKind::SHM, "", 7411}, 9, 900.0}};
  EXPECT_NE(
    render_table(s, opt).find("locators: SHM port 7411 (selected = measured, 9 pkt)"),
    std::string::npos);

  // DATA_SHARING carries no locator at all, which the line says rather than staying blank
  auto & v = s.topics[0].pairs[0].verdict;
  v.transport = Transport::DataSharing;
  v.locator = Locator{};
  m.available = false;
  m.transports.clear();
  m.locators.clear();
  EXPECT_NE(
    render_table(s, opt).find("locators: DATA_SHARING (no locator)"), std::string::npos);

  // Fast DDS < 2.10 predicts UDPv4 without ever showing the locator it would use
  v.transport = Transport::UDPv4;
  v.locator = Locator{};
  v.reasons = {"same-host-locators-hidden"};
  EXPECT_NE(
    render_table(s, opt).find("locators: UDPv4 (hidden by Fast DDS < 2.10)"), std::string::npos);

  // a multicast selection is marked, so a group address is not read as the reader's own
  v.reasons.clear();
  v.locator = Locator{LocatorKind::UDPv4, "239.255.0.1", 7400};
  v.locator_multicast = true;
  EXPECT_NE(
    render_table(s, opt).find("locators: UDPv4 239.255.0.1:7400 (multicast)"),
    std::string::npos);

  // NONE has no path at all: no line
  v.transport = Transport::None;
  EXPECT_EQ(render_table(s, opt).find("locators:"), std::string::npos);
}

TEST(RenderTable, LocatorLineDoesNotWidenThePairColumns)
{
  // The line is emitted raw, not as a row, so a long locator must not shift the columns.
  Snapshot s;
  s.local_host_id = {1, 2, 3, 4};
  auto w = ep(true, "W1", "/talker", LocatorKind::UDPv4);
  auto r = ep(false, "R1", "/listener", LocatorKind::UDPv4);
  r.host_id = {9, 9, 9, 9};
  r.unicast[0] = Locator{LocatorKind::UDPv6, "2001:0db8:0000:0000:0000:0000:0000:0001", 7413};
  w.unicast.push_back(Locator{LocatorKind::UDPv6, "::1", 7411});
  s.endpoints = {w, r};
  s.topics = summarize(s.endpoints);

  RenderOptions opt;
  opt.verbose = true;
  const auto without = render_table(s, opt);
  opt.locators = true;
  const auto with = render_table(s, opt);
  for (const auto & line : {std::string("/talker@local -> /listener@host:09090909")}) {
    EXPECT_NE(with.find(line), std::string::npos);
  }
  EXPECT_NE(with.find("locators: UDPv6 2001:0db8"), std::string::npos);
  // every line that is not the new one is unchanged
  std::string stripped;
  std::istringstream in(with);
  for (std::string line; std::getline(in, line); ) {
    if (line.find("locators:") == std::string::npos) {stripped += line + "\n";}
  }
  EXPECT_EQ(stripped, without);
}

TEST(RenderTable, AdviseLinesUnderPairsAndInTheLegend)
{
  // same host, writer with SHM, reader without: UDPv4 with reader-no-shm-locator (has a
  // remedy) next to same-host-guid and common-udpv4-locator (nothing to change)
  Snapshot s;
  s.local_host_id = {1, 2, 3, 4};
  auto w = ep(true, "W1", "/talker", LocatorKind::UDPv4);
  w.unicast.push_back(Locator{LocatorKind::SHM, "", 7411});
  auto r = ep(false, "R1", "/listener_udp", LocatorKind::UDPv4);
  s.endpoints = {w, r};
  s.topics = summarize(s.endpoints);
  ASSERT_EQ(s.topics[0].pairs.size(), 1u);
  ASSERT_EQ(s.topics[0].pairs[0].verdict.transport, Transport::UDPv4);

  RenderOptions opt;
  opt.verbose = true;
  opt.explain = true;
  const auto without = render_table(s, opt);
  EXPECT_EQ(without.find("    fix "), std::string::npos) << "fix lines only appear with --advise";
  EXPECT_EQ(without.find("      fix: "), std::string::npos);

  opt.advise = true;
  const auto with = render_table(s, opt);
  // one line per code with a remedy, under the pair row, prefixed with the code
  const auto pair_fix =
    with.find("    fix reader-no-shm-locator: Enable SHM on the reader's participant");
  ASSERT_NE(pair_fix, std::string::npos) << with;
  EXPECT_LT(with.find("/talker@local -> /listener_udp@local"), pair_fix);
  EXPECT_EQ(with.find("fix same-host-guid"), std::string::npos);
  EXPECT_EQ(with.find("fix common-udpv4-locator"), std::string::npos);
  // the legend carries the remedy under the code, right after its description
  const auto legend = with.find("\nReason codes:\n");
  ASSERT_NE(legend, std::string::npos);
  EXPECT_NE(
    with.find(
      "  reader-no-shm-locator\n      " + explain("reader-no-shm-locator") + "\n      fix: " +
      *remedy("reader-no-shm-locator") + "\n", legend),
    std::string::npos) << with;
  // and no fix line under a code without a remedy
  const std::string shg = "  same-host-guid\n      " + explain("same-host-guid") + "\n";
  ASSERT_NE(with.find(shg, legend), std::string::npos) << with;
  EXPECT_EQ(with.find(shg + "      fix:", legend), std::string::npos) << with;
}

TEST(RenderTable, AdviseLinesForNoneVerdictsAndNotForGhosts)
{
  Snapshot s;
  s.local_host_id = {1, 2, 3, 4};
  auto w = ep(true, "W1", "/talker", LocatorKind::UDPv4);
  auto r = ep(false, "R1", "/listener", LocatorKind::UDPv4);
  w.qos.reliability = "BEST_EFFORT";
  r.qos.reliability = "RELIABLE";
  s.endpoints = {w, r};
  s.topics = summarize(s.endpoints);
  ASSERT_EQ(s.topics[0].pairs[0].verdict.transport, Transport::None);

  RenderOptions opt;
  opt.advise = true;
  opt.verbose = true;
  const auto out = render_table(s, opt);
  EXPECT_NE(
    out.find("fix qos-incompatible-reliability: Offer RELIABLE on the writer"),
    std::string::npos) << out;
  EXPECT_EQ(out.find("fix qos-incompatible:"), std::string::npos) <<
    "the warning has no remedy of its own";

  // a ghost pair (removed, still displayed) is gone: nothing to fix
  WatchDecorations deco;
  deco.ghosts.push_back(
    GhostPair{PairKey{"/chatter", "W1", "R9"}, "std_msgs/msg/String",
      "/talker@local", "/gone@local", "UDPv4"});
  opt.watch = &deco;
  const auto watched = render_table(s, opt);
  size_t fixes = 0;
  for (size_t pos = watched.find("fix qos-incompatible-reliability"); pos != std::string::npos;
    pos = watched.find("fix qos-incompatible-reliability", pos + 1))
  {
    ++fixes;
  }
  EXPECT_EQ(fixes, 1u) << watched;
}

TEST(RenderTable, AdviseLinesDoNotWidenThePairColumns)
{
  Snapshot s;
  s.local_host_id = {1, 2, 3, 4};
  auto w = ep(true, "W1", "/talker", LocatorKind::UDPv4);
  w.unicast.push_back(Locator{LocatorKind::SHM, "", 7411});
  auto r = ep(false, "R1", "/listener_udp", LocatorKind::UDPv4);
  s.endpoints = {w, r};
  s.topics = summarize(s.endpoints);

  RenderOptions opt;
  opt.verbose = true;
  opt.explain = true;
  const auto without = render_table(s, opt);
  opt.advise = true;
  const auto with = render_table(s, opt);
  // every line that is not a fix line is unchanged
  std::string stripped;
  std::istringstream in(with);
  for (std::string line; std::getline(in, line); ) {
    if (line.find("    fix ") == std::string::npos &&
      line.find("      fix: ") == std::string::npos)
    {
      stripped += line + "\n";
    }
  }
  EXPECT_EQ(stripped, without);
}

// ---- decorations of a two-snapshot comparison (transport_viz diff) ---------------------

TEST(ChangesSummary, CountsAndPlurals)
{
  Changes c;
  EXPECT_EQ(changes_summary(c), "none");
  c.added.push_back(PairKey{"/a", "w", "r"});
  EXPECT_EQ(changes_summary(c), "+1 pair");
  c.added.push_back(PairKey{"/b", "w", "r"});
  c.removed.push_back(PairKey{"/c", "w", "r"});
  EXPECT_EQ(changes_summary(c), "+2 pairs  -1 pair");
  c.changed.push_back(PairChange{PairKey{"/d", "w", "r"}, PairState{}, PairState{}});
  EXPECT_EQ(changes_summary(c), "+2 pairs  -1 pair  ~1 changed");
}

TEST(DecorationsFor, MarksAddedAndChangedAndGhostsRemovedFromBefore)
{
  auto before = snapshot();
  before.endpoints.push_back(ep(false, "R2", "/gone", LocatorKind::SHM));
  before.topics = summarize(before.endpoints);
  auto after = snapshot();
  Changes c = diff_snapshots(before, after, KeyMode::Guid);
  ASSERT_EQ(c.removed.size(), 1u);
  c.changed.push_back(
    PairChange{pair_key(after.topics[0], after.topics[0].pairs[0]), PairState{}, PairState{}});
  RenderOptions opt;
  auto deco = decorations_for(c, before, opt);
  EXPECT_EQ(deco.marks.at(pair_key(after.topics[0], after.topics[0].pairs[0])), '~');
  ASSERT_EQ(deco.ghosts.size(), 1u);
  EXPECT_EQ(deco.ghosts[0].reader_label, "/gone@local");
  EXPECT_EQ(deco.ghosts[0].transport_label, "SHM");
  EXPECT_EQ(deco.summary, "-1 pair  ~1 changed");
  opt.verbose = true;
  opt.watch = &deco;
  auto out = render_table(after, opt);
  EXPECT_NE(out.find("~  /chatter"), std::string::npos);
  EXPECT_NE(out.find("/gone@local"), std::string::npos);
  EXPECT_NE(out.find("(removed)"), std::string::npos);
  EXPECT_NE(out.find("changes: -1 pair  ~1 changed"), std::string::npos);
}

TEST(KeepChangedTopics, DropsTopicsWithoutAMarkOrGhost)
{
  auto snap = snapshot();
  snap.endpoints.push_back(ep(true, "W2", "/other_pub", LocatorKind::SHM));
  snap.endpoints.push_back(ep(false, "R2", "/other_sub", LocatorKind::SHM));
  snap.endpoints[2].ros_topic = snap.endpoints[3].ros_topic = "/other";
  snap.endpoints[2].dds_topic = snap.endpoints[3].dds_topic = "rt/other";
  snap.topics = summarize(snap.endpoints);
  ASSERT_EQ(snap.topics.size(), 2u);
  WatchDecorations deco;
  keep_changed_topics(snap, deco);
  EXPECT_TRUE(snap.topics.empty());
  snap.topics = summarize(snap.endpoints);
  deco.marks[pair_key(snap.topics[1], snap.topics[1].pairs[0])] = '+';
  keep_changed_topics(snap, deco);
  ASSERT_EQ(snap.topics.size(), 1u);
  EXPECT_EQ(snap.topics[0].display_topic, "/other");
  // a ghost keeps its topic even without a mark
  snap.topics = summarize(snap.endpoints);
  deco.marks.clear();
  deco.ghosts.push_back(GhostPair{PairKey{"/chatter", "W1", "R9"}, "", "", "", ""});
  keep_changed_topics(snap, deco);
  ASSERT_EQ(snap.topics.size(), 1u);
  EXPECT_EQ(snap.topics[0].display_topic, "/chatter");
}

TEST(WatchState, GhostsComeFromTheFrameKeptByMove)
{
  RenderOptions opt;
  WatchState ws;
  auto first = snapshot();
  first.endpoints.push_back(ep(false, "R2", "/gone", LocatorKind::SHM));
  first.topics = summarize(first.endpoints);
  const Endpoint * writer_before = first.topics[0].pairs[0].writer;
  ws.diff(first, opt);
  EXPECT_EQ(ws.deco.summary, "first frame");
  EXPECT_TRUE(first.changes.removed.empty());
  ws.keep(std::move(first));
  // the summaries of the kept frame still point into its own endpoints (#135: the frame is
  // no longer copied and summarized again)
  ASSERT_EQ(ws.last_snapshot.topics.size(), 1u);
  EXPECT_EQ(ws.last_snapshot.topics[0].pairs[0].writer, writer_before);
  EXPECT_EQ(ws.last_snapshot.topics[0].pairs[0].writer, &ws.last_snapshot.endpoints[0]);

  auto second = snapshot();
  ws.diff(second, opt);
  ASSERT_EQ(second.changes.removed.size(), 1u);
  ASSERT_EQ(ws.deco.ghosts.size(), 1u);
  EXPECT_EQ(ws.deco.ghosts[0].reader_label, "/gone@local");
  EXPECT_EQ(ws.deco.ghosts[0].transport_label, "SHM");
  EXPECT_EQ(ws.deco.summary, "-1 pair");
  ws.keep(std::move(second));

  // the ghost is held for kHoldFrames frames, and a pair that comes back drops it at once
  auto third = snapshot();
  ws.diff(third, opt);
  EXPECT_EQ(ws.deco.ghosts.size(), 1u);
  EXPECT_EQ(ws.deco.summary, "none");
  ws.keep(std::move(third));
  auto fourth = snapshot();
  fourth.endpoints.push_back(ep(false, "R2", "/gone", LocatorKind::SHM));
  fourth.topics = summarize(fourth.endpoints);
  ws.diff(fourth, opt);
  EXPECT_TRUE(ws.deco.ghosts.empty());
  EXPECT_EQ(ws.deco.marks.count(PairKey{"/chatter", "W1", "R2"}), 1u);
}
