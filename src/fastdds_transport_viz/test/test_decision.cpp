// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/model.hpp"
#include "fastdds_transport_viz/ros_names.hpp"

using namespace fastdds_transport_viz;  // NOLINT

namespace
{

Locator udp4(const std::string & ip, uint32_t port = 7411)
{
  return Locator{LocatorKind::UDPv4, ip, port};
}
Locator shm(uint32_t port = 7411)
{
  return Locator{LocatorKind::SHM, "", port};
}

Endpoint make(
  bool writer, HostId host, std::vector<Locator> unicast,
  DataSharingKind ds = DataSharingKind::Off, std::vector<uint64_t> domains = {})
{
  static int counter = 0;
  Endpoint e;
  e.is_writer = writer;
  e.host_id = host;
  e.guid = (writer ? "w" : "r") + std::to_string(counter++);
  e.dds_topic = "rt/chatter";
  e.dds_type = "std_msgs::msg::dds_::String_";
  e.ros_topic = "/chatter";
  e.ros_type = "std_msgs/msg/String";
  e.unicast = std::move(unicast);
  e.qos.data_sharing = ds;
  e.qos.data_sharing_domains = std::move(domains);
  return e;
}

bool has(const std::vector<std::string> & v, const std::string & s)
{
  return std::find(v.begin(), v.end(), s) != v.end();
}

const HostId HOST_A{0x01, 0x0f, 0xaa, 0xbb};
const HostId HOST_B{0x01, 0x0f, 0xcc, 0xdd};

}  // namespace

TEST(Decision, SameHostBothShmGivesShm)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1"), shm()});
  auto r = make(false, HOST_A, {udp4("10.0.0.1"), shm()});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::SHM);
  EXPECT_EQ(v.confidence, Confidence::Certain);
  EXPECT_TRUE(has(v.reasons, "same-host-guid"));
  EXPECT_TRUE(has(v.reasons, "both-shm-locators"));
  EXPECT_TRUE(has(v.reasons, "datasharing-disabled-writer"));
  EXPECT_TRUE(v.warnings.empty());
}

TEST(Decision, SameHostReaderWithoutShmFallsBackToUdp)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1"), shm()});
  auto r = make(false, HOST_A, {udp4("10.0.0.1")});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::UDPv4);
  EXPECT_TRUE(has(v.reasons, "reader-no-shm-locator"));
  EXPECT_FALSE(has(v.reasons, "writer-no-shm-locator"));
  EXPECT_TRUE(has(v.reasons, "common-udpv4-locator"));
}

TEST(Decision, SameHostWriterWithoutShmFallsBackToUdp)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1")});
  auto r = make(false, HOST_A, {udp4("10.0.0.1"), shm()});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::UDPv4);
  EXPECT_TRUE(has(v.reasons, "writer-no-shm-locator"));
}

TEST(Decision, DifferentHostIgnoresShm)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1"), shm()});
  auto r = make(false, HOST_B, {udp4("10.0.0.2"), shm()});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::UDPv4);
  EXPECT_TRUE(has(v.reasons, "different-host"));
  EXPECT_TRUE(has(v.reasons, "shm-locators-ignored-across-hosts"));
  EXPECT_FALSE(has(v.reasons, "same-host-guid"));
}

TEST(Decision, NoCommonTransport)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1")});
  auto r = make(false, HOST_B, {Locator{LocatorKind::UDPv6, "::1", 7411}});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_TRUE(has(v.reasons, "no-common-transport"));
}

TEST(Decision, ReaderLocatorOrderDecidesNetworkKind)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1"), Locator{LocatorKind::UDPv6, "::1", 7411}});
  auto r = make(false, HOST_B, {Locator{LocatorKind::UDPv6, "::2", 7411}, udp4("10.0.0.2")});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::UDPv6);
}

