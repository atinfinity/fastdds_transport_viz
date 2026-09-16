// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// The Fast DDS-facing helpers and the observers' error paths with real participants
// (domain 200 so that the launch tests on domain 0 are not disturbed).

#include <unistd.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "fastdds_transport_viz/fastdds_compat.hpp"

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#if FTV_FASTDDS_3
#include "typesPubSubTypes.hpp"   // vendored statistics types (Fast DDS 3.x)
#else
#include "typesPubSubTypes.h"     // vendored statistics types (Fast DDS 2.14)
#endif

#include "fastdds_transport_viz/discovery_observer.hpp"
#include "fastdds_transport_viz/fastdds_util.hpp"
#include "fastdds_transport_viz/ros_discovery_info_observer.hpp"
#include "fastdds_transport_viz/stats_observer.hpp"

#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <rmw_dds_common/msg/participant_entities_info.hpp>

using namespace fastdds_transport_viz;  // NOLINT
namespace fdds = eprosima::fastdds::dds;

TEST(FastDdsUtil, ConvertLocatorKinds)
{
  ftv_rtps::Locator_t l;
  l.kind = LOCATOR_KIND_UDPv4;
  ftv_rtps::IPLocator::setIPv4(l, "10.0.0.1");
  l.port = 7411;
  auto c = convert_locator(l);
  EXPECT_EQ(c.kind, LocatorKind::UDPv4);
  EXPECT_EQ(c.address, "10.0.0.1");
  EXPECT_EQ(c.port, 7411u);

  l.kind = LOCATOR_KIND_UDPv6;
  ftv_rtps::IPLocator::setIPv6(l, "fd00::1");
  EXPECT_EQ(convert_locator(l).kind, LocatorKind::UDPv6);
  EXPECT_EQ(convert_locator(l).address, "fd00::1");

  l.kind = LOCATOR_KIND_TCPv4;
  ftv_rtps::IPLocator::setIPv4(l, "10.0.0.2");
  ftv_rtps::IPLocator::setPhysicalPort(l, 7500);
  EXPECT_EQ(convert_locator(l).kind, LocatorKind::TCPv4);
  EXPECT_EQ(convert_locator(l).port, 7500u);

  l.kind = LOCATOR_KIND_TCPv6;
  ftv_rtps::IPLocator::setIPv6(l, "fd00::2");
  EXPECT_EQ(convert_locator(l).kind, LocatorKind::TCPv6);
  EXPECT_EQ(convert_locator(l).address, "fd00::2");

  l.kind = LOCATOR_KIND_SHM;
  l.port = 7413;
  EXPECT_EQ(convert_locator(l).kind, LocatorKind::SHM);
  EXPECT_EQ(convert_locator(l).port, 7413u);

  l.kind = 99;
  EXPECT_EQ(convert_locator(l).kind, LocatorKind::Invalid);
}

TEST(FastDdsUtil, NonShmUnicastLocators)
{
  ftv_rtps::Locator_t shm;
  shm.kind = LOCATOR_KIND_SHM;
  shm.port = 7411;
  ftv_rtps::Locator_t udp;
  udp.kind = LOCATOR_KIND_UDPv4;
  ftv_rtps::IPLocator::setIPv4(udp, "10.0.0.1");
  udp.port = 7413;
  ftv_rtps::Locator_t multicast = udp;
  ftv_rtps::IPLocator::setIPv4(multicast, "239.255.0.1");
  ftv_rtps::Locator_t tcp;
  tcp.kind = LOCATOR_KIND_TCPv4;
  ftv_rtps::IPLocator::setIPv4(tcp, "10.0.0.2");
  ftv_rtps::IPLocator::setPhysicalPort(tcp, 7500);

  eprosima::fastdds::rtps::LocatorList listening;
  for (const auto & l : {shm, udp, multicast, tcp}) {
    listening.push_back(l);
  }
  const auto kept = non_shm_unicast_locators(listening);
  const std::vector<ftv_rtps::Locator_t> out(kept.begin(), kept.end());
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0], udp);
  EXPECT_EQ(out[1], tcp);

  eprosima::fastdds::rtps::LocatorList only_shm;
  only_shm.push_back(shm);
  EXPECT_TRUE(non_shm_unicast_locators(only_shm).empty());
}

