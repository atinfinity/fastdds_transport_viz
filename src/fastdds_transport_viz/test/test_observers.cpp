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
#include "fastdds_transport_viz/stats_observer.hpp"

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
