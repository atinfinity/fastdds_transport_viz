// Throwaway probe for issue #193: what type-identity information reaches the wire in
// DATA(w)/DATA(r) on Fast DDS 2.x (Humble 2.6, Jazzy 2.14) and 3.x (Lyrical 3.6)?
// Not part of the colcon package; build with g++ inside the distro image.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if __has_include(<fastdds/config.hpp>)
#include <fastdds/config.hpp>
#define P_FASTDDS_3 (FASTDDS_VERSION_MAJOR >= 3)
#else
#include <fastrtps/config.h>
#define FASTDDS_VERSION_MAJOR FASTRTPS_VERSION_MAJOR
#define P_FASTDDS_3 0
#endif

#if P_FASTDDS_3
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/rtps/builtin/data/PublicationBuiltinTopicData.hpp>
#include <fastdds/rtps/builtin/data/SubscriptionBuiltinTopicData.hpp>
#include <fastdds/rtps/reader/ReaderDiscoveryStatus.hpp>
#include <fastdds/rtps/writer/WriterDiscoveryStatus.hpp>
#else
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/rtps/builtin/data/WriterProxyData.h>
#include <fastdds/rtps/builtin/data/ReaderProxyData.h>
#include <fastdds/rtps/reader/ReaderDiscoveryInfo.h>
#include <fastdds/rtps/writer/WriterDiscoveryInfo.h>
#include <fastrtps/types/TypeObject.h>
#include <fastrtps/types/TypeIdentifier.h>
#endif

namespace edds = eprosima::fastdds::dds;

static std::mutex g_mutex;
static std::set<std::string> g_seen;

static std::string hexbytes(const uint8_t * p, size_t n)
{
  std::string out;
  char buf[4];
  for (size_t i = 0; i < n; ++i) {
    std::snprintf(buf, sizeof(buf), "%02x", p[i]);
    out += buf;
  }
  return out;
}

template<typename GUID>
static std::string guid_str(const GUID & g)
{
  std::string out;
  char buf[8];
  for (int i = 0; i < 12; ++i) {
    std::snprintf(buf, sizeof(buf), "%02x", g.guidPrefix.value[i]);
    out += buf;
  }
  out += "|";
  for (int i = 0; i < 4; ++i) {
    std::snprintf(buf, sizeof(buf), "%02x", g.entityId.value[i]);
    out += buf;
  }
  return out;
}

