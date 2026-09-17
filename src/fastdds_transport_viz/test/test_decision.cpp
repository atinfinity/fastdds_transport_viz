// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <stdexcept>
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

TEST(Decision, SameShmPortOnTwoParticipantsIsAnIpcSplit)
{
  // two host-network containers without host IPC: each namespace hands out 7000 first
  auto w = make(true, HOST_A, {udp4("10.0.0.1"), shm(16161)});
  auto r = make(false, HOST_A, {udp4("10.0.0.1"), shm(16163)});
  w.participant_guid_prefix = "P1";
  r.participant_guid_prefix = "P2";
  w.participant_shm_ports = {7000, 16161};
  r.participant_shm_ports = {7000, 16163};
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_EQ(v.confidence, Confidence::Certain);
  EXPECT_EQ(v.locator.kind, LocatorKind::Invalid);
  EXPECT_TRUE(has(v.reasons, "same-host-guid"));
  EXPECT_TRUE(has(v.reasons, "both-shm-locators"));
  EXPECT_TRUE(has(v.reasons, "shm-port-collision"));
  EXPECT_FALSE(has(v.reasons, "shm-reader-port-not-visible"));
  EXPECT_EQ(v.warnings, (std::vector<std::string>{"shm-ipc-namespace-split"}));

  // Humble: only the pid-based port, numbered per namespace, so the endpoint locators collide
  auto hw = make(true, HOST_A, {shm(16911)});
  auto hr = make(false, HOST_A, {shm(16911)});
  hw.participant_guid_prefix = "P1";
  hr.participant_guid_prefix = "P2";
  EXPECT_TRUE(has(decide(hw, hr).reasons, "shm-port-collision"));

  // one participant writing to itself shares its ports with itself
  hr.participant_guid_prefix = "P1";
  EXPECT_EQ(decide(hw, hr).transport, Transport::SHM);
}

TEST(Decision, OneSideVisibleFromTheToolIsAnIpcSplit)
{
  auto w = make(true, HOST_A, {shm(16163)});
  auto r = make(false, HOST_A, {shm(16165)});
  w.participant_guid_prefix = "P1";
  r.participant_guid_prefix = "P2";
  w.participant_shm_visibility = ShmVisibility::Visible;
  r.participant_shm_visibility = ShmVisibility::NotVisible;
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_EQ(v.confidence, Confidence::Certain);
  EXPECT_TRUE(has(v.reasons, "shm-reader-port-not-visible"));
  EXPECT_FALSE(has(v.reasons, "shm-port-collision"));
  EXPECT_TRUE(has(v.warnings, "shm-ipc-namespace-split"));

  std::swap(w.participant_shm_visibility, r.participant_shm_visibility);
  v = decide(w, r);
  EXPECT_TRUE(has(v.reasons, "shm-writer-port-not-visible"));
  EXPECT_TRUE(has(v.warnings, "shm-ipc-namespace-split"));

  // both outside the tool's namespace, or not decidable: they may share another one
  for (auto [wv, rv] : {std::pair{ShmVisibility::NotVisible, ShmVisibility::NotVisible},
      std::pair{ShmVisibility::Visible, ShmVisibility::Visible},
      std::pair{ShmVisibility::Visible, ShmVisibility::Unprobed},
      std::pair{ShmVisibility::Unprobed, ShmVisibility::NotVisible}})
  {
    w.participant_shm_visibility = wv;
    r.participant_shm_visibility = rv;
    v = decide(w, r);
    EXPECT_EQ(v.transport, Transport::SHM);
    EXPECT_TRUE(v.warnings.empty());
  }
}

TEST(Decision, PortCollisionAndOneSideVisibleAreBothListed)
{
  // the tool in the talker's IPC namespace, both nodes on the reader's 7000 port: the talker's
  // own port is held here, the listener's is not, so both signals tell the split (#118)
  auto w = make(true, HOST_A, {shm(7413)});
  auto r = make(false, HOST_A, {shm(7415)});
  w.participant_guid_prefix = "P1";
  r.participant_guid_prefix = "P2";
  w.participant_shm_ports = {7000, 7413};
  r.participant_shm_ports = {7000, 7415};
  w.participant_shm_visibility = ShmVisibility::Visible;
  r.participant_shm_visibility = ShmVisibility::NotVisible;
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_EQ(v.confidence, Confidence::Certain);
  auto pos = [&v](const std::string & reason) {
      return std::find(v.reasons.begin(), v.reasons.end(), reason) - v.reasons.begin();
    };
  ASSERT_TRUE(has(v.reasons, "shm-port-collision"));
  ASSERT_TRUE(has(v.reasons, "shm-reader-port-not-visible"));
  EXPECT_EQ(pos("shm-port-collision") + 1, pos("shm-reader-port-not-visible"));
  EXPECT_EQ(v.warnings, (std::vector<std::string>{"shm-ipc-namespace-split"}));
}

TEST(Decision, IpcSplitRuleOrderAndScope)
{
  auto split_pair = [](DataSharingKind ds) {
      auto w = make(true, HOST_A, {shm(16911)}, ds);
      auto r = make(false, HOST_A, {shm(16911)}, ds);
      w.participant_guid_prefix = "P1";
      r.participant_guid_prefix = "P2";
      return std::pair{w, r};
    };

  // QoS incompatibility wins: the endpoints do not even match
  auto [w, r] = split_pair(DataSharingKind::Off);
  w.qos.reliability = "BEST_EFFORT";
  r.qos.reliability = "RELIABLE";
  auto v = decide(w, r);
  EXPECT_TRUE(has(v.warnings, "qos-incompatible"));
  EXPECT_FALSE(has(v.warnings, "shm-ipc-namespace-split"));
  EXPECT_FALSE(has(v.reasons, "shm-port-collision"));

  // data-sharing endpoints in two IPC namespaces lose every sample too (#110)
  std::tie(w, r) = split_pair(DataSharingKind::On);
  v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_TRUE(has(v.warnings, "shm-ipc-namespace-split"));
  EXPECT_TRUE(has(v.reasons, "datasharing-qos-enabled-both"));
  EXPECT_TRUE(has(v.reasons, "shm-port-collision"));
  EXPECT_FALSE(has(v.reasons, "datasharing-unverified-by-traffic"));

  // another host id: not an SHM pair at all
  std::tie(w, r) = split_pair(DataSharingKind::Off);
  r.host_id = HOST_B;
  v = decide(w, r);
  EXPECT_FALSE(has(v.warnings, "shm-ipc-namespace-split"));
  EXPECT_FALSE(has(v.reasons, "shm-port-collision"));

  // multicast SHM numbers are shared on purpose: only unicast ports collide
  w = make(true, HOST_A, {shm(16161)});
  r = make(false, HOST_A, {shm(16163)});
  w.participant_guid_prefix = "P1";
  r.participant_guid_prefix = "P2";
  w.multicast.push_back(shm(7400));
  r.multicast.push_back(shm(7400));
  v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::SHM);
  EXPECT_TRUE(v.warnings.empty());
}