TEST(FastDdsUtil, QosMapping)
{
  fdds::ReliabilityQosPolicy rel;
  rel.kind = fdds::RELIABLE_RELIABILITY_QOS;
  EXPECT_EQ(reliability_to_string(rel), "RELIABLE");
  rel.kind = fdds::BEST_EFFORT_RELIABILITY_QOS;
  EXPECT_EQ(reliability_to_string(rel), "BEST_EFFORT");

  fdds::DurabilityQosPolicy dur;
  for (auto [kind, text] : {
    std::pair{fdds::VOLATILE_DURABILITY_QOS, "VOLATILE"},
    std::pair{fdds::TRANSIENT_LOCAL_DURABILITY_QOS, "TRANSIENT_LOCAL"},
    std::pair{fdds::TRANSIENT_DURABILITY_QOS, "TRANSIENT"},
    std::pair{fdds::PERSISTENT_DURABILITY_QOS, "PERSISTENT"}})
  {
    dur.kind = kind;
    EXPECT_EQ(durability_to_string(dur), text);
  }
  dur.kind = static_cast<fdds::DurabilityQosPolicyKind>(42);
  EXPECT_EQ(durability_to_string(dur), "UNKNOWN");

  fdds::DataSharingQosPolicy ds;
  EndpointQos out;
  ds.off();
  fill_data_sharing(ds, out);
  EXPECT_EQ(out.data_sharing, DataSharingKind::Off);
  ds.on("", {7, 8});
  fill_data_sharing(ds, out);
  EXPECT_EQ(out.data_sharing, DataSharingKind::On);
  EXPECT_EQ(out.data_sharing_domains, (std::vector<uint64_t>{7, 8}));
  ds.automatic();
  fill_data_sharing(ds, out);
  EXPECT_EQ(out.data_sharing, DataSharingKind::Auto);
}

TEST(FastDdsUtil, GuidStrings)
{
  ftv_rtps::GUID_t g;
  for (int i = 0; i < 12; ++i) {
    g.guidPrefix.value[i] = static_cast<uint8_t>(i);
  }
  g.entityId.value[0] = 0;
  g.entityId.value[1] = 0;
  g.entityId.value[2] = 0x14;
  g.entityId.value[3] = 0x03;
  EXPECT_EQ(guid_to_string(g), "00.01.02.03.04.05.06.07.08.09.0a.0b|00.00.14.03");
  EXPECT_EQ(prefix_to_string(g.guidPrefix), "00.01.02.03.04.05.06.07.08.09.0a.0b");
}

TEST(DiscoveryObserver, ParticipantCreationFailureThrows)
{
  // A default participant profile whose lease duration is shorter than its announcement
  // period: RTPSDomain rejects the attributes, create_participant() returns nullptr. The
  // profile is read by the DomainParticipantFactory once per process, so this runs in a
  // child process.
  ::testing::FLAGS_gtest_death_test_style = "fast";   // no participant exists yet: fork is safe
  EXPECT_EXIT(
  {
    char path[] = "/tmp/ftv_broken_XXXXXX.xml";
    int fd = ::mkstemps(path, 4);
    const char * xml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
    "<dds xmlns=\"http://www.eprosima.com/XMLSchemas/fastRTPS_Profiles\"><profiles>\n"
    "<participant profile_name=\"broken\" is_default_profile=\"true\"><rtps><builtin>\n"
    "<discovery_config><leaseDuration><sec>1</sec></leaseDuration>\n"
    "<leaseAnnouncement><sec>5</sec></leaseAnnouncement></discovery_config>\n"
    "</builtin></rtps></participant>\n"
    "</profiles></dds>\n";
    (void)!::write(fd, xml, std::strlen(xml));
    ::close(fd);
    ::setenv("FASTRTPS_DEFAULT_PROFILES_FILE", path, 1);
    auto * factory = fdds::DomainParticipantFactory::get_instance();
    factory->load_XML_profiles_file(path);
    fdds::DomainParticipantQos broken;
    factory->get_participant_qos_from_profile("broken", broken);
    factory->set_default_participant_qos(broken);     // what the observer uses
    const auto lease =
    factory->get_default_participant_qos().wire_protocol().builtin.discovery_config
    .leaseDuration;
    std::fprintf(stderr, "child: default lease %d s\n", static_cast<int>(lease.seconds));
    try {
      DiscoveryObserver obs(202);
    } catch (const std::runtime_error & e) {
      std::fprintf(stderr, "caught: %s\n", e.what());
      ::unlink(path);
      std::exit(7);
    }
    ::unlink(path);
    std::exit(0);
  }, ::testing::ExitedWithCode(7), "failed to create Fast DDS DomainParticipant");
}

