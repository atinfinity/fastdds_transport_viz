// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <tuple>
#include <string>
#include <vector>

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
StatsData stats_with(
  const Endpoint & w, std::vector<TrafficSample> traffic, bool delivered_to = false,
  const Endpoint * r = nullptr)
{
  StatsData s;
  s.enabled = true;
  s.participants_with_stats.insert(w.participant_guid_prefix);
  s.traffic = std::move(traffic);
  if (delivered_to && r) {
    s.delivered[{w.guid, r->guid}] = 1;
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

TEST(ApplyStats, LoopbackReaderLocatorMatchesTrafficToLocalAddress)
{
  // A reader on the tool's host is announced with 127.0.0.1 (Fast DDS's localhost
  // transformation) while the remote writer's RTPS_SENT names the host's real address.
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_B, {udp4("192.168.1.6")}));
  eps.push_back(make(false, HOST_A, {udp4("127.0.0.1", 7413), shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  ASSERT_EQ(topics[0].pairs.size(), 1u);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", udp4("192.168.1.8", 7413), 7, 700.0}});
  apply_stats(topics, stats);   // without the host's addresses: nothing matches
  EXPECT_TRUE(topics[0].pairs[0].measured.transports.empty());
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.warnings, "no-traffic-observed"));

  topics = summarize(eps);
  stats.local_addresses = {"192.168.1.8", "172.17.0.1"};
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_EQ(p.verdict.transport, Transport::UDPv4);
  ASSERT_EQ(p.measured.transports.size(), 1u);
  EXPECT_EQ(p.measured.transports[0], Transport::UDPv4);
  EXPECT_EQ(p.measured.packets, 7u);
  EXPECT_TRUE(has(p.verdict.reasons, "measured-udpv4-traffic"));
  EXPECT_FALSE(has(p.verdict.warnings, "no-traffic-observed"));
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

TEST(ApplyStats, DataSharingConfirmedWhenDataCountDoesNotGrow)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7413)}, DataSharingKind::On, {1}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  // heartbeats on the link, delivery proven, DATA_COUNT stayed at 5 during the window
  auto stats = stats_with(eps[0], {TrafficSample{"P1", shm(7413), 3, 300.0}}, true, &eps[1]);
  stats.data_count[eps[0].guid] = DataCountSample{5, 5, 2};
  stats.statistics_writers.insert({"P1", kStatsDataCountTopic});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_EQ(p.verdict.transport, Transport::DataSharing);
  EXPECT_EQ(p.verdict.confidence, Confidence::Certain);
  EXPECT_TRUE(has(p.verdict.reasons, "datasharing-confirmed-no-data-submessages"));
  EXPECT_TRUE(p.measured.data_count_available);
  EXPECT_EQ(p.measured.data_submessages, 0u);
  EXPECT_EQ(p.measured.delivered_samples, 1u);
}

TEST(ApplyStats, DataSharingConfirmedWhenWriterNeverPublishedDataCount)
{
  // A writer that never sent a DATA submessage never publishes DATA_COUNT; the
  // discovered DATA_COUNT statistics writer of its participant stands in for "zero".
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7413)}, DataSharingKind::On, {1}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", shm(7413), 3, 300.0}}, true, &eps[1]);
  stats.statistics_writers.insert({"P1", kStatsDataCountTopic});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_EQ(p.verdict.confidence, Confidence::Certain);
  EXPECT_TRUE(has(p.verdict.reasons, "datasharing-confirmed-no-data-submessages"));
  EXPECT_EQ(p.measured.data_submessages, 0u);
}

TEST(ApplyStats, DataSharingNotUsedWhenDataSubmessagesWereSent)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7413)}, DataSharingKind::On, {1}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", shm(7413), 30, 30000.0}}, true, &eps[1]);
  stats.data_count[eps[0].guid] = DataCountSample{5, 25, 6};
  stats.statistics_writers.insert({"P1", kStatsDataCountTopic});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_EQ(p.verdict.transport, Transport::SHM);
  EXPECT_EQ(p.verdict.confidence, Confidence::Certain);
  EXPECT_TRUE(has(p.verdict.reasons, "datasharing-data-submessages-sent"));
  EXPECT_TRUE(has(p.verdict.reasons, "measured-shm-traffic"));
  EXPECT_TRUE(has(p.verdict.warnings, "datasharing-not-used"));
  EXPECT_EQ(p.measured.data_submessages, 20u);
}