TEST(Decision, DataSharingIpcSplit)
{
  // UDPv4 only: data-sharing does not need the SHM transport
  auto w = make(true, HOST_A, {udp4("10.0.0.1")}, DataSharingKind::On, {1});
  auto r = make(false, HOST_A, {udp4("10.0.0.1", 7413)}, DataSharingKind::On, {1});
  w.participant_guid_prefix = "P1";
  r.participant_guid_prefix = "P2";
  auto set = [&](ShmVisibility wv, ShmVisibility rv) {
      w.datasharing_segment_visibility = wv;
      r.datasharing_segment_visibility = rv;
      return decide(w, r);
    };

  // no listing, both segments here, or both elsewhere (a third namespace): no evidence
  using SV = ShmVisibility;
  for (auto [wv, rv] : {std::pair{SV::Unprobed, SV::Unprobed}, {SV::Visible, SV::Visible},
      {SV::NotVisible, SV::NotVisible}, {SV::Visible, SV::Unprobed}})
  {
    auto v = set(wv, rv);
    EXPECT_EQ(v.transport, Transport::DataSharing);
    EXPECT_EQ(v.confidence, Confidence::Likely);
    EXPECT_TRUE(has(v.reasons, "datasharing-unverified-by-traffic"));
    EXPECT_TRUE(v.warnings.empty());
  }

  // one segment here and the other not: two /dev/shm
  auto v = set(SV::Visible, SV::NotVisible);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_EQ(v.confidence, Confidence::Certain);
  EXPECT_EQ(
    v.reasons, (std::vector<std::string>{"same-host-guid", "datasharing-qos-enabled-both",
      "datasharing-domain-ids-match", "datasharing-reader-segment-not-visible"}));
  EXPECT_EQ(v.warnings, (std::vector<std::string>{"shm-ipc-namespace-split"}));
  v = set(SV::NotVisible, SV::Visible);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_TRUE(has(v.reasons, "datasharing-writer-segment-not-visible"));

  // one participant has one /dev/shm
  r.participant_guid_prefix = "P1";
  v = set(SV::Visible, SV::NotVisible);
  EXPECT_EQ(v.transport, Transport::DataSharing);

  // data-sharing off: not a data-sharing pair, the segments say nothing
  r.participant_guid_prefix = "P2";
  r.qos.data_sharing = DataSharingKind::Off;
  v = set(SV::Visible, SV::NotVisible);
  EXPECT_EQ(v.transport, Transport::UDPv4);
  EXPECT_FALSE(has(v.warnings, "shm-ipc-namespace-split"));

  // with SHM on both sides, every signal that fires is listed after both-shm-locators
  w = make(true, HOST_A, {shm(7000)}, DataSharingKind::On, {1});
  r = make(false, HOST_A, {shm(7000)}, DataSharingKind::On, {1});
  w.participant_guid_prefix = "P1";
  r.participant_guid_prefix = "P2";
  v = set(SV::Visible, SV::NotVisible);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_EQ(
    v.reasons, (std::vector<std::string>{"same-host-guid", "datasharing-qos-enabled-both",
      "datasharing-domain-ids-match", "both-shm-locators", "shm-port-collision",
      "datasharing-reader-segment-not-visible"}));
  EXPECT_EQ(v.warnings, (std::vector<std::string>{"shm-ipc-namespace-split"}));
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

TEST(RosNames, UnknownNodeNameBecomesEmpty)
{
  // what rclcpp reports without the node's ros_discovery_info sample (#112)
  EXPECT_EQ(normalize_node_name("_NODE_NAMESPACE_UNKNOWN_/_NODE_NAME_UNKNOWN_"), "");
  EXPECT_EQ(normalize_node_name("/talker"), "/talker");
  EXPECT_EQ(normalize_node_name(""), "");
  EXPECT_EQ(fully_qualified_node_name("", "talker"), "/talker");
  EXPECT_EQ(fully_qualified_node_name("/", "talker"), "/talker");
  EXPECT_EQ(fully_qualified_node_name("/robot1", "talker"), "/robot1/talker");
  EXPECT_EQ(
    fully_qualified_node_name("_NODE_NAMESPACE_UNKNOWN_", "_NODE_NAME_UNKNOWN_"), kUnknownNodeName);
}

TEST(RosNames, GraphIsQueriedOnlyWhileItChanges)
{
  // the first frame, and the one-shot run, always query
  EXPECT_TRUE(should_refresh_graph(false, 0, 0, 60.0));
  // an endpoint or a ros_discovery_info sample since the last query
  EXPECT_TRUE(should_refresh_graph(true, 12, 11, 60.0));
  // the node names of the last endpoints may still be on their way
  EXPECT_TRUE(should_refresh_graph(true, 12, 12, kGraphRefreshGraceSeconds));
  // a quiet graph costs nothing
  EXPECT_FALSE(should_refresh_graph(true, 12, 12, kGraphRefreshGraceSeconds + 0.1));
}

TEST(RosNames, GraphNameWinsOverDiscoveryInfo)
{
  EXPECT_EQ(merge_node_name("/talker", "/other"), "/talker");
  EXPECT_EQ(merge_node_name("", "/talker"), "/talker");
  EXPECT_EQ(merge_node_name(kUnknownNodeName, "/talker"), "/talker");
  EXPECT_EQ(merge_node_name(kUnknownNodeName, ""), "");
  EXPECT_EQ(merge_node_name("", kUnknownNodeName), "");
}

TEST(RosNames, NodeNameTableReplacesAParticipantsEntries)
{
  const auto gid = [](uint8_t participant, uint8_t entity) {
      EndpointGid g{};
      g[0] = participant;
      g[15] = entity;
      return g;
    };
  const ParticipantPrefix p1{1}, p2{2};
  NodeNameTable table;
  table.update(p1, {{"", "talker", {gid(1, 4)}, {gid(1, 3)}}, {"/robot", "cam", {}, {gid(1, 7)}}});
  table.update(p2, {{"/", "listener", {gid(2, 4)}, {}}});
  EXPECT_EQ(table.size(), 4u);
  EXPECT_EQ(table.lookup(gid(1, 3)), "/talker");     // writer
  EXPECT_EQ(table.lookup(gid(1, 4)), "/talker");     // reader
  EXPECT_EQ(table.lookup(gid(1, 7)), "/robot/cam");
  EXPECT_EQ(table.lookup(gid(2, 4)), "/listener");
  EXPECT_EQ(table.lookup(gid(3, 4)), "");
  // the next sample of a participant lists all of its nodes: the camera node is gone
  table.update(p1, {{"", "talker", {gid(1, 4)}, {gid(1, 3), gid(1, 9)}}});
  EXPECT_EQ(table.size(), 4u);
  EXPECT_EQ(table.lookup(gid(1, 7)), "");
  EXPECT_EQ(table.lookup(gid(1, 9)), "/talker");
  EXPECT_EQ(table.lookup(gid(2, 4)), "/listener");   // other participants untouched
  table.update(p1, {});
  EXPECT_EQ(table.size(), 1u);
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

TEST(ApplyStats, IpcSplitKeepsNoneDespiteShmTraffic)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(16161)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 16163), shm(16163)}));
  eps[0].participant_guid_prefix = "P1";
  eps[1].participant_guid_prefix = "P2";
  eps[0].participant_shm_visibility = ShmVisibility::Visible;
  eps[1].participant_shm_visibility = ShmVisibility::NotVisible;
  auto topics = summarize(eps);
  // the writer pushes into the reader's port in its own /dev/shm: counted, never delivered
  auto stats = stats_with(eps[0], {TrafficSample{"P1", shm(16163), 10, 1632.0}});
  apply_stats(topics, stats);
  auto & p = topics[0].pairs[0];
  EXPECT_EQ(p.verdict.transport, Transport::None);
  EXPECT_EQ(p.verdict.confidence, Confidence::Certain);
  EXPECT_TRUE(has(p.verdict.reasons, "measured-shm-traffic"));
  EXPECT_EQ(p.verdict.warnings, (std::vector<std::string>{"shm-ipc-namespace-split"}));

  topics = summarize(eps);
  stats = stats_with(eps[0], {TrafficSample{"P1", shm(16163), 10, 1632.0}}, true, &eps[1]);
  apply_stats(topics, stats);
  EXPECT_TRUE(has(topics[0].pairs[0].verdict.warnings, "shm-ipc-namespace-split-but-delivered"));
  EXPECT_EQ(topics[0].pairs[0].verdict.transport, Transport::None);

  // nothing is expected to flow, so a writer without statistics is not worth a warning
  topics = summarize(eps);
  StatsData none;
  none.enabled = true;
  apply_stats(topics, none);
  EXPECT_EQ(
    topics[0].pairs[0].verdict.warnings, (std::vector<std::string>{"shm-ipc-namespace-split"}));
}