TEST(DiscoveryObserver, ValidDomainStartsEmpty)
{
  DiscoveryObserver obs(200);
  ASSERT_NE(obs.participant(), nullptr);
  EXPECT_EQ(obs.event_count(), 0u);
  EXPECT_TRUE(obs.snapshot().empty());
  const auto id = obs.local_host_id();
  EXPECT_EQ(id[0], obs.participant()->guid().guidPrefix.value[0]);
  EXPECT_EQ(id[3], obs.participant()->guid().guidPrefix.value[3]);
  EXPECT_LE(obs.last_event(), std::chrono::steady_clock::now());
}

TEST(StatsObserver, ReusesAnExistingTopicAndRejectsANonTopicDescription)
{
  DiscoveryObserver obs(201);
  auto * participant = obs.participant();
  ASSERT_NE(participant, nullptr);

  // A topic of the right type already exists (what Fast DDS does with FASTDDS_STATISTICS
  // set in the tool's environment): the observer must reuse it.
  fdds::TypeSupport traffic_type(
    new eprosima::fastdds::statistics::Entity2LocatorTrafficPubSubType());
  traffic_type.register_type(participant);
  auto * existing = participant->create_topic(
    "_fastdds_statistics_rtps_sent", traffic_type.get_type_name(), fdds::TOPIC_QOS_DEFAULT);
  ASSERT_NE(existing, nullptr);
  {
    StatsObserver stats(participant);
    EXPECT_EQ(stats.snapshot().samples, 0u);
    stats.poll();
  }
  // still usable after the observer released its readers
  EXPECT_NE(participant->lookup_topicdescription("_fastdds_statistics_rtps_sent"), nullptr);

  // A description with a statistics topic name that is not a Topic: creation fails.
  auto * base = participant->create_topic(
    "some_base", traffic_type.get_type_name(), fdds::TOPIC_QOS_DEFAULT);
  ASSERT_NE(base, nullptr);
  auto * filtered = participant->create_contentfilteredtopic(
    "_fastdds_statistics_history2history_latency", base, "", {});
  ASSERT_NE(filtered, nullptr);
  EXPECT_THROW(StatsObserver{participant}, std::runtime_error);
}

TEST(StatsObserver, ReadersAnnounceNoShmLocator)
{
  // a same-host writer in another IPC namespace would pick SHM and lose its statistics (#106)
  DiscoveryObserver obs(202);
  StatsObserver stats(obs.participant());
  std::vector<fdds::DataReader *> readers;
  ASSERT_NE(stats.subscriber(), nullptr);
  ASSERT_TRUE(retcode_ok(stats.subscriber()->get_datareaders(readers)));
  EXPECT_EQ(readers.size(), 10u);   // the probe reader is gone, and PUBLICATION_THROUGHPUT (#137)
  for (auto * r : readers) {
    eprosima::fastdds::rtps::LocatorList locators;
    ASSERT_TRUE(retcode_ok(r->get_listening_locators(locators)));
    EXPECT_FALSE(locators.empty());
    for (const auto & l : locators) {
      EXPECT_NE(l.kind, LOCATOR_KIND_SHM) << r->get_topicdescription()->get_name();
    }
  }
}

TEST(RosDiscoveryInfoObserver, ReaderAnnouncesNoShmLocator)
{
  // a same-host node in another IPC namespace would write its samples into its own /dev/shm
  DiscoveryObserver obs(203);
  RosDiscoveryInfoObserver names(obs.participant());
  ASSERT_NE(names.reader(), nullptr);
#if FTV_SAME_HOST_LOCATORS_FILTERED
  // Fast DDS 2.6 would give the raw participant only a same-host node's SHM locator
  EXPECT_NE(names.participant(), obs.participant());
  EXPECT_EQ(names.participant()->get_domain_id(), 203u);
#else
  EXPECT_EQ(names.participant(), obs.participant());
#endif
  eprosima::fastdds::rtps::LocatorList locators;
  ASSERT_TRUE(retcode_ok(names.reader()->get_listening_locators(locators)));
  EXPECT_FALSE(locators.empty());
  for (const auto & l : locators) {
    EXPECT_NE(l.kind, LOCATOR_KIND_SHM);
  }
  const auto qos = names.reader()->get_qos();
  EXPECT_EQ(qos.reliability().kind, fdds::RELIABLE_RELIABILITY_QOS);
  EXPECT_EQ(qos.durability().kind, fdds::TRANSIENT_LOCAL_DURABILITY_QOS);
  EXPECT_EQ(qos.history().kind, fdds::KEEP_ALL_HISTORY_QOS);
}