TEST(Decision, DataSharingLikelyWhenBothAnnounceMatchingDomains)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1"), shm()}, DataSharingKind::Auto, {42});
  auto r = make(false, HOST_A, {udp4("10.0.0.1"), shm()}, DataSharingKind::Auto, {42});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::DataSharing);
  EXPECT_EQ(v.confidence, Confidence::Likely);
  EXPECT_TRUE(has(v.reasons, "datasharing-qos-enabled-both"));
  EXPECT_TRUE(has(v.reasons, "datasharing-domain-ids-match"));
  EXPECT_TRUE(has(v.reasons, "datasharing-unverified-by-traffic"));
}

TEST(Decision, DataSharingDomainMismatchFallsBackToShm)
{
  auto w = make(true, HOST_A, {shm()}, DataSharingKind::On, {1});
  auto r = make(false, HOST_A, {shm()}, DataSharingKind::On, {2});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::SHM);
  EXPECT_TRUE(has(v.reasons, "datasharing-domain-ids-mismatch"));
}

TEST(Decision, DataSharingOffOnReaderGivesShm)
{
  auto w = make(true, HOST_A, {shm()}, DataSharingKind::Auto, {1});
  auto r = make(false, HOST_A, {shm()}, DataSharingKind::Off);
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::SHM);
  EXPECT_TRUE(has(v.reasons, "datasharing-disabled-reader"));
}

TEST(Decision, DataSharingNotUsedAcrossHosts)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1")}, DataSharingKind::Auto, {1});
  auto r = make(false, HOST_B, {udp4("10.0.0.2")}, DataSharingKind::Auto, {1});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::UDPv4);
}

TEST(Decision, SameHostIdButDifferentIpWarns)
{
  auto w = make(true, HOST_A, {udp4("172.18.0.2"), shm()});
  auto r = make(false, HOST_A, {udp4("172.18.0.3"), shm()});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::SHM);
  EXPECT_TRUE(has(v.warnings, "host-id-match-but-ip-differs"));
}

TEST(Decision, EveryEmittedCodeHasAnExplanation)
{
  for (const auto & code : known_codes()) {
    EXPECT_NE(explain(code), "(no description)") << code;
  }
  EXPECT_EQ(explain("nonexistent-code"), "(no description)");
}

TEST(Summarize, PairsWritersWithReadersAndFlagsUnmatched)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm()}));
  eps.push_back(make(false, HOST_A, {shm()}));
  eps.push_back(make(false, HOST_B, {udp4("10.0.0.2")}));
  Endpoint lonely = make(true, HOST_A, {shm()});
  lonely.dds_topic = "rt/lonely";
  lonely.ros_topic = "/lonely";
  eps.push_back(lonely);
  Endpoint mismatch = make(false, HOST_A, {shm()});
  mismatch.dds_type = "std_msgs::msg::dds_::Int32_";
  eps.push_back(mismatch);

  auto topics = summarize(eps);
  ASSERT_EQ(topics.size(), 2u);
  EXPECT_EQ(topics[0].display_topic, "/chatter");
  EXPECT_EQ(topics[0].writers.size(), 1u);
  EXPECT_EQ(topics[0].readers.size(), 3u);
  EXPECT_EQ(topics[0].pairs.size(), 2u);   // the Int32 reader is not paired
  EXPECT_TRUE(has(topics[0].unmatched_reasons, "type-name-mismatch"));
  EXPECT_EQ(topics[1].display_topic, "/lonely");
  EXPECT_TRUE(topics[1].pairs.empty());
  EXPECT_TRUE(has(topics[1].unmatched_reasons, "no-matching-reader"));
}