TEST(ApplyStats, IpcSplitWarnsAboutNonShmTrafficInTheWindow)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(16161)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 16163), shm(16163)}));
  eps[0].participant_guid_prefix = "P1";
  eps[1].participant_guid_prefix = "P2";
  eps[0].participant_shm_visibility = ShmVisibility::Visible;
  eps[1].participant_shm_visibility = ShmVisibility::NotVisible;
  const auto udp = udp4("10.0.0.1", 16163);
  const std::string split = "shm-ipc-namespace-split";
  const std::string non_shm = "shm-ipc-namespace-split-but-non-shm-traffic";
  auto verdict_with = [&eps](std::vector<TrafficSample> traffic, bool delivered) {
      auto topics = summarize(eps);
      const auto stats = stats_with(eps[0], std::move(traffic), delivered, &eps[1]);
      apply_stats(topics, stats);
      return topics[0].pairs[0].verdict;
    };

  // Fast DDS keeps same-host traffic of two SHM participants on SHM: UDP contradicts the split
  auto v = verdict_with(
    {TrafficSample{"P1", shm(16163), 10, 1632.0}, TrafficSample{"P1", udp, 4, 400.0}}, false);
  EXPECT_EQ(v.transport, Transport::None);
  EXPECT_EQ(v.confidence, Confidence::Certain);
  EXPECT_TRUE(has(v.reasons, "measured-udpv4-traffic"));
  EXPECT_EQ(v.warnings, (std::vector<std::string>{split, non_shm}));

  // UDP packets from before the observation only: the kind is measured, the link is quiet now
  v = verdict_with({TrafficSample{"P1", udp, 4, 400.0, 4, 400.0}}, false);
  EXPECT_TRUE(has(v.reasons, "measured-udpv4-traffic"));
  EXPECT_EQ(v.warnings, (std::vector<std::string>{split}));

  // SHM traffic alone is expected
  v = verdict_with({TrafficSample{"P1", shm(16163), 10, 1632.0}}, false);
  EXPECT_EQ(v.warnings, (std::vector<std::string>{split}));

  // a proven delivery is reported on its own
  v = verdict_with({TrafficSample{"P1", udp, 4, 400.0}}, true);
  EXPECT_EQ(
    v.warnings,
    (std::vector<std::string>{split, "shm-ipc-namespace-split-but-delivered", non_shm}));
  EXPECT_EQ(v.transport, Transport::None);

  // a data-sharing split whose reader has no SHM: the two participants' other endpoints use UDP
  std::vector<Endpoint> ds;
  ds.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}, DataSharingKind::On, {1}));
  ds.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7413)}, DataSharingKind::On, {1}));
  ds[0].participant_guid_prefix = "P1";
  ds[1].participant_guid_prefix = "P2";
  ds[0].datasharing_segment_visibility = ShmVisibility::Visible;
  ds[1].datasharing_segment_visibility = ShmVisibility::NotVisible;
  auto topics = summarize(ds);
  const auto stats = stats_with(ds[0], {TrafficSample{"P1", udp4("10.0.0.1", 7413), 4, 400.0}});
  apply_stats(topics, stats);
  const auto & dv = topics[0].pairs[0].verdict;
  EXPECT_EQ(dv.transport, Transport::None);
  EXPECT_FALSE(has(dv.reasons, "both-shm-locators"));
  EXPECT_TRUE(has(dv.reasons, "measured-udpv4-traffic"));
  EXPECT_EQ(dv.warnings, (std::vector<std::string>{split}));
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
  auto stats = stats_with(
    eps[0], {
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
  const auto & ds = *std::find_if(
    topics[0].pairs.begin(), topics[0].pairs.end(),
    [&](const Pair & q) {return q.reader == &eps[1];});
  EXPECT_EQ(ds.verdict.transport, Transport::DataSharing);
  EXPECT_EQ(ds.verdict.confidence, Confidence::Likely);
  EXPECT_TRUE(has(ds.verdict.reasons, "datasharing-ambiguous-mixed-readers"));
  EXPECT_TRUE(ds.verdict.warnings.empty());
}

TEST(ApplyStats, DataSharingSplitReaderGetsNoDataAndKeepsItsVerdict)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {shm(7415)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7413)}, DataSharingKind::On, {1}));
  eps.push_back(make(false, HOST_A, {shm(7417)}, DataSharingKind::On, {1}));   // other /dev/shm
  eps[0].participant_guid_prefix = "P1";
  eps[1].participant_guid_prefix = "P2";
  eps[2].participant_guid_prefix = "P3";
  eps[0].datasharing_segment_visibility = ShmVisibility::Visible;
  eps[1].datasharing_segment_visibility = ShmVisibility::Visible;
  eps[2].datasharing_segment_visibility = ShmVisibility::NotVisible;
  auto topics = summarize(eps);
  ASSERT_EQ(topics[0].pairs.size(), 2u);
  // heartbeats to both readers, but no DATA through a transport
  auto stats = stats_with(
    eps[0], {TrafficSample{"P1", shm(7413), 3, 300.0}, TrafficSample{"P1", shm(7417), 3, 300.0}},
    true, &eps[1]);
  stats.data_count[eps[0].guid] = DataCountSample{5, 5, 6};
  stats.statistics_writers.insert({"P1", kStatsDataCountTopic});
  apply_stats(topics, stats);
  auto pair_of = [&](const Endpoint & reader) {
      return *std::find_if(
        topics[0].pairs.begin(), topics[0].pairs.end(),
        [&](const Pair & q) {return q.reader == &reader;});
    };
  // the split reader is not a transport reader: the other pair is still confirmed
  const auto ds = pair_of(eps[1]);
  EXPECT_EQ(ds.verdict.transport, Transport::DataSharing);
  EXPECT_EQ(ds.verdict.confidence, Confidence::Certain);
  EXPECT_TRUE(has(ds.verdict.reasons, "datasharing-confirmed-no-data-submessages"));
  // heartbeats to the split reader confirm nothing; it stays NONE
  const auto split = pair_of(eps[2]);
  EXPECT_EQ(split.verdict.transport, Transport::None);
  EXPECT_EQ(split.verdict.confidence, Confidence::Certain);
  EXPECT_EQ(split.verdict.warnings, (std::vector<std::string>{"shm-ipc-namespace-split"}));
  EXPECT_TRUE(has(split.verdict.reasons, "datasharing-reader-segment-not-visible"));
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

TEST(ApplyStats, WindowDeltas)
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
  apply_stats(topics, stats);
  const auto & topic = topics[0];
  const auto & p = *std::find_if(
    topic.pairs.begin(), topic.pairs.end(),
    [&](const Pair & q) {return q.writer == &eps[0];});
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

TEST(ApplyStats, NoWriterInstanceLimitSuspectedWithoutTheLimit)
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
  stats.writer_instance_limit = false;   // Fast DDS 3.5 and later
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_TRUE(p.measured.transports.empty());
  EXPECT_FALSE(has(p.verdict.warnings, "stats-writer-instance-limit-suspected"));
  EXPECT_TRUE(has(p.verdict.warnings, "no-traffic-observed"));
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