TEST(FastDdsUtil, SamplePublisherPrefix)
{
  fdds::SampleInfo info;
  ftv_rtps::GUID_t writer;
  const unsigned char prefix[12] = {1, 0x0f, 0xba, 0, 0x1a, 0, 0x33, 0x0e, 1, 0, 0, 0};
  std::memcpy(writer.guidPrefix.value, prefix, sizeof(prefix));
  writer.entityId.value[3] = 0xc2;
  info.sample_identity.writer_guid(writer);
  EXPECT_EQ(sample_publisher_prefix(info), "01.0f.ba.00.1a.00.33.0e.01.00.00.00");
}

TEST(StatsObserver, SourcesArePublishersNotParticipantsNamedInASample)
{
#if !FTV_HAS_STATISTICS
  GTEST_SKIP() << "Fast DDS built without the statistics module";
#else
  DiscoveryObserver obs(205);
  StatsObserver stats(obs.participant());

  // a writer on a participant without statistics, its reader on one with HISTORY_LATENCY:
  // the reader's reports name the writer, which must not count as a statistics source
  auto * factory = fdds::DomainParticipantFactory::get_instance();
  auto * plain = factory->create_participant(205, fdds::PARTICIPANT_QOS_DEFAULT);
  fdds::DomainParticipantQos stats_qos = fdds::PARTICIPANT_QOS_DEFAULT;
  stats_qos.properties().properties().emplace_back(
    "fastdds.statistics", "HISTORY_LATENCY_TOPIC;RTPS_LOST_TOPIC");
  auto * reporting = factory->create_participant(205, stats_qos);
  ASSERT_NE(plain, nullptr);
  ASSERT_NE(reporting, nullptr);

  const std::string topic_name = "ftv_test_statistics_sources";
  fdds::DataWriterQos wqos = fdds::DATAWRITER_QOS_DEFAULT;
  wqos.reliability().kind = fdds::RELIABLE_RELIABILITY_QOS;
  fdds::DataReaderQos rqos = fdds::DATAREADER_QOS_DEFAULT;
  rqos.reliability().kind = fdds::RELIABLE_RELIABILITY_QOS;
  auto type_w = RosDiscoveryInfoObserver::make_type_support();
  type_w.register_type(plain);
  auto * topic_w = plain->create_topic(
    topic_name, RosDiscoveryInfoObserver::kTypeName, fdds::TOPIC_QOS_DEFAULT);
  auto type_r = RosDiscoveryInfoObserver::make_type_support();
  type_r.register_type(reporting);
  auto * topic_r = reporting->create_topic(
    topic_name, RosDiscoveryInfoObserver::kTypeName, fdds::TOPIC_QOS_DEFAULT);
  ASSERT_NE(topic_w, nullptr);
  ASSERT_NE(topic_r, nullptr);
  auto * writer =
    plain->create_publisher(fdds::PUBLISHER_QOS_DEFAULT)->create_datawriter(topic_w, wqos);
  auto * reader =
    reporting->create_subscriber(fdds::SUBSCRIBER_QOS_DEFAULT)->create_datareader(topic_r, rqos);
  ASSERT_NE(writer, nullptr);
  ASSERT_NE(reader, nullptr);

  const auto pair = std::make_pair(
    guid_to_string(writer->guid()), guid_to_string(reader->guid()));
  rmw_dds_common::msg::ParticipantEntitiesInfo msg;
  msg.gid.data.fill(0);
  StatsData data;
  for (int i = 0; i < 100 && !data.delivered.count(pair); ++i) {   // up to 10 s
    static_cast<void>(writer->write(&msg, fdds::HANDLE_NIL));
    usleep(100 * 1000);
    data = stats.snapshot();
  }
  ASSERT_TRUE(data.delivered.count(pair)) << "no HISTORY_LATENCY sample for the pair";
  const auto & sources = data.participants_with_stats;
  EXPECT_TRUE(sources.count(prefix_to_string(reporting->guid().guidPrefix)));
  EXPECT_FALSE(sources.count(prefix_to_string(plain->guid().guidPrefix)));
  EXPECT_FALSE(sources.count(prefix_to_string(obs.participant()->guid().guidPrefix)));

  ASSERT_TRUE(retcode_ok(plain->delete_contained_entities()));
  ASSERT_TRUE(retcode_ok(reporting->delete_contained_entities()));
  ASSERT_TRUE(retcode_ok(factory->delete_participant(plain)));
  ASSERT_TRUE(retcode_ok(factory->delete_participant(reporting)));
#endif
}