TEST(FilterByNode, KeepsMatchingNodesTheirPartnersAndUnpairedEndpoints)
{
  auto ep = [](bool writer, const std::string & topic, const std::string & node) {
      Endpoint e = make(writer, HOST_A, {shm(), udp4("127.0.0.1")});
      e.dds_topic = "rt" + topic;
      e.ros_topic = topic;
      e.node_name = node;
      return e;
    };
  std::vector<Endpoint> eps;
  eps.push_back(ep(true, "/chatter", "/talker"));      // 0
  eps.push_back(ep(false, "/chatter", "/listener"));   // 1
  eps.push_back(ep(false, "/chatter", "/other"));      // 2
  eps.push_back(ep(true, "/solo", "/listener"));       // 3: no reader
  eps.push_back(ep(true, "/x", "/foo"));               // 4
  eps.push_back(ep(false, "/x", "/bar"));              // 5
  eps.push_back(ep(false, "/raw", ""));                // 6: no node name
  auto topics = summarize(eps);
  ASSERT_EQ(topics.size(), 4u);

  filter_by_node(topics, [](const Endpoint & e) {return e.node_name == "/listener";});

  ASSERT_EQ(topics.size(), 2u);
  const auto & chatter = topics[0];
  EXPECT_EQ(chatter.display_topic, "/chatter");
  ASSERT_EQ(chatter.pairs.size(), 1u);
  EXPECT_EQ(chatter.pairs[0].writer, &eps[0]);       // partner kept
  EXPECT_EQ(chatter.pairs[0].reader, &eps[1]);
  EXPECT_EQ(chatter.writers.size(), 1u);
  EXPECT_EQ(chatter.readers.size(), 1u);             // /other dropped
  EXPECT_TRUE(chatter.unmatched_reasons.empty());

  const auto & solo = topics[1];
  EXPECT_EQ(solo.display_topic, "/solo");
  EXPECT_TRUE(solo.pairs.empty());
  EXPECT_EQ(solo.writers.size(), 1u);
  EXPECT_TRUE(has(solo.unmatched_reasons, "no-matching-reader"));
}

TEST(FilterByNode, RecomputesUnmatchedWhenPartnersVanish)
{
  // writer of /talker paired with a reader of /other only: filtering on /talker keeps
  // the pair (and thus /other's reader); filtering on /other keeps it too. Filtering on
  // a node whose reader has no writer yields no-matching-writer.
  auto ep = [](bool writer, const std::string & node) {
      Endpoint e = make(writer, HOST_A, {shm(), udp4("127.0.0.1")});
      e.node_name = node;
      return e;
    };
  std::vector<Endpoint> eps;
  eps.push_back(ep(true, "/talker"));
  eps.push_back(ep(false, "/other"));
  Endpoint lonely = ep(false, "/lonely");
  lonely.dds_type = "other::type";       // type mismatch: never paired
  eps.push_back(lonely);
  auto topics = summarize(eps);
  ASSERT_EQ(topics.size(), 1u);
  EXPECT_TRUE(has(topics[0].unmatched_reasons, "type-name-mismatch"));

  filter_by_node(topics, [](const Endpoint & e) {return e.node_name == "/lonely";});
  ASSERT_EQ(topics.size(), 1u);
  EXPECT_TRUE(topics[0].pairs.empty());
  EXPECT_TRUE(topics[0].writers.empty());
  EXPECT_TRUE(has(topics[0].unmatched_reasons, "no-matching-writer"));
  EXPECT_FALSE(has(topics[0].unmatched_reasons, "no-matching-reader"));
}

TEST(RosNames, DemangleTopics)
{
  EXPECT_EQ(demangle_topic("rt/chatter").kind, RosEntityKind::Topic);
  EXPECT_EQ(demangle_topic("rt/chatter").name, "/chatter");
  EXPECT_EQ(demangle_topic("rt/ns/chatter").name, "/ns/chatter");
  EXPECT_EQ(demangle_topic("rq/add_two_intsRequest").kind, RosEntityKind::ServiceRequest);
  EXPECT_EQ(demangle_topic("rq/add_two_intsRequest").name, "/add_two_ints");
  EXPECT_EQ(demangle_topic("rr/add_two_intsReply").kind, RosEntityKind::ServiceReply);
  EXPECT_EQ(demangle_topic("rr/add_two_intsReply").name, "/add_two_ints");
  EXPECT_EQ(demangle_topic("ros_discovery_info").kind, RosEntityKind::NotRos);
  EXPECT_EQ(demangle_topic("_fastdds_statistics_rtps_sent").kind, RosEntityKind::NotRos);
}

