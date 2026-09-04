// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <string>
#include <vector>

#include <gtest/gtest.h>

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
  w.ghosts.push_back(GhostPair{PairKey{"/chatter", "W1", "R9"}, "std_msgs/msg/String",
      "/talker@local", "/old_listener@local", "UDPv4"});
  w.ghosts.push_back(GhostPair{PairKey{"/gone", "W5", "R5"}, "std_msgs/msg/Int32",
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
  opt.max_width = 30;
  auto out = render_table(snapshot(), opt);
  size_t start = 0;
  while (start < out.size()) {
    size_t end = out.find('\n', start);
    if (end == std::string::npos) {end = out.size();}
    EXPECT_LE(visible_width(out.substr(start, end - start)), 30u);
    start = end + 1;
  }
}
