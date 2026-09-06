// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/discovery_observer.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>

#include "fastdds_transport_viz/fastdds_compat.hpp"
#include "fastdds_transport_viz/fastdds_util.hpp"
#include "fastdds_transport_viz/ros_names.hpp"

namespace fastdds_transport_viz
{

namespace dds = eprosima::fastdds::dds;
namespace rtps = ftv_rtps;

std::string guid_to_string(const rtps::GUID_t & guid)
{
  char buf[64];
  int n = 0;
  for (int i = 0; i < 12; ++i) {
    n += std::snprintf(buf + n, sizeof(buf) - n, "%02x%s", guid.guidPrefix.value[i], i < 11 ? "." : "|");
  }
  for (int i = 0; i < 4; ++i) {
    n += std::snprintf(buf + n, sizeof(buf) - n, "%02x%s", guid.entityId.value[i], i < 3 ? "." : "");
  }
  return buf;
}

std::string prefix_to_string(const rtps::GuidPrefix_t & prefix)
{
  char buf[48];
  int n = 0;
  for (int i = 0; i < 12; ++i) {
    n += std::snprintf(buf + n, sizeof(buf) - n, "%02x%s", prefix.value[i], i < 11 ? "." : "");
  }
  return buf;
}

Locator convert_locator(const rtps::Locator_t & l)
{
  Locator out;
  out.port = l.port;
  switch (l.kind) {
    case LOCATOR_KIND_UDPv4:
      out.kind = LocatorKind::UDPv4;
      out.address = rtps::IPLocator::toIPv4string(l);
      break;
    case LOCATOR_KIND_UDPv6:
      out.kind = LocatorKind::UDPv6;
      out.address = rtps::IPLocator::toIPv6string(l);
      break;
    case LOCATOR_KIND_TCPv4:
      out.kind = LocatorKind::TCPv4;
      out.address = rtps::IPLocator::toIPv4string(l);
      out.port = rtps::IPLocator::getPhysicalPort(l);
      break;
    case LOCATOR_KIND_TCPv6:
      out.kind = LocatorKind::TCPv6;
      out.address = rtps::IPLocator::toIPv6string(l);
      out.port = rtps::IPLocator::getPhysicalPort(l);
      break;
    case LOCATOR_KIND_SHM:
      out.kind = LocatorKind::SHM;
      break;
    default:
      out.kind = LocatorKind::Invalid;
      break;
  }
  return out;
}

namespace
{

void fill_locators(const rtps::RemoteLocatorList & list, Endpoint & e)
{
  for (const auto & l : list.unicast) {
    e.unicast.push_back(convert_locator(l));
  }
  for (const auto & l : list.multicast) {
    e.multicast.push_back(convert_locator(l));
  }
}

}  // namespace

std::string reliability_to_string(const dds::ReliabilityQosPolicy & q)
{
  return q.kind == dds::RELIABLE_RELIABILITY_QOS ? "RELIABLE" : "BEST_EFFORT";
}

std::string durability_to_string(const dds::DurabilityQosPolicy & q)
{
  switch (q.kind) {
    case dds::VOLATILE_DURABILITY_QOS: return "VOLATILE";
    case dds::TRANSIENT_LOCAL_DURABILITY_QOS: return "TRANSIENT_LOCAL";
    case dds::TRANSIENT_DURABILITY_QOS: return "TRANSIENT";
    case dds::PERSISTENT_DURABILITY_QOS: return "PERSISTENT";
    default: return "UNKNOWN";
  }
}

void fill_data_sharing(const dds::DataSharingQosPolicy & q, EndpointQos & out)
{
  switch (q.kind()) {
    case dds::OFF: out.data_sharing = DataSharingKind::Off; break;
    case dds::ON: out.data_sharing = DataSharingKind::On; break;
    case dds::AUTO: out.data_sharing = DataSharingKind::Auto; break;
    default: out.data_sharing = DataSharingKind::Unknown; break;
  }
  out.data_sharing_domains = q.domain_ids();
}

namespace
{

template<typename ProxyData>
Endpoint make_endpoint(const ProxyData & data, bool is_writer)
{
  Endpoint e;
  e.is_writer = is_writer;
  const auto & guid = disc_guid(data);
  e.guid = guid_to_string(guid);
  for (int i = 0; i < 12; ++i) {
    e.guid_bytes[i] = guid.guidPrefix.value[i];
  }
  for (int i = 0; i < 4; ++i) {
    e.guid_bytes[12 + i] = guid.entityId.value[i];
    e.host_id[i] = guid.guidPrefix.value[i];
  }
  e.participant_guid_prefix = prefix_to_string(guid.guidPrefix);
  e.dds_topic = disc_topic(data);
  e.dds_type = disc_type(data);
  auto ros = demangle_topic(e.dds_topic);
  if (ros.kind != RosEntityKind::NotRos) {
    e.ros_topic = ros.name;
  }
  e.ros_type = demangle_type(e.dds_type);
  fill_locators(disc_locators(data), e);
  e.qos.reliability = reliability_to_string(disc_reliability(data));
  e.qos.durability = durability_to_string(disc_durability(data));
  fill_data_sharing(disc_data_sharing(data), e.qos);
  return e;
}

}  // namespace

DiscoveryObserver::DiscoveryObserver(int domain_id)
: last_event_(std::chrono::steady_clock::now())
{
  // The factory default honours the default participant profile of
  // FASTRTPS_DEFAULT_PROFILES_FILE (PARTICIPANT_QOS_DEFAULT is the built-in constant),
  // so the observer participant discovers the same way the observed nodes do.
  auto * factory = dds::DomainParticipantFactory::get_instance();
  dds::DomainParticipantQos qos = factory->get_default_participant_qos();
  qos.name("fastdds_transport_viz");
  participant_ = factory->create_participant(
    static_cast<dds::DomainId_t>(domain_id), qos, this, dds::StatusMask::none());
  if (participant_ == nullptr) {
    throw std::runtime_error("failed to create Fast DDS DomainParticipant");
  }
  const auto & prefix = participant_->guid().guidPrefix;
  for (int i = 0; i < 4; ++i) {
    local_host_id_[i] = prefix.value[i];
  }
}

DiscoveryObserver::~DiscoveryObserver()
{
  if (participant_ != nullptr) {
    dds::DomainParticipantFactory::get_instance()->delete_participant(participant_);
  }
}

std::vector<Endpoint> DiscoveryObserver::snapshot() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<Endpoint> out;
  out.reserve(endpoints_.size());
  for (const auto & kv : endpoints_) {
    out.push_back(kv.second);
  }
  return out;
}