TEST(ApplyStats, DataSharingStaysLikelyWithMixedReaders)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7413)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7417)}, DataSharingKind::Off));   // plain SHM reader
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  ASSERT_EQ(topics[0].pairs.size(), 2u);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", shm(7413), 3, 300.0}}, true, &eps[1]);
  stats.data_count[eps[0].guid] = DataCountSample{5, 25, 6};   // DATA for the SHM reader
  stats.statistics_writers.insert({"P1", kStatsDataCountTopic});
  apply_stats(topics, stats);
  const auto & ds = *std::find_if(topics[0].pairs.begin(), topics[0].pairs.end(),
      [&](const Pair & q) {return q.reader == &eps[1];});
  EXPECT_EQ(ds.verdict.transport, Transport::DataSharing);
  EXPECT_EQ(ds.verdict.confidence, Confidence::Likely);
  EXPECT_TRUE(has(ds.verdict.reasons, "datasharing-ambiguous-mixed-readers"));
  EXPECT_TRUE(ds.verdict.warnings.empty());
}

TEST(ApplyStats, DeliveredWithoutMeasuredTrafficIsItsOwnWarning)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}));
  eps.push_back(make(false, HOST_A, {shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  // traffic only to unrelated locators, but HISTORY_LATENCY proves delivery
  auto stats = stats_with(eps[0], {TrafficSample{"P1", shm(7001), 3, 300.0}}, true, &eps[1]);
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_TRUE(p.measured.delivered);
  EXPECT_TRUE(p.measured.transports.empty());
  EXPECT_TRUE(has(p.verdict.warnings, "delivered-without-measured-traffic"));
  EXPECT_FALSE(has(p.verdict.warnings, "no-traffic-observed"));
  EXPECT_EQ(p.verdict.confidence, Confidence::Certain);  // nothing measured, prediction stands
}

TEST(ApplyStats, ThroughputPerWriterAndPerTopicAndWindowDeltas)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}));
  eps.push_back(make(true, HOST_A, {shm(7419)}));
  eps.push_back(make(false, HOST_A, {shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  eps[1].participant_guid_prefix = "P2";
  auto topics = summarize(eps);
  ASSERT_EQ(topics[0].pairs.size(), 2u);
  // cumulative 100 packets / 10000 bytes at the first sample, 130 / 13000 at the last
  TrafficSample t{"P1", shm(7413), 130, 13000.0};
  t.packets_first = 100;
  t.bytes_first = 10000.0;
  t.samples = 3;
  auto stats = stats_with(eps[0], {t});
  stats.participants_with_stats.insert("P2");
  stats.throughput[eps[0].guid] = ThroughputStat{300.0, 120.0, 3};   // mean 100 B/s
  stats.throughput[eps[1].guid] = ThroughputStat{50.0, 50.0, 1};     // 50 B/s
  apply_stats(topics, stats);
  const auto & topic = topics[0];
  EXPECT_TRUE(topic.throughput_available);
  EXPECT_DOUBLE_EQ(topic.throughput, 150.0);
  const auto & p = *std::find_if(topic.pairs.begin(), topic.pairs.end(),
      [&](const Pair & q) {return q.writer == &eps[0];});
  EXPECT_TRUE(p.measured.throughput_available);
  EXPECT_DOUBLE_EQ(p.measured.throughput, 100.0);
  EXPECT_EQ(p.measured.packets, 30u);            // during the observation
  EXPECT_DOUBLE_EQ(p.measured.bytes, 3000.0);
  EXPECT_EQ(p.measured.packets_total, 130u);     // since the participant started
  EXPECT_DOUBLE_EQ(p.measured.bytes_total, 13000.0);
  EXPECT_TRUE(has(p.verdict.reasons, "measured-shm-traffic"));
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

// ---- coverage of the remaining branches -----------------------------------------

namespace
{
Locator tcp4(const std::string & ip, uint32_t port = 7411)
{
  return Locator{LocatorKind::TCPv4, ip, port};
}
Locator tcp6(const std::string & ip, uint32_t port = 7411)
{
  return Locator{LocatorKind::TCPv6, ip, port};
}
Locator udp6(const std::string & ip, uint32_t port = 7411)
{
  return Locator{LocatorKind::UDPv6, ip, port};
}
}  // namespace

TEST(Decision, CrossHostTcpAndUdp6Locators)
{
  auto w4 = make(true, HOST_A, {tcp4("10.0.0.1"), shm()});
  auto r4 = make(false, HOST_B, {tcp4("10.0.0.2"), shm()});
  auto v = decide(w4, r4);
  EXPECT_EQ(v.transport, Transport::TCPv4);
  EXPECT_TRUE(has(v.reasons, "different-host"));
  EXPECT_TRUE(has(v.reasons, "common-tcpv4-locator"));
  EXPECT_TRUE(has(v.reasons, "shm-locators-ignored-across-hosts"));

  auto w6 = make(true, HOST_A, {tcp6("fd00::1")});
  auto r6 = make(false, HOST_B, {tcp6("fd00::2")});
  EXPECT_EQ(decide(w6, r6).transport, Transport::TCPv6);
  EXPECT_TRUE(has(decide(w6, r6).reasons, "common-tcpv6-locator"));

  auto wu6 = make(true, HOST_A, {udp6("fd00::1")});
  auto ru6 = make(false, HOST_B, {udp6("fd00::2")});
  EXPECT_EQ(decide(wu6, ru6).transport, Transport::UDPv6);
  EXPECT_TRUE(has(decide(wu6, ru6).reasons, "common-udpv6-locator"));
}

TEST(Decision, NoCommonTransportGivesNone)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1")});
  auto r = make(false, HOST_B, {tcp4("10.0.0.2")});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_TRUE(has(v.reasons, "no-common-transport"));
}

TEST(Decision, DataSharingQosBranches)
{
  // reader disabled
  auto w = make(true, HOST_A, {shm()}, DataSharingKind::Auto, {1});
  auto r = make(false, HOST_A, {shm()}, DataSharingKind::Off);
  EXPECT_TRUE(has(decide(w, r).reasons, "datasharing-disabled-reader"));
  EXPECT_EQ(decide(w, r).transport, Transport::SHM);
  // unknown on one side
  auto ru = make(false, HOST_A, {shm()}, DataSharingKind::Unknown);
  EXPECT_TRUE(has(decide(w, ru).reasons, "datasharing-qos-unknown"));
  // domain ids announced but disjoint: falls through to SHM
  auto rd = make(false, HOST_A, {shm()}, DataSharingKind::On, {2});
  auto v = decide(w, rd);
  EXPECT_EQ(v.transport, Transport::SHM);
  EXPECT_TRUE(has(v.reasons, "datasharing-domain-ids-mismatch"));
  // one side without ids: still data-sharing, likely
  auto rn = make(false, HOST_A, {shm()}, DataSharingKind::On, {});
  auto vn = decide(w, rn);
  EXPECT_EQ(vn.transport, Transport::DataSharing);
  EXPECT_EQ(vn.confidence, Confidence::Likely);
  EXPECT_TRUE(has(vn.reasons, "datasharing-domain-ids-unknown"));
}

TEST(Decision, WriterWithoutShmLocatorOnSameHost)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1")});
  auto r = make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::UDPv4);
  EXPECT_TRUE(has(v.reasons, "writer-no-shm-locator"));
}