TEST(RosDiscoveryInfoObserver, ReadsWhatAnRmwWrites)
{
  DiscoveryObserver obs(204);
  RosDiscoveryInfoObserver names(obs.participant());

  // a stand-in for another process's rmw: same topic, type and QoS as rmw_dds_common's writer
  auto * factory = fdds::DomainParticipantFactory::get_instance();
  auto * node = factory->create_participant(204, fdds::PARTICIPANT_QOS_DEFAULT);
  ASSERT_NE(node, nullptr);
  auto type = RosDiscoveryInfoObserver::make_type_support();
  type.register_type(node);
  auto * topic = node->create_topic(
    RosDiscoveryInfoObserver::kTopicName, RosDiscoveryInfoObserver::kTypeName,
    fdds::TOPIC_QOS_DEFAULT);
  ASSERT_NE(topic, nullptr);
  auto * publisher = node->create_publisher(fdds::PUBLISHER_QOS_DEFAULT);
  fdds::DataWriterQos wqos = fdds::DATAWRITER_QOS_DEFAULT;
  wqos.reliability().kind = fdds::RELIABLE_RELIABILITY_QOS;
  wqos.durability().kind = fdds::TRANSIENT_LOCAL_DURABILITY_QOS;
  wqos.history().kind = fdds::KEEP_LAST_HISTORY_QOS;
  wqos.history().depth = 1;
  auto * writer = publisher->create_datawriter(topic, wqos);
  ASSERT_NE(writer, nullptr);

  rmw_dds_common::msg::ParticipantEntitiesInfo msg;
  msg.gid.data.fill(0);
  msg.gid.data[0] = 0x42;
  rmw_dds_common::msg::NodeEntitiesInfo talker;
  talker.node_namespace = "/robot";
  talker.node_name = "talker";
  auto gid = msg.gid;
  gid.data[15] = 0x03;
  talker.writer_gid_seq.push_back(gid);
  msg.node_entities_info_seq.push_back(talker);
  ASSERT_TRUE(retcode_ok(writer->write(&msg, fdds::HANDLE_NIL)));   // 2.x: write(data) is bool

  EndpointGid wanted{};
  wanted[0] = 0x42;
  wanted[15] = 0x03;
  std::string name;
  for (int i = 0; i < 100 && name.empty(); ++i) {   // up to 10 s for discovery and delivery
    usleep(100 * 1000);
    names.poll();
    name = names.node_for_guid(wanted);
  }
  EXPECT_EQ(name, "/robot/talker");
  EXPECT_EQ(names.table().size(), 1u);

  ASSERT_TRUE(retcode_ok(node->delete_contained_entities()));
  ASSERT_TRUE(retcode_ok(factory->delete_participant(node)));
}

TEST(FastDdsUtil, LivelinessOwnershipAndDurations)
{
  fdds::LivelinessQosPolicy liv;
  liv.kind = fdds::AUTOMATIC_LIVELINESS_QOS;
  EXPECT_EQ(liveliness_to_string(liv), "AUTOMATIC");
  liv.kind = fdds::MANUAL_BY_PARTICIPANT_LIVELINESS_QOS;
  EXPECT_EQ(liveliness_to_string(liv), "MANUAL_BY_PARTICIPANT");
  liv.kind = fdds::MANUAL_BY_TOPIC_LIVELINESS_QOS;
  EXPECT_EQ(liveliness_to_string(liv), "MANUAL_BY_TOPIC");
  liv.kind = static_cast<fdds::LivelinessQosPolicyKind>(9);
  EXPECT_EQ(liveliness_to_string(liv), "UNKNOWN");

  fdds::OwnershipQosPolicy own;
  own.kind = fdds::SHARED_OWNERSHIP_QOS;
  EXPECT_EQ(ownership_to_string(own), "SHARED");
  own.kind = fdds::EXCLUSIVE_OWNERSHIP_QOS;
  EXPECT_EQ(ownership_to_string(own), "EXCLUSIVE");

  EXPECT_TRUE(std::isinf(duration_seconds(liv.lease_duration)));   // default: infinite
  liv.lease_duration.seconds = 2;
  liv.lease_duration.nanosec = 500000000u;
  EXPECT_DOUBLE_EQ(duration_seconds(liv.lease_duration), 2.5);
}
