// Throwaway probe for issue #210 (Fast DDS 3.x only): prints, per discovered participant
// and endpoint, the fields that could tell "unchecked and different" from "unchecked and fine".
// Build (lyrical image):
//   g++ -std=c++17 -O1 probe.cpp -o probe -I/opt/ros/lyrical/include/fastdds \
//     -I/opt/ros/lyrical/includefastcdr -L/opt/ros/lyrical/lib \
//     -L/opt/ros/lyrical/lib/aarch64-linux-gnu -lfastdds -lfastcdr -pthread
// usage: probe <seconds>   (ROS_DOMAIN_ID)
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>
#include <fastdds/rtps/builtin/data/ParticipantBuiltinTopicData.hpp>
#include <fastdds/rtps/builtin/data/PublicationBuiltinTopicData.hpp>
#include <fastdds/rtps/builtin/data/SubscriptionBuiltinTopicData.hpp>
#include <fastdds/rtps/participant/ParticipantDiscoveryInfo.hpp>
#include <fastdds/rtps/reader/ReaderDiscoveryStatus.hpp>
#include <fastdds/rtps/writer/WriterDiscoveryStatus.hpp>

namespace edds = eprosima::fastdds::dds;
namespace rtps = eprosima::fastdds::rtps;
namespace xt = eprosima::fastdds::dds::xtypes;

static std::mutex g_mutex;
static std::set<std::string> g_seen;

static std::string hexbytes(const uint8_t * p, size_t n)
{
  std::string out; char buf[4];
  for (size_t i = 0; i < n; ++i) { std::snprintf(buf, sizeof(buf), "%02x", p[i]); out += buf; }
  return out;
}
template<typename GUID>
static std::string guid_str(const GUID & g)
{
  return hexbytes(g.guidPrefix.value, 12) + "|" + hexbytes(g.entityId.value, 4);
}
static std::string ud(const std::vector<uint8_t> & v)
{
  std::string out;
  for (uint8_t c : v) { out += (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '.'; }
  return out;
}
static std::string tid_str(const xt::TypeIdentifier & tid)
{
  std::ostringstream os; char buf[4];
  std::snprintf(buf, sizeof(buf), "%02x", tid._d());
  os << "_d=0x" << buf;
  if (tid._d() == xt::EK_MINIMAL || tid._d() == xt::EK_COMPLETE) {
    os << " hash=" << hexbytes(tid.equivalence_hash().data(), tid.equivalence_hash().size());
  }
  return os.str();
}
static std::string reprs(const edds::DataRepresentationQosPolicy & r)
{
  std::string s = "[";
  for (auto k : r.m_value) { s += std::to_string(static_cast<int>(k)) + " "; }
  return s + "]";
}
static void out(const std::string & line)
{
  std::lock_guard<std::mutex> lock(g_mutex);
  if (g_seen.insert(line).second) { std::cout << line << std::endl; }
}

template<typename D>
static void common(std::ostringstream & os, const char * what, const D & d)
{
  os << what << " topic=" << d.topic_name.to_string() << " type=" << d.type_name.to_string()
     << " guid=" << guid_str(d.guid) << "\n";
  const auto & tip = d.type_information;
  os << "    type_information.assigned=" << (tip.assigned() ? "yes" : "no")
     << " minimal{" << tid_str(tip.type_information.minimal().typeid_with_size().type_id())
     << " size=" << tip.type_information.minimal().typeid_with_size().typeobject_serialized_size()
     << "} complete{" << tid_str(tip.type_information.complete().typeid_with_size().type_id())
     << " size=" << tip.type_information.complete().typeid_with_size().typeobject_serialized_size()
     << "}\n";
  os << "    representation=" << reprs(d.representation)
     << " user_data=\"" << ud(d.user_data.data_vec()) << "\"";
  os << " properties={";
  for (const auto & p : d.properties) { os << p.first() << "=" << p.second() << ";"; }
  os << "}";
}

class Probe : public edds::DomainParticipantListener
{
public:
  void on_participant_discovery(
    edds::DomainParticipant *, rtps::ParticipantDiscoveryStatus reason,
    const edds::ParticipantBuiltinTopicData & info, bool & ignore) override
  {
    ignore = false;
    if (reason != rtps::ParticipantDiscoveryStatus::DISCOVERED_PARTICIPANT &&
      reason != rtps::ParticipantDiscoveryStatus::CHANGED_QOS_PARTICIPANT) { return; }
    std::ostringstream os;
    const auto & pv = info.product_version;
    os << "PARTICIPANT name=" << info.participant_name.to_string()
       << " guid=" << hexbytes(info.guid.guidPrefix.value, 12)
       << " vendor=" << hexbytes(info.vendor_id.data(), 2)
       << " product_version=" << static_cast<int>(pv.major) << "." << static_cast<int>(pv.minor)
       << "." << static_cast<int>(pv.patch) << "." << static_cast<int>(pv.tweak)
       << " user_data=\"" << ud(info.user_data.data_vec()) << "\" properties={";
    for (const auto & p : info.properties) { os << p.first() << "=" << p.second() << ";"; }
    os << "}";
    out(os.str());
  }
  void on_data_writer_discovery(
    edds::DomainParticipant *, rtps::WriterDiscoveryStatus reason,
    const rtps::PublicationBuiltinTopicData & d, bool & ignore) override
  {
    ignore = false;
    if (reason != rtps::WriterDiscoveryStatus::DISCOVERED_WRITER &&
      reason != rtps::WriterDiscoveryStatus::CHANGED_QOS_WRITER) { return; }
    std::ostringstream os;
    common(os, "WRITER", d);
    os << "\n    max_serialized_size=" << d.max_serialized_size;
    out(os.str());
  }
  void on_data_reader_discovery(
    edds::DomainParticipant *, rtps::ReaderDiscoveryStatus reason,
    const rtps::SubscriptionBuiltinTopicData & d, bool & ignore) override
  {
    ignore = false;
    if (reason != rtps::ReaderDiscoveryStatus::DISCOVERED_READER &&
      reason != rtps::ReaderDiscoveryStatus::CHANGED_QOS_READER) { return; }
    std::ostringstream os;
    common(os, "READER", d);
    os << "\n    type_consistency.kind=" << static_cast<int>(d.type_consistency.m_kind)
       << " ignore_member_names=" << d.type_consistency.m_ignore_member_names
       << " prevent_type_widening=" << d.type_consistency.m_prevent_type_widening
       << " force_type_validation=" << d.type_consistency.m_force_type_validation;
    out(os.str());
  }
};

int main(int argc, char ** argv)
{
  const int seconds = argc > 1 ? std::atoi(argv[1]) : 10;
  int domain = std::getenv("ROS_DOMAIN_ID") ? std::atoi(std::getenv("ROS_DOMAIN_ID")) : 0;
  std::cout << "# p210 probe (Fast DDS 3.x) domain " << domain << " for " << seconds << "s" << std::endl;
  Probe probe;
  edds::DomainParticipantQos qos = edds::PARTICIPANT_QOS_DEFAULT;
  qos.name("p210_probe");
  auto * p = edds::DomainParticipantFactory::get_instance()->create_participant(domain, qos, &probe);
  if (!p) { std::cerr << "create_participant failed" << std::endl; return 1; }
  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  edds::DomainParticipantFactory::get_instance()->delete_participant(p);
  return 0;
}