TEST(Model, ToStringCoversEveryValue)
{
  EXPECT_EQ(to_string(LocatorKind::TCPv4), "TCPv4");
  EXPECT_EQ(to_string(LocatorKind::TCPv6), "TCPv6");
  EXPECT_EQ(to_string(LocatorKind::UDPv6), "UDPv6");
  EXPECT_EQ(to_string(LocatorKind::Invalid), "INVALID");
  EXPECT_EQ(to_string(Transport::TCPv4), "TCPv4");
  EXPECT_EQ(to_string(Transport::TCPv6), "TCPv6");
  EXPECT_EQ(to_string(Transport::UDPv6), "UDPv6");
  EXPECT_EQ(to_string(Transport::DataSharing), "DATA_SHARING");
  EXPECT_EQ(to_string(Transport::None), "NONE");
  EXPECT_EQ(to_string(Confidence::Likely), "likely");
  EXPECT_EQ(to_string(DataSharingKind::On), "ON");
  EXPECT_EQ(to_string(DataSharingKind::Auto), "AUTO");
  EXPECT_EQ(to_string(DataSharingKind::Unknown), "UNKNOWN");
  EXPECT_EQ(host_id_hex(HostId{0x01, 0x0f, 0xaa, 0xbb}), "010faabb");
}