TEST(RosNames, DemangleTypes)
{
  EXPECT_EQ(demangle_type("std_msgs::msg::dds_::String_"), "std_msgs/msg/String");
  EXPECT_EQ(
    demangle_type("example_interfaces::srv::dds_::AddTwoInts_Request_"),
    "example_interfaces/srv/AddTwoInts_Request");
  EXPECT_EQ(demangle_type("HelloWorld"), "");
}

// ---- statistics overlay --------------------------------------------------------

namespace
{
StatsData stats_with(const Endpoint & w, std::vector<TrafficSample> traffic, bool delivered_to = false,
  const Endpoint * r = nullptr)
{
  StatsData s;
  s.enabled = true;
  s.participants_with_stats.insert(w.participant_guid_prefix);
  s.traffic = std::move(traffic);
  if (delivered_to && r) {
    s.delivered.insert({w.guid, r->guid});
  }
  return s;
}
}  // namespace

TEST(ApplyStats, MeasuredShmConfirmsPrediction)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  ASSERT_EQ(topics[0].pairs.size(), 1u);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", shm(7413), 10, 1000.0}});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_TRUE(p.measured.available);
  ASSERT_EQ(p.measured.transports.size(), 1u);
  EXPECT_EQ(p.measured.transports[0], Transport::SHM);
  EXPECT_EQ(p.measured.packets, 10u);
  EXPECT_EQ(p.verdict.confidence, Confidence::Certain);
  EXPECT_TRUE(has(p.verdict.reasons, "measured-shm-traffic"));
  EXPECT_TRUE(p.verdict.warnings.empty());
}

TEST(ApplyStats, MismatchWarnsWhenPacketsWentElsewhere)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  // predicted SHM, but packets only on the reader's UDPv4 locator
  auto stats = stats_with(eps[0], {TrafficSample{"P1", udp4("10.0.0.1", 7413), 5, 500.0}});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_EQ(p.verdict.transport, Transport::SHM);
  EXPECT_TRUE(has(p.verdict.reasons, "measured-udpv4-traffic"));
  EXPECT_TRUE(has(p.verdict.warnings, "measured-transport-mismatch"));
}

TEST(ApplyStats, TrafficToOtherLocatorsIsIgnored)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}));
  eps.push_back(make(false, HOST_A, {shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  // metatraffic port 7412 is not a locator of the reader; different source participant too
  auto stats = stats_with(eps[0], {
      TrafficSample{"P1", udp4("10.0.0.1", 7412), 5, 500.0},
      TrafficSample{"P9", shm(7413), 5, 500.0}});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_TRUE(p.measured.transports.empty());
  EXPECT_TRUE(has(p.verdict.warnings, "no-traffic-observed"));
  EXPECT_EQ(p.verdict.confidence, Confidence::Certain);   // prediction untouched
}

TEST(ApplyStats, NoStatsFromWriterParticipant)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}));
  eps.push_back(make(false, HOST_A, {shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  StatsData s;
  s.enabled = true;   // enabled but nobody publishes
  apply_stats(topics, s);
  const auto & p = topics[0].pairs[0];
  EXPECT_FALSE(p.measured.available);
  EXPECT_TRUE(has(p.verdict.warnings, "stats-not-enabled-on-writer"));
}

TEST(ApplyStats, DisabledStatsLeavesVerdictUntouched)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}));
  eps.push_back(make(false, HOST_A, {shm(7413)}));
  auto topics = summarize(eps);
  StatsData s;
  apply_stats(topics, s);
  EXPECT_FALSE(topics[0].pairs[0].measured.available);
  EXPECT_TRUE(topics[0].pairs[0].verdict.warnings.empty());
}

TEST(ApplyStats, DataSharingConfirmedByDeliveryWithoutTraffic)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7413)}, DataSharingKind::On, {1}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  ASSERT_EQ(topics[0].pairs[0].verdict.transport, Transport::DataSharing);
  auto stats = stats_with(eps[0], {}, true, &eps[1]);
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_TRUE(p.measured.delivered);
  EXPECT_EQ(p.verdict.confidence, Confidence::Certain);
  EXPECT_TRUE(has(p.verdict.reasons, "datasharing-confirmed-no-traffic"));
  EXPECT_FALSE(has(p.verdict.reasons, "datasharing-unverified-by-traffic"));
}