TEST(Codes, RemediesAreOneSentenceAndExplicitPerCode)
{
  // The table forces every entry to state a remedy or std::nullopt; here the content:
  // a remedy is one sentence that names what to change, a description no longer carries it.
  size_t with_remedy = 0;
  for (const auto & c : known_codes()) {
    auto r = remedy(c);
    if (!r) {continue;}
    ++with_remedy;
    EXPECT_FALSE(r->empty()) << c;
    EXPECT_EQ(r->back(), '.') << c << ": " << *r;
    EXPECT_LT(r->size(), 320u) << c << " is not a one-liner: " << *r;
  }
  EXPECT_GE(with_remedy, 25u);
  EXPECT_LT(with_remedy, known_codes().size()) << "some codes describe a normal state";
  EXPECT_FALSE(remedy("not-a-code").has_value());

  // codes that describe a normal state, a measured fact or ask for a bug report: nothing to fix
  for (const auto * c : {"same-host-guid", "both-shm-locators", "common-udpv4-locator",
      "measured-shm-traffic", "measured-transport-mismatch", "qos-incompatible-but-delivered",
      "qos-incompatible", "datasharing-confirmed-no-traffic", "shm-port-collision",
      "shm-reader-port-not-visible", "shm-writer-port-not-visible",
      "datasharing-reader-segment-not-visible", "datasharing-writer-segment-not-visible",
      "shm-ipc-namespace-split-but-delivered", "shm-ipc-namespace-split-but-non-shm-traffic",
      "buffer-companion-folded", "buffer-companion"})
  {
    EXPECT_FALSE(remedy(c).has_value()) << c;
  }
  // codes whose remedy is the whole point of --advise
  EXPECT_NE(remedy("shm-stale-files")->find("fastdds shm clean"), std::string::npos);
  EXPECT_NE(remedy("reader-no-shm-locator")->find("FASTDDS_BUILTIN_TRANSPORTS"), std::string::npos);
  EXPECT_NE(remedy("datasharing-disabled-writer")->find("data_sharing"), std::string::npos);
  EXPECT_NE(remedy("qos-incompatible-reliability")->find("create_subscription"), std::string::npos);
  EXPECT_NE(remedy("stats-not-enabled-on-writer")->find("FASTDDS_STATISTICS"), std::string::npos);
  EXPECT_NE(remedy("shm-ipc-namespace-split")->find("ipc: host"), std::string::npos);
  EXPECT_NE(remedy("shm-ipc-namespace-split")->find("data_sharing OFF"), std::string::npos);
  // the remedy moved out of the description: it is said once
  EXPECT_EQ(explain("shm-stale-files").find("fastdds shm clean"), std::string::npos);
  EXPECT_EQ(explain("shm-nearly-full").find("--shm-size"), std::string::npos);
  EXPECT_EQ(explain("no-traffic-observed").find("--timeout"), std::string::npos);
  EXPECT_EQ(explain("stats-not-enabled-on-writer").find("Start it"), std::string::npos);
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
  eps[1].multicast = {udp4("239.255.0.1", 7400)};
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", udp4("10.0.0.2", 7413), 50, 5000.0}});
  stats.statistics_writers.insert({"P2", kStatsRtpsLostTopic});
  // the reader's participant missed 3 packets from the writer's participant on its unicast
  // locator during the window
  stats.lost.push_back(TrafficSample{"P1", udp4("10.0.0.2", 7413), 7, 700.0, 4, 400.0, 2, "P2"});
  // not counted: the reversed shape (the writer's participant missing packets from the
  // reader's participant on its own locator) ...
  stats.lost.push_back(TrafficSample{"P2", udp4("10.0.0.1", 7411), 100, 0.0, 0, 0.0, 2, "P1"});
  // ... the reader's multicast locator ...
  stats.lost.push_back(
    TrafficSample{"P1", udp4("239.255.0.1", 7400), 100, 0.0, 0, 0.0, 2, "P2"});
  // ... and another receiver of the same locator
  stats.lost.push_back(TrafficSample{"P1", udp4("10.0.0.2", 7413), 100, 0.0, 0, 0.0, 2, "P9"});
  stats.resent_datas[eps[0].guid] = DataCountSample{10, 12, 2};
  stats.heartbeats[eps[0].guid] = DataCountSample{0, 40, 5};
  stats.gaps[eps[0].guid] = DataCountSample{1, 1, 2};
  stats.acknacks[eps[1].guid] = DataCountSample{5, 9, 3};
  stats.nackfrags[eps[1].guid] = DataCountSample{0, 0, 1};
  apply_stats(topics, stats);
  {
    const auto & r = topics[0].pairs[0].measured.reliability;
    EXPECT_TRUE(r.available);
    EXPECT_TRUE(r.lost_available);
    EXPECT_EQ(r.lost_packets, 3u);
    EXPECT_EQ(r.resent, 2u);
    EXPECT_EQ(r.heartbeats, 40u);
    EXPECT_EQ(r.gaps, 0u);
    EXPECT_EQ(r.acknacks, 4u);
    EXPECT_EQ(r.nackfrags, 0u);
    EXPECT_TRUE(has(topics[0].pairs[0].verdict.warnings, "rtps-packets-lost"));
    EXPECT_TRUE(topics[0].reliability_available);
    EXPECT_TRUE(topics[0].lost_available);
    EXPECT_EQ(topics[0].lost_packets, 3u);
    EXPECT_EQ(topics[0].resent, 2u);
  }

  // two pairs between the same participants: the loss belongs to the participant pair, so
  // both pairs show it and the topic counts it once
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}));
  eps[2].participant_guid_prefix = "P1";
  topics = summarize(eps);
  apply_stats(topics, stats);
  ASSERT_EQ(topics[0].pairs.size(), 2u);
  for (const auto & p : topics[0].pairs) {
    EXPECT_EQ(p.measured.reliability.lost_packets, 3u);
    EXPECT_TRUE(has(p.verdict.warnings, "rtps-packets-lost"));
  }
  EXPECT_EQ(topics[0].lost_packets, 3u);
  eps.pop_back();

  // loopback: the reader next to the tool is announced as 127.0.0.1, the remote writer
  // addresses its real address
  eps[1].unicast = {udp4("127.0.0.1", 7413)};
  topics = summarize(eps);
  stats.local_addresses = {"10.0.0.2"};
  apply_stats(topics, stats);
  EXPECT_EQ(topics[0].pairs[0].measured.reliability.lost_packets, 3u);

  // the reader's participant publishes RTPS_LOST but reported no gap: nothing lost
  stats.lost.clear();
  topics = summarize(eps);
  apply_stats(topics, stats);
  {
    const auto & r = topics[0].pairs[0].measured.reliability;
    EXPECT_TRUE(r.lost_available);
    EXPECT_EQ(r.lost_packets, 0u);
    EXPECT_FALSE(has(topics[0].pairs[0].verdict.warnings, "rtps-packets-lost"));
    EXPECT_TRUE(topics[0].lost_available);
    EXPECT_EQ(topics[0].lost_packets, 0u);
  }

  // without its RTPS_LOST writer the loss is unknown; the other counters stay
  stats.statistics_writers.clear();
  topics = summarize(eps);
  apply_stats(topics, stats);
  {
    const auto & r = topics[0].pairs[0].measured.reliability;
    EXPECT_TRUE(r.available);
    EXPECT_FALSE(r.lost_available);
    EXPECT_EQ(r.resent, 2u);
    EXPECT_EQ(r.acknacks, 4u);
    EXPECT_TRUE(topics[0].reliability_available);
    EXPECT_FALSE(topics[0].lost_available);
    EXPECT_EQ(topics[0].resent, 2u);
  }

  // no counters at all
  topics = summarize(eps);
  apply_stats(topics, stats_with(eps[0], {}));
  EXPECT_FALSE(topics[0].pairs[0].measured.reliability.available);
  EXPECT_FALSE(topics[0].pairs[0].measured.reliability.lost_available);
  EXPECT_FALSE(topics[0].reliability_available);
  EXPECT_FALSE(topics[0].lost_available);
  EXPECT_FALSE(has(topics[0].pairs[0].verdict.warnings, "rtps-packets-lost"));
}

TEST(Decision, SelectedLocatorIsTheMatchingReaderLocator)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1")});
  auto r = make(false, HOST_B, {udp4("10.0.0.2", 7413)});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::UDPv4);
  EXPECT_TRUE(v.locator == udp4("10.0.0.2", 7413));
  EXPECT_FALSE(v.locator_multicast);
}

TEST(Decision, ShmVerdictSelectsTheReadersShmLocator)
{
  // The reader's SHM locator names the /dev/shm port the writer writes into, so it is
  // selected the same way a network locator is.
  auto w = make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)});
  auto r = make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)});
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::SHM);
  EXPECT_TRUE(v.locator == shm(7413));
  EXPECT_FALSE(v.locator_multicast);
}

TEST(Decision, NoSelectedLocatorWhenThereIsNoPath)
{
  auto w6 = make(true, HOST_A, {udp4("10.0.0.1")});
  auto r6 = make(false, HOST_B, {Locator{LocatorKind::UDPv6, "::1", 7411}});
  auto none_verdict = decide(w6, r6);
  EXPECT_EQ(none_verdict.transport, Transport::None);
  EXPECT_EQ(none_verdict.locator.kind, LocatorKind::Invalid);
}

TEST(Decision, SelectedLocatorCanBeMulticast)
{
  auto w = make(true, HOST_A, {udp4("10.0.0.1")});
  auto r = make(false, HOST_B, {});
  r.multicast.push_back(udp4("239.255.0.1", 7400));
  auto v = decide(w, r);
  EXPECT_EQ(v.transport, Transport::UDPv4);
  EXPECT_TRUE(v.locator == udp4("239.255.0.1", 7400));
  EXPECT_TRUE(v.locator_multicast);
}

TEST(ApplyStats, MeasuredLocatorsBreakDownTheTraffic)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(
    eps[0], {TrafficSample{"P1", shm(7413), 10, 1000.0},
      TrafficSample{"P1", udp4("10.0.0.1", 7413), 4, 400.0}});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  ASSERT_EQ(p.measured.locators.size(), 2u);
  uint64_t packets = 0;
  double bytes = 0.0;
  for (const auto & ml : p.measured.locators) {
    packets += ml.packets;
    bytes += ml.bytes;
  }
  EXPECT_EQ(packets, p.measured.packets);   // the entries are a breakdown of the total
  EXPECT_DOUBLE_EQ(bytes, p.measured.bytes);
  EXPECT_EQ(p.measured.packets, 14u);
  // the SHM locator the verdict selected did carry packets
  EXPECT_TRUE(p.verdict.locator == shm(7413));
  EXPECT_FALSE(has(p.verdict.warnings, "measured-locator-mismatch"));
}