std::chrono::steady_clock::time_point DiscoveryObserver::last_event() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return last_event_;
}

size_t DiscoveryObserver::event_count() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return event_count_;
}

HostId DiscoveryObserver::local_host_id() const
{
  return local_host_id_;
}

void DiscoveryObserver::touch()
{
  last_event_ = std::chrono::steady_clock::now();
  ++event_count_;
}

void DiscoveryObserver::upsert(Endpoint && e)
{
  std::lock_guard<std::mutex> lock(mutex_);
  endpoints_[e.guid] = std::move(e);
  touch();
}

void DiscoveryObserver::erase(const std::string & guid)
{
  std::lock_guard<std::mutex> lock(mutex_);
  endpoints_.erase(guid);
  touch();
}

#if FTV_FASTDDS_3
void DiscoveryObserver::on_participant_discovery(
  dds::DomainParticipant *, rtps::ParticipantDiscoveryStatus,
  const rtps::ParticipantBuiltinTopicData &, bool & should_be_ignored)
{
  should_be_ignored = false;
  std::lock_guard<std::mutex> lock(mutex_);
  touch();
}

void DiscoveryObserver::on_data_reader_discovery(
  dds::DomainParticipant *, rtps::ReaderDiscoveryStatus reason,
  const rtps::SubscriptionBuiltinTopicData & info, bool & should_be_ignored)
{
  should_be_ignored = false;
  switch (reason) {
    case rtps::ReaderDiscoveryStatus::DISCOVERED_READER:
    case rtps::ReaderDiscoveryStatus::CHANGED_QOS_READER:
      upsert(make_endpoint(info, false));
      break;
    case rtps::ReaderDiscoveryStatus::REMOVED_READER:
      erase(guid_to_string(info.guid));
      break;
    default:
      break;
  }
}

void DiscoveryObserver::on_data_writer_discovery(
  dds::DomainParticipant *, rtps::WriterDiscoveryStatus reason,
  const rtps::PublicationBuiltinTopicData & info, bool & should_be_ignored)
{
  should_be_ignored = false;
  switch (reason) {
    case rtps::WriterDiscoveryStatus::DISCOVERED_WRITER:
    case rtps::WriterDiscoveryStatus::CHANGED_QOS_WRITER:
      upsert(make_endpoint(info, true));
      break;
    case rtps::WriterDiscoveryStatus::REMOVED_WRITER:
      erase(guid_to_string(info.guid));
      break;
    default:
      break;
  }
}
#else
void DiscoveryObserver::on_participant_discovery(
  dds::DomainParticipant *, rtps::ParticipantDiscoveryInfo &&)
{
  std::lock_guard<std::mutex> lock(mutex_);
  touch();
}

void DiscoveryObserver::on_subscriber_discovery(
  dds::DomainParticipant *, rtps::ReaderDiscoveryInfo && info)
{
  switch (info.status) {
    case rtps::ReaderDiscoveryInfo::DISCOVERED_READER:
    case rtps::ReaderDiscoveryInfo::CHANGED_QOS_READER:
      upsert(make_endpoint(info.info, false));
      break;
    case rtps::ReaderDiscoveryInfo::REMOVED_READER:
      erase(guid_to_string(info.info.guid()));
      break;
    default:
      break;
  }
}

void DiscoveryObserver::on_publisher_discovery(
  dds::DomainParticipant *, rtps::WriterDiscoveryInfo && info)
{
  switch (info.status) {
    case rtps::WriterDiscoveryInfo::DISCOVERED_WRITER:
    case rtps::WriterDiscoveryInfo::CHANGED_QOS_WRITER:
      upsert(make_endpoint(info.info, true));
      break;
    case rtps::WriterDiscoveryInfo::REMOVED_WRITER:
      erase(guid_to_string(info.info.guid()));
      break;
    default:
      break;
  }
}

#endif

}  // namespace fastdds_transport_viz