TEST(ApplyStats, DataSharingStaysLikelyWithParticipantTraffic)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7413)}, DataSharingKind::On, {1}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", shm(7413), 3, 300.0}}, true, &eps[1]);
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_EQ(p.verdict.transport, Transport::DataSharing);
  EXPECT_EQ(p.verdict.confidence, Confidence::Likely);
  EXPECT_TRUE(has(p.verdict.reasons, "datasharing-ambiguous-participant-traffic"));
}

TEST(ApplyStats, WriterInstanceLimitSuspectedWhenTenLocatorsReported)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}));
  eps.push_back(make(false, HOST_A, {shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  std::vector<TrafficSample> traffic;
  for (uint32_t port = 8000; port < 8010; ++port) {   // 10 unrelated locators
    traffic.push_back(TrafficSample{"P1", udp4("10.0.0.1", port), 1, 100.0});
  }
  auto stats = stats_with(eps[0], traffic);
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_TRUE(p.measured.transports.empty());
  EXPECT_TRUE(has(p.verdict.warnings, "stats-writer-instance-limit-suspected"));
  EXPECT_FALSE(has(p.verdict.warnings, "no-traffic-observed"));
}

// ---- frame-to-frame diff (--watch) ----------------------------------------------

TEST(Diff, DetectsAddedRemovedAndChangedPairs)
{
  std::map<PairKey, PairState> prev, cur;
  PairState shm;
  shm.transport = Transport::SHM;
  PairState udp;
  udp.transport = Transport::UDPv4;
  PairState shm_measured = shm;
  shm_measured.measured = {Transport::SHM};

  prev[{"/a", "w1", "r1"}] = shm;   // unchanged
  prev[{"/a", "w1", "r2"}] = shm;   // changes transport
  prev[{"/b", "w2", "r3"}] = udp;   // removed
  cur[{"/a", "w1", "r1"}] = shm;
  cur[{"/a", "w1", "r2"}] = udp;
  cur[{"/c", "w3", "r4"}] = shm_measured;   // added

  auto c = diff(prev, cur);
  ASSERT_EQ(c.added.size(), 1u);
  EXPECT_EQ(c.added[0].topic, "/c");
  ASSERT_EQ(c.removed.size(), 1u);
  EXPECT_EQ(c.removed[0].reader_guid, "r3");
  ASSERT_EQ(c.changed.size(), 1u);
  EXPECT_EQ(c.changed[0].from.transport, Transport::SHM);
  EXPECT_EQ(c.changed[0].to.transport, Transport::UDPv4);
}

TEST(Diff, MeasuredAndWarningChangesCount)
{
  std::map<PairKey, PairState> prev, cur;
  PairState a;
  a.transport = Transport::SHM;
  PairState b = a;
  b.measured = {Transport::SHM};
  PairState c = a;
  c.warnings = {"no-traffic-observed"};
  prev[{"/t", "w", "r1"}] = a;
  prev[{"/t", "w", "r2"}] = a;
  cur[{"/t", "w", "r1"}] = b;
  cur[{"/t", "w", "r2"}] = c;
  auto d = diff(prev, cur);
  EXPECT_EQ(d.changed.size(), 2u);
  EXPECT_TRUE(d.added.empty());
  EXPECT_TRUE(d.removed.empty());
  EXPECT_TRUE(diff(cur, cur).empty());
}

TEST(Diff, PairStatesFromSnapshot)
{
  Snapshot snap;
  snap.endpoints.push_back(make(true, HOST_A, {shm()}));
  snap.endpoints.push_back(make(false, HOST_A, {shm()}));
  snap.topics = summarize(snap.endpoints);
  auto states = pair_states(snap);
  ASSERT_EQ(states.size(), 1u);
  EXPECT_EQ(states.begin()->first.topic, "/chatter");
  EXPECT_EQ(states.begin()->second.transport, Transport::SHM);
}