TEST(Codes, EveryKnownCodeHasADescriptionAndUnknownDoesNot)
{
  auto codes = known_codes();
  EXPECT_GT(codes.size(), 40u);
  for (const auto & c : codes) {
    EXPECT_NE(explain(c), "(no description)") << c;
  }
  EXPECT_EQ(explain("not-a-code"), "(no description)");
}

TEST(Diff, AddedRemovedChanged)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  Snapshot a;
  a.endpoints = eps;
  a.topics = summarize(a.endpoints);
  auto before = pair_states(a);
  ASSERT_EQ(before.size(), 1u);

  // same pair measured on UDPv4 => changed; a second reader => added
  Snapshot b = a;
  b.endpoints.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7417)}));
  b.topics = summarize(b.endpoints);
  auto stats = stats_with(b.endpoints[0], {TrafficSample{"P1", udp4("10.0.0.1", 7413), 5, 500.0}});
  apply_stats(b.topics, stats);
  auto after = pair_states(b);
  ASSERT_EQ(after.size(), 2u);
  auto c = diff(before, after);
  EXPECT_EQ(c.added.size(), 1u);
  EXPECT_EQ(c.changed.size(), 1u);
  EXPECT_TRUE(c.removed.empty());
  EXPECT_FALSE(c.empty());
  EXPECT_EQ(c.changed[0].from.measured.size(), 0u);
  EXPECT_EQ(c.changed[0].to.measured.size(), 1u);
  EXPECT_TRUE(has(c.changed[0].to.warnings, "measured-transport-mismatch"));

  auto back = diff(after, before);
  EXPECT_EQ(back.removed.size(), 1u);
  EXPECT_EQ(back.changed.size(), 1u);
  EXPECT_TRUE(diff(after, after).empty());
}

TEST(ApplyStats, DataSharingNoDeliveryAndAmbiguousTraffic)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7413)}, DataSharingKind::On, {1}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  ASSERT_EQ(topics[0].pairs[0].verdict.transport, Transport::DataSharing);
  // statistics available, nothing delivered, no traffic
  auto stats = stats_with(eps[0], {});
  apply_stats(topics, stats);
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.reasons, "datasharing-no-delivery-observed"));
  EXPECT_EQ(topics[0].pairs[0].verdict.confidence, Confidence::Likely);
  // traffic on the link but no delivery proof and no DATA_COUNT
  topics = summarize(eps);
  stats = stats_with(eps[0], {TrafficSample{"P1", shm(7413), 4, 400.0}});
  apply_stats(topics, stats);
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.reasons, "datasharing-ambiguous-participant-traffic"));
  EXPECT_EQ(topics[0].pairs[0].verdict.transport, Transport::DataSharing);
}

TEST(ApplyStats, MeasuredTcpReasonAndDeliveredWithoutTraffic)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {tcp4("10.0.0.1")}));
  eps.push_back(make(false, HOST_B, {tcp4("10.0.0.2", 7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", tcp4("10.0.0.2", 7413), 3, 300.0}});
  apply_stats(topics, stats);
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.reasons, "measured-tcpv4-traffic"));
  EXPECT_EQ(topics[0].pairs[0].verdict.confidence, Confidence::Certain);

  topics = summarize(eps);
  stats = stats_with(eps[0], {}, true, &eps[1]);
  apply_stats(topics, stats);
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.warnings, "delivered-without-measured-traffic"));
  EXPECT_TRUE(topics[0].pairs[0].measured.delivered);
}