static std::string userdata_str(const std::vector<uint8_t> & v)
{
  std::string out;
  for (uint8_t c : v) {
    out += (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '.';
  }
  return out;
}

#if !P_FASTDDS_3
using eprosima::fastrtps::types::TypeIdentifier;

static std::string tid_str(const TypeIdentifier & tid)
{
  std::ostringstream os;
  const uint8_t d = static_cast<uint8_t>(tid._d());
  os << "kind=0x";
  char buf[4];
  std::snprintf(buf, sizeof(buf), "%02x", d);
  os << buf;
  if (d == eprosima::fastrtps::types::EK_MINIMAL ||
    d == eprosima::fastrtps::types::EK_COMPLETE ||
    d == eprosima::fastrtps::types::TI_STRONGLY_CONNECTED_COMPONENT)
  {
    os << " hash=" << hexbytes(
      reinterpret_cast<const uint8_t *>(tid.equivalence_hash()), 14);
  }
  return os.str();
}

template<typename PROXY>
static void report(const char * what, const PROXY & d)
{
  std::ostringstream os;
  os << what << " topic=" << d.topicName().to_string()
     << " type=" << d.typeName().to_string()
     << " guid=" << guid_str(d.guid()) << "\n";
  os << "    has_type_id=" << (d.has_type_id() ? "yes" : "no")
     << " has_type=" << (d.has_type() ? "yes" : "no")
     << " has_type_information=" << (d.has_type_information() ? "yes" : "no") << "\n";
  if (d.has_type_id()) {
    os << "    type_id: " << tid_str(d.type_id().m_type_identifier) << "\n";
  }
  if (d.has_type()) {
    os << "    type_object: present (TypeObjectV1)\n";
  }
  if (d.has_type_information()) {
    const auto & ti = d.type_information();
    os << "    type_information: assigned=" << (ti.assigned() ? "yes" : "no") << "\n";
    os << "      minimal: "
       << tid_str(ti.type_information.minimal().typeid_with_size().type_id())
       << " size=" << ti.type_information.minimal().typeid_with_size().typeobject_serialized_size()
       << " deps=" << ti.type_information.minimal().dependent_typeid_count() << "\n";
    os << "      complete: "
       << tid_str(ti.type_information.complete().typeid_with_size().type_id())
       << " size=" << ti.type_information.complete().typeid_with_size().typeobject_serialized_size()
       << " deps=" << ti.type_information.complete().dependent_typeid_count() << "\n";
  }
  os << "    user_data=\"" << userdata_str(d.m_qos.m_userData.data_vec()) << "\"";
  std::lock_guard<std::mutex> lock(g_mutex);
  const std::string line = os.str();
  if (g_seen.insert(line).second) {
    std::cout << line << std::endl;
  }
}

class Probe : public edds::DomainParticipantListener
{
public:
  void on_publisher_discovery(
    edds::DomainParticipant *, eprosima::fastrtps::rtps::WriterDiscoveryInfo && info) override
  {
    if (info.status == eprosima::fastrtps::rtps::WriterDiscoveryInfo::DISCOVERED_WRITER ||
      info.status == eprosima::fastrtps::rtps::WriterDiscoveryInfo::CHANGED_QOS_WRITER)
    {
      report("WRITER", info.info);
    }
  }
  void on_subscriber_discovery(
    edds::DomainParticipant *, eprosima::fastrtps::rtps::ReaderDiscoveryInfo && info) override
  {
    if (info.status == eprosima::fastrtps::rtps::ReaderDiscoveryInfo::DISCOVERED_READER ||
      info.status == eprosima::fastrtps::rtps::ReaderDiscoveryInfo::CHANGED_QOS_READER)
    {
      report("READER", info.info);
    }
  }
};

#else   // Fast DDS 3.x

namespace xt = eprosima::fastdds::dds::xtypes;

static std::string tid_str(const xt::TypeIdentifier & tid)
{
  std::ostringstream os;
  const uint8_t d = tid._d();
  char buf[4];
  std::snprintf(buf, sizeof(buf), "%02x", d);
  os << "kind=0x" << buf;
  if (d == xt::EK_MINIMAL || d == xt::EK_COMPLETE) {
    const auto & h = tid.equivalence_hash();
    os << " hash=" << hexbytes(h.data(), h.size());
  }
  return os.str();
}

template<typename D>
static void report(const char * what, const D & d)
{
  std::ostringstream os;
  os << what << " topic=" << d.topic_name.to_string()
     << " type=" << d.type_name.to_string()
     << " guid=" << guid_str(d.guid) << "\n";
  const auto & tip = d.type_information;
  os << "    type_information.assigned=" << (tip.assigned() ? "yes" : "no") << "\n";
  os << "      minimal: "
     << tid_str(tip.type_information.minimal().typeid_with_size().type_id())
     << " size=" << tip.type_information.minimal().typeid_with_size().typeobject_serialized_size()
     << " deps=" << tip.type_information.minimal().dependent_typeid_count() << "\n";
  os << "      complete: "
     << tid_str(tip.type_information.complete().typeid_with_size().type_id())
     << " size=" << tip.type_information.complete().typeid_with_size().typeobject_serialized_size()
     << " deps=" << tip.type_information.complete().dependent_typeid_count() << "\n";
  os << "    user_data=\"" << userdata_str(d.user_data.data_vec()) << "\"";
  std::lock_guard<std::mutex> lock(g_mutex);
  const std::string line = os.str();
  if (g_seen.insert(line).second) {
    std::cout << line << std::endl;
  }
}

class Probe : public edds::DomainParticipantListener
{
public:
  void on_data_writer_discovery(
    edds::DomainParticipant *, eprosima::fastdds::rtps::WriterDiscoveryStatus reason,
    const eprosima::fastdds::rtps::PublicationBuiltinTopicData & info, bool & ignored) override
  {
    ignored = false;
    if (reason == eprosima::fastdds::rtps::WriterDiscoveryStatus::DISCOVERED_WRITER ||
      reason == eprosima::fastdds::rtps::WriterDiscoveryStatus::CHANGED_QOS_WRITER)
    {
      report("WRITER", info);
    }
  }
  void on_data_reader_discovery(
    edds::DomainParticipant *, eprosima::fastdds::rtps::ReaderDiscoveryStatus reason,
    const eprosima::fastdds::rtps::SubscriptionBuiltinTopicData & info, bool & ignored) override
  {
    ignored = false;
    if (reason == eprosima::fastdds::rtps::ReaderDiscoveryStatus::DISCOVERED_READER ||
      reason == eprosima::fastdds::rtps::ReaderDiscoveryStatus::CHANGED_QOS_READER)
    {
      report("READER", info);
    }
  }
};
#endif

int main(int argc, char ** argv)
{
  const int seconds = argc > 1 ? std::atoi(argv[1]) : 10;
  int domain = 0;
  if (const char * d = std::getenv("ROS_DOMAIN_ID")) {
    domain = std::atoi(d);
  }
  std::cout << "# Fast DDS major " << FASTDDS_VERSION_MAJOR
            << " domain " << domain << " for " << seconds << "s" << std::endl;
  Probe probe;
  edds::DomainParticipantQos qos = edds::PARTICIPANT_QOS_DEFAULT;
  auto * p = edds::DomainParticipantFactory::get_instance()->create_participant(
    domain, qos, &probe);
  if (p == nullptr) {
    std::cerr << "failed to create participant" << std::endl;
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  edds::DomainParticipantFactory::get_instance()->delete_participant(p);
  return 0;
}