TEST(ApplyStats, TransportMismatchDoesNotAlsoReportALocatorMismatch)
{
  // Predicted SHM, packets only on the reader's UDPv4 locator: the kind is already
  // wrong, and saying the locator is wrong too would report one fact twice.
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_A, {udp4("10.0.0.1"), shm(7415)}));
  eps.push_back(make(false, HOST_A, {udp4("10.0.0.1", 7413), shm(7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", udp4("10.0.0.1", 7413), 5, 500.0}});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_TRUE(has(p.verdict.warnings, "measured-transport-mismatch"));
  EXPECT_FALSE(has(p.verdict.warnings, "measured-locator-mismatch"));
}

TEST(ApplyStats, LocatorMismatchWhenTheSelectedLocatorCarriedNothing)
{
  // Multi-homed reader: the prediction selects the first announced locator, the packets
  // went to the second one. The transport kind still agrees.
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_B, {udp4("192.168.1.6")}));
  eps.push_back(make(false, HOST_A, {udp4("192.168.1.20", 7413), udp4("10.0.0.5", 7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", udp4("10.0.0.5", 7413), 9, 900.0}});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  EXPECT_TRUE(p.verdict.locator == udp4("192.168.1.20", 7413));
  EXPECT_EQ(p.verdict.transport, Transport::UDPv4);
  EXPECT_FALSE(has(p.verdict.warnings, "measured-transport-mismatch"));
  EXPECT_TRUE(has(p.verdict.warnings, "measured-locator-mismatch"));
}

TEST(ApplyStats, NoLocatorMismatchWhenTheSelectedLocatorAlsoCarriedTraffic)
{
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_B, {udp4("192.168.1.6")}));
  eps.push_back(make(false, HOST_A, {udp4("192.168.1.20", 7413), udp4("10.0.0.5", 7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(
    eps[0], {TrafficSample{"P1", udp4("192.168.1.20", 7413), 9, 900.0},
      TrafficSample{"P1", udp4("10.0.0.5", 7413), 3, 300.0}});
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  ASSERT_EQ(p.measured.locators.size(), 2u);
  EXPECT_FALSE(has(p.verdict.warnings, "measured-locator-mismatch"));
}

TEST(ApplyStats, LoopbackRewriteIsNotALocatorMismatch)
{
  // The reader is announced as 127.0.0.1 while the remote writer's RTPS_SENT names the
  // host's real address: the same locator, spelled two ways.
  std::vector<Endpoint> eps;
  eps.push_back(make(true, HOST_B, {udp4("192.168.1.6")}));
  eps.push_back(make(false, HOST_A, {udp4("127.0.0.1", 7413)}));
  eps[0].participant_guid_prefix = "P1";
  auto topics = summarize(eps);
  auto stats = stats_with(eps[0], {TrafficSample{"P1", udp4("192.168.1.8", 7413), 7, 700.0}});
  stats.local_addresses = {"192.168.1.8"};
  apply_stats(topics, stats);
  const auto & p = topics[0].pairs[0];
  ASSERT_EQ(p.measured.locators.size(), 1u);
  EXPECT_FALSE(has(p.verdict.warnings, "measured-locator-mismatch"));
}

TEST(Diff, GrowingLocatorCountersAreNotAChange)
{
  // PairState keeps locator identities only: MeasuredLocator's counters grow every frame
  // and would otherwise report every active pair as changed for as long as it is used.
  auto w = make(true, HOST_B, {udp4("192.168.1.6")});
  auto r = make(false, HOST_A, {udp4("10.0.0.5", 7413)});
  Pair pair;
  pair.writer = &w;
  pair.reader = &r;
  pair.verdict = decide(w, r);
  pair.measured.locators = {MeasuredLocator{udp4("10.0.0.5", 7413), 10, 1000.0}};
  const auto before = pair_state(pair);

  pair.measured.locators[0].packets = 999;
  pair.measured.locators[0].bytes = 99999.0;
  EXPECT_TRUE(pair_state(pair) == before);

  pair.measured.locators[0].locator = udp4("10.0.0.9", 7413);
  EXPECT_FALSE(pair_state(pair) == before);
}

// ---- two saved snapshots (transport_viz diff) -------------------------------------

namespace
{
Endpoint named(bool writer, const std::string & guid, const std::string & node)
{
  auto e = make(writer, HOST_A, {udp4("10.0.0.1"), shm()});
  e.guid = guid;
  e.node_name = node;
  return e;
}
Endpoint wr(const std::string & guid, const std::string & node) {return named(true, guid, node);}
Endpoint rd(const std::string & guid, const std::string & node) {return named(false, guid, node);}

template<class ... E>
Snapshot snapshot_of(E... eps)
{
  Snapshot s;
  s.endpoints = {eps ...};
  s.topics = summarize(s.endpoints);
  return s;
}
}  // namespace

TEST(DiffSnapshots, GuidKeySeesARestartAsRemovedPlusAdded)
{
  auto before = snapshot_of(wr("w1", "/talker"), rd("r1", "/listener"));
  auto after = snapshot_of(wr("w2", "/talker"), rd("r2", "/listener"));
  auto c = diff_snapshots(before, after, KeyMode::Guid);
  EXPECT_EQ(c.key, KeyMode::Guid);
  ASSERT_EQ(c.added.size(), 1u);
  ASSERT_EQ(c.removed.size(), 1u);
  EXPECT_TRUE(c.changed.empty());
  EXPECT_EQ(c.added[0].writer_guid, "w2");
  EXPECT_EQ(c.added[0].writer_node, "/talker");    // pair keys carry the node names
  EXPECT_EQ(c.removed[0].reader_guid, "r1");
  EXPECT_EQ(c.removed[0].reader_node, "/listener");
  EXPECT_FALSE(c.before.has_value());
}

TEST(DiffSnapshots, NodeKeySurvivesARestartAndReportsBothGuids)
{
  auto before = snapshot_of(wr("w1", "/talker"), rd("r1", "/listener"));
  auto after = snapshot_of(wr("w2", "/talker"), rd("r2", "/listener"));
  EXPECT_TRUE(diff_snapshots(before, after, KeyMode::Node).empty());

  // the listener lost its SHM locator: same nodes, the transport changed
  after.endpoints[1].unicast = {udp4("10.0.0.1")};
  after.topics = summarize(after.endpoints);
  auto c = diff_snapshots(before, after, KeyMode::Node);
  EXPECT_EQ(c.key, KeyMode::Node);
  EXPECT_TRUE(c.added.empty());
  EXPECT_TRUE(c.removed.empty());
  ASSERT_EQ(c.changed.size(), 1u);
  EXPECT_EQ(c.changed[0].key.writer_guid, "w2");           // the after identity ...
  EXPECT_EQ(c.changed[0].key.reader_guid, "r2");
  EXPECT_EQ(c.changed[0].before_key.writer_guid, "w1");    // ... and the before one
  EXPECT_EQ(c.changed[0].before_key.reader_guid, "r1");
  EXPECT_EQ(c.changed[0].from.transport, Transport::SHM);
  EXPECT_EQ(c.changed[0].to.transport, Transport::UDPv4);
}

TEST(DiffSnapshots, NodeKeyIgnoresRenumberedPortsButNotAddressesOrKinds)
{
  // the restarted listener got the next SHM port: not a change under the node key ...
  auto before = snapshot_of(wr("w1", "/talker"), rd("r1", "/listener"));
  auto after = snapshot_of(wr("w2", "/talker"), rd("r2", "/listener"));
  after.endpoints[1].unicast = {udp4("10.0.0.1", 7417), shm(7417)};
  after.topics = summarize(after.endpoints);
  EXPECT_EQ(after.topics[0].pairs[0].verdict.locator.port, 7417u);
  EXPECT_TRUE(diff_snapshots(before, after, KeyMode::Node).empty());
  // ... but under the GUID key the pairs are different ones anyway
  EXPECT_EQ(diff_snapshots(before, after, KeyMode::Guid).added.size(), 1u);
  // another address (a different interface) is a change under both keys
  auto other = snapshot_of(wr("w1", "/talker"), rd("r1", "/listener"));
  other.endpoints[1].unicast = {udp4("10.0.0.2", 7411)};   // UDPv4 only, other address
  other.topics = summarize(other.endpoints);
  auto udp = snapshot_of(wr("w1", "/talker"), rd("r1", "/listener"));
  udp.endpoints[1].unicast = {udp4("10.0.0.1", 7415)};
  udp.topics = summarize(udp.endpoints);
  EXPECT_EQ(diff_snapshots(udp, other, KeyMode::Guid).changed.size(), 1u);
  EXPECT_EQ(diff_snapshots(udp, other, KeyMode::Node).changed.size(), 1u);
  udp.endpoints[1].unicast = {udp4("10.0.0.2", 7419)};   // same address, another port
  udp.topics = summarize(udp.endpoints);
  EXPECT_TRUE(diff_snapshots(udp, other, KeyMode::Node).empty());
  EXPECT_EQ(diff_snapshots(udp, other, KeyMode::Guid).changed.size(), 1u);
}

TEST(DiffSnapshots, NodeKeyMatchesSeveralEndpointsOfOneNodeInGuidOrder)
{
  // one node with two writers on the topic, one reader: two pairs of the same node pair
  auto before = snapshot_of(wr("w1", "/multi"), wr("w2", "/multi"), rd("r1", "/sink"));
  auto after = snapshot_of(wr("w8", "/multi"), wr("w9", "/multi"), rd("r7", "/sink"));
  EXPECT_TRUE(diff_snapshots(before, after, KeyMode::Node).empty());
  // a third writer of the same node appears: one added pair, the others still match
  after.endpoints.push_back(wr("w7", "/multi"));
  after.topics = summarize(after.endpoints);
  auto c = diff_snapshots(before, after, KeyMode::Node);
  ASSERT_EQ(c.added.size(), 1u);
  EXPECT_TRUE(c.removed.empty());
  EXPECT_TRUE(c.changed.empty());
  EXPECT_EQ(c.added[0].writer_guid, "w9");   // the highest GUID is the new ordinal
}

TEST(DiffSnapshots, NodeKeyFallsBackToTheGuidWithoutANodeName)
{
  auto before = snapshot_of(wr("w1", ""), rd("r1", "/sink"));
  auto same = snapshot_of(wr("w1", ""), rd("r9", "/sink"));
  auto other = snapshot_of(wr("w2", ""), rd("r9", "/sink"));
  EXPECT_TRUE(diff_snapshots(before, same, KeyMode::Node).empty());
  auto c = diff_snapshots(before, other, KeyMode::Node);
  EXPECT_EQ(c.added.size(), 1u);
  EXPECT_EQ(c.removed.size(), 1u);
  EXPECT_EQ(c.removed[0].writer_guid, "w1");
}

TEST(DiffSnapshots, RemovedTopicAndAddedTopic)
{
  auto before = snapshot_of(wr("w1", "/a"), rd("r1", "/b"));
  auto after = snapshot_of(wr("w2", "/a"), rd("r2", "/b"));
  after.endpoints[0].ros_topic = "/other";
  after.endpoints[0].dds_topic = "rt/other";
  after.endpoints[1].ros_topic = "/other";
  after.endpoints[1].dds_topic = "rt/other";
  after.topics = summarize(after.endpoints);
  auto c = diff_snapshots(before, after, KeyMode::Node);
  ASSERT_EQ(c.added.size(), 1u);
  ASSERT_EQ(c.removed.size(), 1u);
  EXPECT_EQ(c.added[0].topic, "/other");
  EXPECT_EQ(c.removed[0].topic, "/chatter");
}

// ---- rmw native-buffer companions (<topic>/_buf_cpu) -------------------------------

namespace
{
/// An endpoint of participant `prefix` on `topic` with entity key `key` (GUID bytes 12..14).
Endpoint entity(
  bool writer, const std::string & topic, uint32_t key, const std::string & prefix = "P1")
{
  Endpoint e = make(writer, HOST_A, {shm()});
  e.participant_guid_prefix = prefix;
  e.dds_topic = "rt" + topic;
  e.ros_topic = topic;
  e.dds_type = "std_msgs::msg::dds_::UInt8MultiArray_";
  e.ros_type = "std_msgs/msg/UInt8MultiArray";
  e.guid_bytes[12] = static_cast<uint8_t>(key >> 16);
  e.guid_bytes[13] = static_cast<uint8_t>(key >> 8);
  e.guid_bytes[14] = static_cast<uint8_t>(key);
  e.guid_bytes[15] = writer ? 0x03 : 0x04;
  e.guid = prefix + "|" + std::to_string(key);
  return e;
}

const TopicSummary & topic_named(const std::vector<TopicSummary> & topics, const std::string & n)
{
  for (const auto & t : topics) {
    if (t.display_topic == n) {return t;}
  }
  throw std::runtime_error("no topic " + n);
}

/// The codes of a topic's pairs and those of the topic itself.
std::vector<std::string> all_codes(const TopicSummary & t)
{
  std::vector<std::string> out = t.unmatched_reasons;
  for (const auto & p : t.pairs) {
    out.insert(out.end(), p.verdict.reasons.begin(), p.verdict.reasons.end());
  }
  return out;
}

/// Parent and companion writer in P1, parent and companion reader in P2, as Lyrical creates them.
std::vector<Endpoint> lyrical_large_array()
{
  std::vector<Endpoint> eps;
  eps.push_back(entity(true, "/large_array", 1));
  eps.push_back(entity(true, "/large_array/_buf_cpu", 2));
  eps.push_back(entity(false, "/large_array", 1, "P2"));
  eps.push_back(entity(false, "/large_array/_buf_cpu", 2, "P2"));
  link_buffer_companions(eps);
  return eps;
}
}  // namespace

TEST(BufferCompanion, TheSingleCandidateIsTheParentAndTheCompanionTopicLeavesTheDefaultView)
{
  auto eps = lyrical_large_array();
  EXPECT_EQ(eps[1].buffer_parent_guid, eps[0].guid);
  EXPECT_EQ(eps[0].buffer_companion_guids, std::vector<std::string>{eps[1].guid});
  EXPECT_EQ(eps[3].buffer_parent_guid, eps[2].guid);
  EXPECT_EQ(eps[2].buffer_companion_guids, std::vector<std::string>{eps[3].guid});
  EXPECT_TRUE(eps[0].buffer_parent_guid.empty());

  link_buffer_companions(eps);   // linking again starts over
  EXPECT_EQ(eps[0].buffer_companion_guids.size(), 1u);

  auto topics = summarize(eps);
  const auto & parent = topic_named(topics, "/large_array");
  const auto & companion = topic_named(topics, "/large_array/_buf_cpu");
  ASSERT_EQ(parent.pairs.size(), 1u);
  ASSERT_EQ(companion.pairs.size(), 1u);
  EXPECT_TRUE(has(parent.pairs[0].verdict.reasons, "buffer-companion-folded"));
  EXPECT_FALSE(has(parent.pairs[0].verdict.reasons, "buffer-companion"));
  EXPECT_TRUE(has(companion.pairs[0].verdict.reasons, "buffer-companion"));
  EXPECT_FALSE(has(companion.pairs[0].verdict.reasons, "buffer-companion-folded"));
  EXPECT_EQ(parent.pairs[0].verdict.transport, decide(eps[0], eps[2]).transport);
  EXPECT_TRUE(in_default_view(parent));
  EXPECT_FALSE(in_default_view(companion));
}

TEST(BufferCompanion, SeveralCandidatesAreToldApartByTheEntityKey)
{
  std::vector<Endpoint> eps;
  eps.push_back(entity(true, "/large_array", 1));
  eps.push_back(entity(true, "/large_array", 3));
  eps.push_back(entity(true, "/large_array/_buf_cpu", 4));   // 3 + 1
  eps.push_back(entity(true, "/large_array/_buf_cpu", 9));   // no writer with key 8
  Endpoint other_kind = entity(true, "/large_array/_buf_cpu", 2);
  other_kind.guid_bytes[15] = 0x02;                          // key 1 + 1, another kind
  eps.push_back(other_kind);
  link_buffer_companions(eps);
  EXPECT_EQ(eps[2].buffer_parent_guid, eps[1].guid);
  EXPECT_TRUE(eps[3].buffer_parent_guid.empty());
  EXPECT_TRUE(eps[4].buffer_parent_guid.empty());
  EXPECT_TRUE(eps[0].buffer_companion_guids.empty());
  EXPECT_EQ(eps[1].buffer_companion_guids, std::vector<std::string>{eps[2].guid});

  // no reader: the codes go to the topic, and an unmatched companion keeps it visible
  auto topics = summarize(eps);
  const auto & companion = topic_named(topics, "/large_array/_buf_cpu");
  EXPECT_TRUE(companion.pairs.empty());
  EXPECT_TRUE(has(companion.unmatched_reasons, "no-matching-reader"));
  EXPECT_TRUE(has(companion.unmatched_reasons, "buffer-companion"));
  EXPECT_TRUE(has(companion.unmatched_reasons, "buffer-companion-unmatched"));
  EXPECT_TRUE(in_default_view(companion));
  EXPECT_TRUE(
    has(topic_named(topics, "/large_array").unmatched_reasons, "buffer-companion-folded"));
}

TEST(BufferCompanion, OnlyAWriterOrReaderOfTheParentTopicInTheSameParticipantWithTheSameType)
{
  std::vector<Endpoint> eps;
  eps.push_back(entity(true, "/a", 1));
  eps.push_back(entity(true, "/a/_buf_cpu", 2, "P2"));   // another participant
  eps.push_back(entity(false, "/a/_buf_cpu", 3));        // a reader for a writer
  Endpoint other_type = entity(true, "/a/_buf_cpu", 4);
  other_type.dds_type = "std_msgs::msg::dds_::String_";
  eps.push_back(other_type);
  eps.push_back(entity(true, "/b/_buf/0123456789abcdef0123456789abcdef", 5));   // accelerator
  eps.push_back(entity(true, "/_buf_cpu", 6));             // no parent name at all
  link_buffer_companions(eps);
  for (const auto & e : eps) {
    EXPECT_TRUE(e.buffer_parent_guid.empty()) << e.dds_topic;
    EXPECT_TRUE(e.buffer_companion_guids.empty()) << e.dds_topic;
  }
  auto topics = summarize(eps);
  EXPECT_TRUE(has(all_codes(topic_named(topics, "/a/_buf_cpu")), "buffer-companion-unmatched"));
  EXPECT_FALSE(has(all_codes(topic_named(topics, "/a")), "buffer-companion-folded"));
  const auto & accelerator = topic_named(topics, "/b/_buf/0123456789abcdef0123456789abcdef");
  const auto accelerator_codes = all_codes(accelerator);
  EXPECT_EQ(
    std::count_if(
      accelerator_codes.begin(), accelerator_codes.end(),
      [](const std::string & c) {return c.rfind("buffer-", 0) == 0;}), 0);
  EXPECT_TRUE(in_default_view(accelerator));
  EXPECT_TRUE(has(topic_named(topics, "/_buf_cpu").unmatched_reasons, "no-matching-reader"));
}

TEST(BufferCompanion, DefaultViewStillNeedsARosTopic)
{
  auto eps = lyrical_large_array();
  auto topics = summarize(eps);
  TopicSummary service = topic_named(topics, "/large_array");
  service.dds_topic = "rq/large_arrayRequest";
  EXPECT_FALSE(in_default_view(service));
  TopicSummary raw = topic_named(topics, "/large_array");
  raw.is_ros_topic = false;
  EXPECT_FALSE(in_default_view(raw));
}

TEST(BufferCompanion, FilterByNodeKeepsTheCodes)
{
  std::vector<Endpoint> eps;
  eps.push_back(entity(true, "/large_array", 1));
  eps.push_back(entity(true, "/large_array/_buf_cpu", 2));
  eps.push_back(entity(false, "/large_array", 1, "P2"));
  for (auto & e : eps) {
    e.node_name = e.is_writer ? "/pub" : "/sub";
  }
  link_buffer_companions(eps);
  auto topics = summarize(eps);
  filter_by_node(topics, [](const Endpoint & e) {return e.node_name == "/pub";});
  const auto & parent = topic_named(topics, "/large_array");
  ASSERT_EQ(parent.pairs.size(), 1u);
  EXPECT_TRUE(has(parent.pairs[0].verdict.reasons, "buffer-companion-folded"));
  const auto & companion = topic_named(topics, "/large_array/_buf_cpu");
  EXPECT_TRUE(has(companion.unmatched_reasons, "no-matching-reader"));
  EXPECT_TRUE(has(companion.unmatched_reasons, "buffer-companion"));
}

TEST(ApplyStats, BufferCompanionCountersAreAddedToTheParentPair)
{
  auto eps = lyrical_large_array();
  const std::string W = eps[0].guid, WB = eps[1].guid, R = eps[2].guid, RB = eps[3].guid;
  auto topics = summarize(eps);
  StatsData stats;
  stats.enabled = true;
  stats.participants_with_stats = {"P1", "P2"};
  // Lyrical with buffer-aware subscribers: the samples go on the companions only
  stats.data_count[W] = DataCountSample{0, 0, 2};
  stats.data_count[WB] = DataCountSample{10, 406, 5};
  stats.heartbeats[W] = DataCountSample{5, 5, 2};
  stats.heartbeats[WB] = DataCountSample{0, 242, 5};
  stats.acknacks[R] = DataCountSample{1, 2, 2};
  stats.acknacks[RB] = DataCountSample{0, 10, 5};
  stats.delivered[{WB, RB}] = 3;
  LatencyStat parent_latency;
  parent_latency.add(0.002);
  stats.latency[{W, R}] = parent_latency;
  LatencyStat companion_latency;
  companion_latency.add(0.001);
  companion_latency.add(0.004);
  companion_latency.add(0.003);
  stats.latency[{WB, RB}] = companion_latency;
  apply_stats(topics, stats);

  const auto & parent = topic_named(topics, "/large_array");
  const auto & m = parent.pairs[0].measured;
  EXPECT_TRUE(m.delivered);
  EXPECT_EQ(m.delivered_samples, 3u);
  EXPECT_TRUE(m.data_count_available);
  EXPECT_EQ(m.data_submessages, 396u);
  EXPECT_TRUE(m.reliability.available);
  EXPECT_EQ(m.reliability.heartbeats, 242u);
  EXPECT_EQ(m.reliability.acknacks, 11u);
  EXPECT_TRUE(m.latency_available);
  EXPECT_EQ(m.latency.samples, 4u);
  EXPECT_NEAR(m.latency.sum, 0.010, 1e-12);
  EXPECT_DOUBLE_EQ(m.latency.min, 0.001);
  EXPECT_DOUBLE_EQ(m.latency.max, 0.004);
  EXPECT_DOUBLE_EQ(m.latency.last, 0.003);   // the companions'
  EXPECT_NEAR(parent.latency, 0.0025, 1e-12);

  // the companion topic keeps its own numbers
  const auto & c = topic_named(topics, "/large_array/_buf_cpu").pairs[0].measured;
  EXPECT_EQ(c.delivered_samples, 3u);
  EXPECT_EQ(c.data_submessages, 396u);
  EXPECT_EQ(c.reliability.heartbeats, 242u);
  EXPECT_EQ(c.reliability.acknacks, 10u);
  EXPECT_EQ(c.latency.samples, 3u);
}

TEST(ApplyStats, WithoutCompanionsTheCountersAreThePairsOwn)
{
  // rmw_fastrtps_dynamic_cpp or a subscriber without native buffers: everything on the parent
  std::vector<Endpoint> eps;
  eps.push_back(entity(true, "/large_array", 1));
  eps.push_back(entity(false, "/large_array", 1, "P2"));
  link_buffer_companions(eps);
  auto topics = summarize(eps);
  EXPECT_FALSE(has(topics[0].pairs[0].verdict.reasons, "buffer-companion-folded"));
  StatsData stats;
  stats.enabled = true;
  stats.participants_with_stats = {"P1"};
  stats.data_count[eps[0].guid] = DataCountSample{3, 7, 2};
  stats.delivered[{eps[0].guid, eps[1].guid}] = 4;
  apply_stats(topics, stats);
  EXPECT_EQ(topics[0].pairs[0].measured.data_submessages, 4u);
  EXPECT_EQ(topics[0].pairs[0].measured.delivered_samples, 4u);
  EXPECT_FALSE(topics[0].pairs[0].measured.latency_available);
}

// discovery_completeness() (#133): the endpoints the nodes announce in ros_discovery_info
// against the endpoints discovery actually delivered.

namespace
{

ParticipantPrefix prefix(uint8_t tag)
{
  ParticipantPrefix p{};
  p[0] = 1;
  p[11] = tag;
  return p;
}

EndpointGid gid(uint8_t participant_tag, uint8_t entity)
{
  EndpointGid g{};
  g[0] = 1;
  g[11] = participant_tag;
  g[15] = entity;
  return g;
}

Endpoint discovered(const EndpointGid & g)
{
  Endpoint e;
  e.guid_bytes = g;
  e.guid = "g" + std::to_string(g[11]) + "." + std::to_string(g[15]);
  return e;
}

}  // namespace

TEST(DiscoveryCompleteness, EveryAnnouncedEndpointDiscoveredIsComplete)
{
  const auto p = prefix(1);
  std::map<ParticipantPrefix, std::vector<EndpointGid>> announced{{p, {gid(1, 1), gid(1, 2)}}};
  std::vector<Endpoint> eps{discovered(gid(1, 1)), discovered(gid(1, 2))};
  const auto d = discovery_completeness(announced, {p}, eps);
  ASSERT_TRUE(d.complete.has_value());
  EXPECT_TRUE(*d.complete);
  EXPECT_EQ(d.announced_not_discovered, 0u);
  EXPECT_EQ(d.participants_incomplete, 0u);
  EXPECT_EQ(d.endpoints, 2u);
}

TEST(DiscoveryCompleteness, AnAnnouncedEndpointNeverDiscoveredIsIncomplete)
{
  const auto p1 = prefix(1);
  const auto p2 = prefix(2);
  std::map<ParticipantPrefix, std::vector<EndpointGid>> announced{
    {p1, {gid(1, 1), gid(1, 2)}},
    {p2, {gid(2, 1)}},
  };
  // p1's second endpoint is still on its way: the observation stopped too early
  std::vector<Endpoint> eps{discovered(gid(1, 1)), discovered(gid(2, 1))};
  const auto d = discovery_completeness(announced, {p1, p2}, eps);
  ASSERT_TRUE(d.complete.has_value());
  EXPECT_FALSE(*d.complete);
  EXPECT_EQ(d.announced_not_discovered, 1u);
  EXPECT_EQ(d.participants_incomplete, 1u);
  EXPECT_EQ(d.announced_by_incomplete, 2u);   // only the incomplete participant's
  EXPECT_EQ(d.endpoints, 2u);
}

TEST(DiscoveryCompleteness, AParticipantThatLeftIsNotCompared)
{
  const auto gone = prefix(9);
  // ros_discovery_info never evicts a departed participant, so its row outlives it
  std::map<ParticipantPrefix, std::vector<EndpointGid>> announced{{gone, {gid(9, 1)}}};
  const auto d = discovery_completeness(announced, {}, {});
  EXPECT_FALSE(d.complete.has_value());   // nothing left to compare: no judgement
  EXPECT_EQ(d.announced_not_discovered, 0u);
}

TEST(DiscoveryCompleteness, OnlyTheLivePartOfTheTableCounts)
{
  const auto live = prefix(1);
  const auto gone = prefix(9);
  std::map<ParticipantPrefix, std::vector<EndpointGid>> announced{
    {live, {gid(1, 1)}},
    {gone, {gid(9, 1), gid(9, 2)}},
  };
  std::vector<Endpoint> eps{discovered(gid(1, 1))};
  const auto d = discovery_completeness(announced, {live}, eps);
  ASSERT_TRUE(d.complete.has_value());
  EXPECT_TRUE(*d.complete);   // the departed participant's endpoints are not missing
}

TEST(DiscoveryCompleteness, NoAnnouncementAtAllLeavesItUnset)
{
  // no ros_discovery_info sample (or the reader could not be created): the endpoints
  // discovery delivered say nothing about what was not delivered
  const auto d = discovery_completeness({}, {prefix(1)}, {discovered(gid(1, 1))});
  EXPECT_FALSE(d.complete.has_value());
  EXPECT_EQ(d.endpoints, 1u);
}

TEST(DiscoveryCompleteness, DiscoveredEndpointsOutsideTheTableDoNotCount)
{
  // a raw DDS writer of a non-ROS participant is discovered but announces nothing
  const auto p = prefix(1);
  std::map<ParticipantPrefix, std::vector<EndpointGid>> announced{{p, {gid(1, 1)}}};
  std::vector<Endpoint> eps{discovered(gid(1, 1)), discovered(gid(7, 1))};
  const auto d = discovery_completeness(announced, {p}, eps);
  ASSERT_TRUE(d.complete.has_value());
  EXPECT_TRUE(*d.complete);
  EXPECT_EQ(d.endpoints, 2u);
}

TEST(IncompleteDiscoveryWarning, ACompleteOrUnjudgedViewSaysNothing)
{
  DiscoveryStatus d;
  EXPECT_EQ(incomplete_discovery_warning(d, 1.0, 3.0, false), "");   // complete unset
  d.complete = true;
  EXPECT_EQ(incomplete_discovery_warning(d, 1.0, 3.0, false), "");
}

TEST(IncompleteDiscoveryWarning, NamesTheNumbersAndLongerSettings)
{
  DiscoveryStatus d;
  d.complete = false;
  d.announced_not_discovered = 12;
  d.announced_by_incomplete = 40;
  d.participants_incomplete = 3;
  EXPECT_EQ(
    incomplete_discovery_warning(d, 1.0, 3.0, false),
    "warning: discovery was still in progress (12 of 40 endpoints announced by 3 participants "
    "were not seen); pass --quiet 3 or --timeout 10 for a complete view");
}

TEST(IncompleteDiscoveryWarning, OneParticipantIsSingular)
{
  DiscoveryStatus d;
  d.complete = false;
  d.announced_not_discovered = 1;
  d.announced_by_incomplete = 6;
  d.participants_incomplete = 1;
  EXPECT_NE(
    incomplete_discovery_warning(d, 1.0, 3.0, false).find("by 1 participant were not seen"),
    std::string::npos);
}

TEST(IncompleteDiscoveryWarning, TheAdviceIsAlwaysLongerThanWhatTheRunUsed)
{
  DiscoveryStatus d;
  d.complete = false;
  d.announced_not_discovered = 2;
  d.announced_by_incomplete = 5;
  d.participants_incomplete = 1;
  // already generous settings: double them rather than advise less
  EXPECT_NE(
    incomplete_discovery_warning(d, 4.0, 30.0, false).find("--quiet 8 or --timeout 60"),
    std::string::npos);
  // --quiet 0 (the window is off): only a timeout is worth advising
  const auto no_quiet = incomplete_discovery_warning(d, 0.0, 3.0, false);
  EXPECT_EQ(no_quiet.find("--quiet"), std::string::npos) << no_quiet;
  EXPECT_NE(no_quiet.find("pass --timeout 10"), std::string::npos) << no_quiet;
  // --stats runs the full timeout and ignores --quiet
  const auto with_stats = incomplete_discovery_warning(d, 1.0, 5.0, true);
  EXPECT_EQ(with_stats.find("--quiet"), std::string::npos) << with_stats;
  EXPECT_NE(with_stats.find("pass --timeout 15"), std::string::npos) << with_stats;
}

TEST(StatisticsLossWarning, NothingLostSaysNothing)
{
  StatsData s;
  EXPECT_FALSE(statistics_samples_were_lost(s));
  EXPECT_EQ(statistics_loss_warning(s), "");
  s.samples = 1000;
  s.samples_lost_at_start = 4000;   // the burst from before the readers matched is not the tool
  EXPECT_FALSE(statistics_samples_were_lost(s));
  EXPECT_EQ(statistics_loss_warning(s), "");
}

TEST(StatisticsLossWarning, NamesTheLossAgainstEverythingThatShouldHaveArrived)
{
  StatsData s;
  s.samples = 8529;
  s.samples_lost = 682142;
  s.samples_lost_at_start = 120;   // out of both the numerator and the denominator
  EXPECT_TRUE(statistics_samples_were_lost(s));
  EXPECT_EQ(
    statistics_loss_warning(s),
    "warning: 682142 of 690671 statistics samples were lost (the tool could not keep up); "
    "some pairs show no measurement although they carry traffic - enable statistics on fewer "
    "nodes, or keep FASTDDS_STATISTICS to the aliases you need "
    "(e.g. RTPS_SENT_TOPIC;RTPS_LOST_TOPIC)");
}

TEST(StatisticsLossWarning, BestEffortLatencyLossIsNotTheToolFallingBehind)
{
  // #141: the HISTORY_LATENCY reader is best-effort by design, so it reports every sequence
  // gap in the loudest topic there is. Those samples coarsen a percentile the pair still
  // shows; they cost no pair its measurement, so they must not raise the warning.
  StatsData s;
  s.samples = 8529;
  s.samples_lost = 248312;
  s.samples_lost_latency = 248312;
  EXPECT_EQ(statistics_counter_samples_lost(s), 0u);
  EXPECT_FALSE(statistics_samples_were_lost(s));
  EXPECT_EQ(statistics_loss_warning(s), "");

  // only the counter part is named, and it alone decides the warning
  s.samples_lost_latency = 248300;
  EXPECT_EQ(statistics_counter_samples_lost(s), 12u);
  EXPECT_TRUE(statistics_samples_were_lost(s));
  EXPECT_NE(
    statistics_loss_warning(s).find("12 of 8541 statistics samples"),
    std::string::npos) << statistics_loss_warning(s);

  // a document written before #141 carries no latency part: everything counts, as it did
  s.samples_lost_latency = 0;
  EXPECT_EQ(statistics_counter_samples_lost(s), 248312u);

  // hand-edited or truncated: the subtraction saturates instead of wrapping around
  s.samples_lost_latency = 999999;
  EXPECT_EQ(statistics_counter_samples_lost(s), 0u);
  EXPECT_FALSE(statistics_samples_were_lost(s));
}

TEST(StatisticsLossWarning, ASingleRejectedSampleIsEnough)
{
  StatsData s;
  s.samples = 10;
  s.samples_rejected = 1;
  EXPECT_TRUE(statistics_samples_were_lost(s));
  // rejected samples share the count and the code: one number for what is missing
  EXPECT_NE(statistics_loss_warning(s).find("1 of 11 statistics samples"), std::string::npos);
  // a longer run collects more of the loss rather than less, so the stderr line never
  // offers --timeout, and the remedy says outright that it does not help
  EXPECT_EQ(statistics_loss_warning(s).find("--timeout"), std::string::npos);
  EXPECT_NE(statistics_loss_warning(s).find("FASTDDS_STATISTICS"), std::string::npos);
  EXPECT_NE(remedy("stats-samples-lost")->find("--timeout does not help"), std::string::npos);
}

TEST(StatisticsLateJoinWindow, FollowsEveryWriterMatch)
{
  EXPECT_TRUE(statistics_late_join_window_open(true, 0.0));
  EXPECT_TRUE(statistics_late_join_window_open(true, kStatisticsLateJoinGraceSeconds / 2));
  // five seconds after the last match the burst is over: a node that joins mid-run excuses
  // that much loss, never the losses that keep coming after it
  EXPECT_FALSE(statistics_late_join_window_open(true, kStatisticsLateJoinGraceSeconds));
  EXPECT_FALSE(statistics_late_join_window_open(true, 60.0));

  // The window has to be this wide because a writer announces what its history dropped with
  // one of its next heartbeats: up to 3.9 s after the match on Lyrical. Before the first
  // match of all there is no match for the grace period to run from.
  EXPECT_TRUE(statistics_late_join_window_open(false, 0.0));
  EXPECT_TRUE(statistics_late_join_window_open(false, 60.0));
}