TEST(ApplyStats, MeasuredUdp6Tcp6AndUnknownLocatorKinds)
{
  using MakerT = std::function<Locator(const std::string &, uint32_t)>;
  for (auto [mk, code, tr] : std::vector<std::tuple<MakerT, std::string, Transport>>{
    {[](const std::string & ip, uint32_t port) {return udp6(ip, port);},
      "measured-udpv6-traffic", Transport::UDPv6},
    {[](const std::string & ip, uint32_t port) {return tcp6(ip, port);},
      "measured-tcpv6-traffic", Transport::TCPv6}})
  {
    std::vector<Endpoint> eps;
    eps.push_back(make(true, HOST_A, {mk("fd00::1", 7411)}));
    eps.push_back(make(false, HOST_B, {mk("fd00::2", 7413)}));
    eps[0].participant_guid_prefix = "P1";
    auto topics = summarize(eps);
    auto stats = stats_with(eps[0], {TrafficSample{"P1", mk("fd00::2", 7413), 2, 200.0}});
    apply_stats(topics, stats);
    EXPECT_TRUE(has(topics[0].pairs[0].verdict.reasons, code)) << code;
    ASSERT_EQ(topics[0].pairs[0].measured.transports.size(), 1u);
    EXPECT_EQ(topics[0].pairs[0].measured.transports[0], tr);
  }
  // a locator of unknown kind announced by the reader and reported by RTPS_SENT
  std::vector<Endpoint> eps;
  Locator odd{LocatorKind::Invalid, "?", 1};
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1")}));
  eps.push_back(make(false, HOST_B, {odd, udp4("10.0.0.2", 7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", odd, 2, 200.0}});
  apply_stats(topics, stats);
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.reasons, "measured-unknown-traffic"));
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.warnings, "measured-transport-mismatch"));
}

TEST(ApplyStats, WriterWithoutStatisticsIsWarnedAndSkipped)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  StatsData stats;
  stats.enabled = true;
  stats.participants_with_stats.insert("P-other");   // statistics arrive, but not from P1
  stats.traffic.push_back(TrafficSample{"P1", shm(7413), 10, 1000.0});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_FALSE(p.measured.available);
  EXPECT_TRUE(has(p.verdict.warnings, "stats-not-enabled-on-writer"));
  EXPECT_TRUE(p.measured.transports.empty() || p.measured.transports.size() == 1u);
  EXPECT_FALSE(has(p.verdict.reasons, "measured-shm-traffic"));
}

// ---- QoS request / offer --------------------------------------------------------------

namespace
{
std::pair<Endpoint, Endpoint> shm_pair()
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)});
  auto r = make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)});
  w.qos.reliability = "RELIABLE"; r.qos.reliability = "RELIABLE";
  w.qos.durability = "VOLATILE"; r.qos.durability = "VOLATILE";
  return {w, r};
}
}  // namespace

TEST(QosMatching, CompatibleDefaults)
{
  auto [w, r] = shm_pair();
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());
  EXPECT_EQ(decide(w, r).transport, Transport::SHM);
  // reliable writer, best-effort reader is fine; transient-local writer, volatile reader too
  r.qos.reliability = "BEST_EFFORT";
  w.qos.durability = "TRANSIENT_LOCAL";
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());
}

TEST(QosMatching, ReliabilityAndDurability)
{
  auto [w, r] = shm_pair();
  w.qos.reliability = "BEST_EFFORT";
  EXPECT_EQ(qos_incompatibilities(w, r), (std::vector<std::string>{"reliability"}));
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_EQ(v.confidence, Confidence::Certain);
  EXPECT_EQ(v.reasons, (std::vector<std::string>{"qos-incompatible-reliability"}));
  EXPECT_TRUE(has(v.warnings, "qos-incompatible"));

  w.qos.reliability = "RELIABLE";
  r.qos.durability = "TRANSIENT_LOCAL";
  EXPECT_EQ(qos_incompatibilities(w, r), (std::vector<std::string>{"durability"}));
  w.qos.durability = "TRANSIENT";
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());
  r.qos.durability = "PERSISTENT";
  EXPECT_EQ(qos_incompatibilities(w, r), (std::vector<std::string>{"durability"}));
  r.qos.durability = "UNKNOWN";   // not judged
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());
}

TEST(QosMatching, DeadlineLivelinessOwnershipPartition)
{
  auto [w, r] = shm_pair();
  r.qos.deadline_s = 0.5;          // reader wants 2 Hz, writer promises nothing (infinite)
  EXPECT_EQ(qos_incompatibilities(w, r), (std::vector<std::string>{"deadline"}));
  w.qos.deadline_s = 0.5;
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());
  w.qos.deadline_s = 0.1;
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());

  r.qos.liveliness = "MANUAL_BY_TOPIC";
  EXPECT_EQ(qos_incompatibilities(w, r), (std::vector<std::string>{"liveliness"}));
  w.qos.liveliness = "MANUAL_BY_TOPIC";
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());
  r.qos.liveliness_lease_s = 1.0;   // writer lease infinite > reader lease
  EXPECT_EQ(qos_incompatibilities(w, r), (std::vector<std::string>{"liveliness"}));
  w.qos.liveliness_lease_s = 0.5;
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());

  r.qos.ownership = "EXCLUSIVE";
  EXPECT_EQ(qos_incompatibilities(w, r), (std::vector<std::string>{"ownership"}));
  w.qos.ownership = "EXCLUSIVE";
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());

  w.qos.partitions = {"robot1"};
  EXPECT_EQ(qos_incompatibilities(w, r), (std::vector<std::string>{"partition"}));
  r.qos.partitions = {"robot*"};    // pattern on either side
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());
  r.qos.partitions = {"robot2", "other"};
  EXPECT_EQ(qos_incompatibilities(w, r), (std::vector<std::string>{"partition"}));
  w.qos.partitions = {};
  r.qos.partitions = {""};          // explicit default partition matches the empty list
  EXPECT_TRUE(qos_incompatibilities(w, r).empty());

  // several policies at once, in a fixed order
  w = shm_pair().first; r = shm_pair().second;
  w.qos.reliability = "BEST_EFFORT";
  r.qos.durability = "TRANSIENT_LOCAL";
  r.qos.ownership = "EXCLUSIVE";
  auto v = decide(w, r);
  EXPECT_EQ(
    v.reasons, (std::vector<std::string>{
    "qos-incompatible-reliability", "qos-incompatible-durability", "qos-incompatible-ownership"}));
}

TEST(ApplyStats, IncompatiblePairDeliveredIsReported)
{
  auto [w, r] = shm_pair();
  w.qos.reliability = "BEST_EFFORT";
  w.participant_guid_prefix = "P1";
  std::vector<Endpoint> eps{w, r};
  auto topics = summarize(eps);
  ASSERT_EQ(topics[0].pairs[0].verdict.transport, Transport::None);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", shm(7413), 5, 500.0}});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_EQ(p.verdict.transport, Transport::None);          // no measured-transport overlay
  EXPECT_FALSE(has(p.verdict.warnings, "measured-transport-mismatch"));
  EXPECT_FALSE(has(p.verdict.warnings, "qos-incompatible-but-delivered"));

  topics = summarize(eps);
  stats = stats_with(eps[0], {}, true, &eps[1]);
  apply_stats(topics, stats);
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.warnings, "qos-incompatible-but-delivered"));
}

TEST(ApplyStats, LatencyPerPairTopicMaxAndClockSkew)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7417), shm(7417)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  ASSERT_EQ(topics[0].pairs.size(), 2u);
  auto stats = stats_with(
    eps[0], {TrafficSample{"P1", shm(7413), 3, 300.0}, TrafficSample{"P1", shm(7417), 3, 300.0}});
  LatencyStat a;
  a.add(0.0004); a.add(0.0006); a.add(0.0013);
  stats.latency[{eps[0].guid, eps[1].guid}] = a;
  stats.delivered[{eps[0].guid, eps[1].guid}] = 3;
  LatencyStat b;
  b.add(-0.002); b.add(-0.001);      // reader clock behind the writer's
  stats.latency[{eps[0].guid, eps[2].guid}] = b;
  apply_stats(topics, stats);
  const auto & p0 = topics[0].pairs[0];
  ASSERT_TRUE(p0.measured.latency_available);
  EXPECT_NEAR(p0.measured.latency.mean(), 0.0007666, 1e-6);
  EXPECT_DOUBLE_EQ(p0.measured.latency.max, 0.0013);
  EXPECT_DOUBLE_EQ(p0.measured.latency.min, 0.0004);
  EXPECT_DOUBLE_EQ(p0.measured.latency.last, 0.0013);
  EXPECT_EQ(p0.measured.latency.samples, 3u);
  EXPECT_FALSE(has(p0.verdict.warnings, "latency-clock-skew-suspected"));
  const auto & p1 = topics[0].pairs[1];
  EXPECT_TRUE(has(p1.verdict.warnings, "latency-clock-skew-suspected"));
  EXPECT_TRUE(topics[0].latency_available);
  EXPECT_NEAR(topics[0].latency, 0.0007666, 1e-6);   // the slowest pair's mean
  // no latency samples at all
  topics = summarize(eps);
  apply_stats(topics, stats_with(eps[0], {}));
  EXPECT_FALSE(topics[0].pairs[0].measured.latency_available);
  EXPECT_FALSE(topics[0].latency_available);
}

TEST(Decision, SameHostLocatorsHiddenOnOldFastDds)
{
  // what Fast DDS 2.6 shows of a same-host writer
  auto w = make(true, HOST_A, {shm(7411)});
  auto r = make(false, HOST_A, {udp4("127.0.0.1", 7413)});   // a UDP-only reader
  EXPECT_EQ(decide(w, r).transport, Transport::None);        // without the flag: no common kind
  w.same_host_locators_filtered = true;
  r.same_host_locators_filtered = true;
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::UDPv4);
  EXPECT_EQ(v.confidence, Confidence::Likely);
  EXPECT_TRUE(has(v.reasons, "same-host-locators-hidden"));
  // both sides SHM: still SHM; both UDP-only: still UDPv4 certain
  auto r2 = make(false, HOST_A, {shm(7413)});
  r2.same_host_locators_filtered = true;
  EXPECT_EQ(decide(w, r2).transport, Transport::SHM);
  auto w3 = make(true, HOST_A, {udp4("127.0.0.1", 7411)});
  w3.same_host_locators_filtered = true;
  EXPECT_EQ(decide(w3, r).transport, Transport::UDPv4);
  EXPECT_EQ(decide(w3, r).confidence, Confidence::Certain);
}

TEST(ApplyStats, ReliabilityCountersAndLostPackets)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}));
  eps.push_back(make(false, HOST_B, {udp4("10.0.0.2", 7413)}));
  eps[0].participant_guid_prefix = "P1";
  eps[1].participant_guid_prefix = "P2";
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", udp4("10.0.0.2", 7413), 50, 5000.0}});
  // the reader's participant missed 3 packets from the writer's UDPv4 locator during the window
  TrafficSample lost{"P2", udp4("10.0.0.1", 7411), 7, 700.0};
  lost.packets_first = 4;
  stats.lost.push_back(lost);
  // another participant
  stats.lost.push_back(TrafficSample{"P9", udp4("10.0.0.1", 7411), 100, 0.0});
  stats.resent_datas[eps[0].guid] = DataCountSample{10, 12, 2};
  stats.heartbeats[eps[0].guid] = DataCountSample{0, 40, 5};
  stats.gaps[eps[0].guid] = DataCountSample{1, 1, 2};
  stats.acknacks[eps[1].guid] = DataCountSample{5, 9, 3};
  stats.nackfrags[eps[1].guid] = DataCountSample{0, 0, 1};
  apply_stats(topics, stats);
  const auto & r = topics[0].pairs[0].measured.reliability;
  EXPECT_TRUE(r.available);
  EXPECT_EQ(r.lost_packets, 3u);
  EXPECT_EQ(r.resent, 2u);
  EXPECT_EQ(r.heartbeats, 40u);
  EXPECT_EQ(r.gaps, 0u);
  EXPECT_EQ(r.acknacks, 4u);
  EXPECT_EQ(r.nackfrags, 0u);
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.warnings, "rtps-packets-lost"));
  EXPECT_TRUE(topics[0].reliability_available);
  EXPECT_EQ(topics[0].lost_packets, 3u);
  EXPECT_EQ(topics[0].resent, 2u);

  // loopback: the writer next to the tool is announced as 127.0.0.1, the remote reader
  // reports the real address
  eps[0].unicast = {udp4("127.0.0.1", 7411)};
  topics = summarize(eps);
  stats.local_addresses = {"10.0.0.1"};
  apply_stats(topics, stats);
  EXPECT_EQ(topics[0].pairs[0].measured.reliability.lost_packets, 3u);

  // no counters at all
  topics = summarize(eps);
  apply_stats(topics, stats_with(eps[0], {}));
  EXPECT_FALSE(topics[0].pairs[0].measured.reliability.available);
  EXPECT_FALSE(topics[0].reliability_available);
  EXPECT_FALSE(has(topics[0].pairs[0].verdict.warnings, "rtps-packets-lost"));
}
