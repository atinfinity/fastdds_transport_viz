// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/ros_discovery_info_observer.hpp"

#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>
#include <fastcdr/config.h>
#include <rosidl_typesupport_fastrtps_cpp/message_type_support.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/qos/DomainParticipantQos.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>
#include <fastdds/dds/topic/TopicDataType.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>

#include <rmw_dds_common/msg/participant_entities_info.hpp>
#include <rmw_dds_common/msg/detail/participant_entities_info__rosidl_typesupport_fastrtps_cpp.hpp>

#include "fastdds_transport_viz/fastdds_compat.hpp"
#include "fastdds_transport_viz/fastdds_util.hpp"

#if FTV_SAME_HOST_LOCATORS_FILTERED
#include <fastdds/rtps/transport/UDPv4TransportDescriptor.h>
#include <fastdds/rtps/transport/shared_mem/SharedMemTransportDescriptor.h>
#endif

namespace fastdds_transport_viz
{

namespace dds = eprosima::fastdds::dds;
namespace rtps = ftv_rtps;
using ParticipantEntitiesInfo = rmw_dds_common::msg::ParticipantEntitiesInfo;

namespace
{

const message_type_support_callbacks_t * callbacks()
{
  const rosidl_message_type_support_t * ts =
    ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(
    rosidl_typesupport_fastrtps_cpp, rmw_dds_common, msg, ParticipantEntitiesInfo)();
  return static_cast<const message_type_support_callbacks_t *>(ts->data);
}

bool serialize_message(
  const message_type_support_callbacks_t * cb, const void * data,
  rtps::SerializedPayload_t & payload)
{
  eprosima::fastcdr::FastBuffer buffer(reinterpret_cast<char *>(payload.data), payload.max_size);
  // plain CDR (XCDR1), what rmw_fastrtps writes
#if FASTCDR_VERSION_MAJOR == 1
  eprosima::fastcdr::Cdr ser(
    buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::Cdr::DDS_CDR);
#else
  eprosima::fastcdr::Cdr ser(
    buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);
  ser.set_encoding_flag(eprosima::fastcdr::EncodingAlgorithmFlag::PLAIN_CDR);
#endif
  payload.encapsulation =
    ser.endianness() == eprosima::fastcdr::Cdr::BIG_ENDIANNESS ? CDR_BE : CDR_LE;
  try {
    ser.serialize_encapsulation();
    if (!cb->cdr_serialize(data, ser)) {return false;}
  } catch (const std::exception &) {   // the payload is too small
    return false;
  }
#if FASTCDR_VERSION_MAJOR == 1
  payload.length = static_cast<uint32_t>(ser.getSerializedDataLength());
#else
  payload.length = static_cast<uint32_t>(ser.get_serialized_data_length());
#endif
  return true;
}

bool deserialize_message(
  const message_type_support_callbacks_t * cb, rtps::SerializedPayload_t & payload, void * data)
{
  eprosima::fastcdr::FastBuffer buffer(reinterpret_cast<char *>(payload.data), payload.length);
#if FASTCDR_VERSION_MAJOR == 1
  eprosima::fastcdr::Cdr deser(
    buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::Cdr::DDS_CDR);
#else
  eprosima::fastcdr::Cdr deser(buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN);
#endif
  try {
    deser.read_encapsulation();
    return cb->cdr_deserialize(deser, data);
  } catch (const std::exception &) {   // truncated, or another distribution's gid size
    return false;
  }
}

class ParticipantEntitiesInfoType : public dds::TopicDataType
{
public:
  ParticipantEntitiesInfoType()
  : cb_(callbacks())
  {
#if FTV_FASTDDS_3
    set_name(RosDiscoveryInfoObserver::kTypeName);
    max_serialized_type_size = kInitialSize;
    is_compute_key_provided = false;
#else
    setName(RosDiscoveryInfoObserver::kTypeName);
    m_typeSize = kInitialSize;
    m_isGetKeyDefined = false;
    auto_fill_type_object(false);
    auto_fill_type_information(false);
#endif
  }

#if FTV_FASTDDS_3
  bool serialize(
    const void * const data, rtps::SerializedPayload_t & payload,
    dds::DataRepresentationId_t) override
  {
    return serialize_message(cb_, data, payload);
  }
  bool deserialize(rtps::SerializedPayload_t & payload, void * data) override
  {
    return deserialize_message(cb_, payload, data);
  }
  uint32_t calculate_serialized_size(const void * const data, dds::DataRepresentationId_t) override
  {
    return 4 + static_cast<uint32_t>(cb_->get_serialized_size(data));
  }
  void * create_data() override {return new ParticipantEntitiesInfo();}
  void delete_data(void * data) override {delete static_cast<ParticipantEntitiesInfo *>(data);}
  bool compute_key(rtps::SerializedPayload_t &, rtps::InstanceHandle_t &, bool) override
  {
    return false;
  }
  bool compute_key(const void * const, rtps::InstanceHandle_t &, bool) override {return false;}
#else
  bool serialize(void * data, rtps::SerializedPayload_t * payload) override
  {
    return serialize_message(cb_, data, *payload);
  }
  bool deserialize(rtps::SerializedPayload_t * payload, void * data) override
  {
    return deserialize_message(cb_, *payload, data);
  }
  std::function<uint32_t()> getSerializedSizeProvider(void * data) override
  {
    return [this, data]() {return 4 + static_cast<uint32_t>(cb_->get_serialized_size(data));};
  }
  void * createData() override {return new ParticipantEntitiesInfo();}
  void deleteData(void * data) override {delete static_cast<ParticipantEntitiesInfo *>(data);}
  bool getKey(void *, rtps::InstanceHandle_t *, bool) override {return false;}
#endif

private:
  // unbounded sequences: the readers reallocate (PREALLOCATED_WITH_REALLOC)
  static constexpr uint32_t kInitialSize = 1024;
  const message_type_support_callbacks_t * cb_;
};

/// The first 16 bytes of an rmw gid (24 bytes on Humble, 16 on Jazzy and newer).
template<typename Gid>
EndpointGid to_endpoint_gid(const Gid & gid)
{
  EndpointGid out{};
  std::copy_n(gid.data.begin(), std::min(out.size(), gid.data.size()), out.begin());
  return out;
}

#if FTV_SAME_HOST_LOCATORS_FILTERED
/// A participant like `like` (same domain and profile) without the SHM transport, or nullptr
/// when SHM is its only transport.
dds::DomainParticipant * create_participant_without_shm(dds::DomainParticipant * like)
{
  namespace transport = eprosima::fastdds::rtps;
  dds::DomainParticipantQos qos = like->get_qos();
  qos.name("fastdds_transport_viz_names");
  auto & user = qos.transport().user_transports;
  user.erase(
    std::remove_if(
      user.begin(), user.end(), [](const auto & t) {
        return std::dynamic_pointer_cast<transport::SharedMemTransportDescriptor>(t) != nullptr;
      }), user.end());
  if (qos.transport().use_builtin_transports) {   // UDPv4 and SHM
    qos.transport().use_builtin_transports = false;
    user.push_back(std::make_shared<transport::UDPv4TransportDescriptor>());
  }
  if (user.empty()) {
    return nullptr;
  }
  return dds::DomainParticipantFactory::get_instance()->create_participant(
    like->get_domain_id(), qos);
}
#endif

}  // namespace

dds::TypeSupport RosDiscoveryInfoObserver::make_type_support()
{
  return dds::TypeSupport(new ParticipantEntitiesInfoType());
}

RosDiscoveryInfoObserver::RosDiscoveryInfoObserver(dds::DomainParticipant * participant)
: participant_(participant)
{
#if FTV_SAME_HOST_LOCATORS_FILTERED
  // Fast DDS 2.6 keeps only the SHM locator of a same-host endpoint in the discovery data of a
  // participant with SHM: from `participant` the nodes' writers are reachable over SHM alone,
  // which a node in another IPC namespace never receives. Read with a participant without SHM.
  owned_participant_ = create_participant_without_shm(participant);
  if (owned_participant_ == nullptr) {
    throw std::runtime_error("failed to create a participant without SHM for ros_discovery_info");
  }
  participant_ = owned_participant_;
#endif
  make_type_support().register_type(participant_);
  topic_ = participant_->create_topic(kTopicName, kTypeName, dds::TOPIC_QOS_DEFAULT);
  subscriber_ = participant_->create_subscriber(dds::SUBSCRIBER_QOS_DEFAULT);
  if (topic_ == nullptr || subscriber_ == nullptr) {
    release();
    throw std::runtime_error("failed to create the ros_discovery_info topic");
  }
  // the reader of rmw_dds_common's graph cache
  dds::DataReaderQos qos = dds::DATAREADER_QOS_DEFAULT;
  qos.reliability().kind = dds::RELIABLE_RELIABILITY_QOS;
  qos.durability().kind = dds::TRANSIENT_LOCAL_DURABILITY_QOS;
  qos.history().kind = dds::KEEP_ALL_HISTORY_QOS;
  qos.endpoint().history_memory_policy = rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
  qos.data_sharing().off();
  // like the statistics readers (#106): no SHM locator, so that a same-host node in another
  // IPC namespace sends its samples over UDP (or TCP) instead of into its own /dev/shm
  const auto locators = probe_non_shm_unicast_locators(subscriber_, topic_, qos);
  if (!locators.empty()) {
    qos.endpoint().unicast_locator_list = locators;
  }
  reader_ = subscriber_->create_datareader(topic_, qos);
  if (reader_ == nullptr) {
    release();
    throw std::runtime_error("failed to create the ros_discovery_info reader");
  }
}

RosDiscoveryInfoObserver::~RosDiscoveryInfoObserver()
{
  release();
}

void RosDiscoveryInfoObserver::release()
{
  if (reader_ != nullptr) {subscriber_->delete_datareader(reader_);}
  if (subscriber_ != nullptr) {participant_->delete_subscriber(subscriber_);}
  if (topic_ != nullptr) {participant_->delete_topic(topic_);}
  if (owned_participant_ != nullptr) {
    dds::DomainParticipantFactory::get_instance()->delete_participant(owned_participant_);
  }
  reader_ = nullptr;
  subscriber_ = nullptr;
  topic_ = nullptr;
  owned_participant_ = nullptr;
}

void RosDiscoveryInfoObserver::poll()
{
  ParticipantEntitiesInfo msg;
  dds::SampleInfo info;
  while (retcode_ok(reader_->take_next_sample(&msg, &info))) {
    if (!info.valid_data) {continue;}
    ++samples_taken_;
    ParticipantPrefix participant{};
    std::copy_n(to_endpoint_gid(msg.gid).begin(), participant.size(), participant.begin());
    std::vector<NodeEntities> nodes;
    nodes.reserve(msg.node_entities_info_seq.size());
    for (const auto & n : msg.node_entities_info_seq) {
      NodeEntities node;
      node.node_namespace = n.node_namespace;
      node.node_name = n.node_name;
      for (const auto & gid : n.reader_gid_seq) {
        node.reader_gids.push_back(to_endpoint_gid(gid));
      }
      for (const auto & gid : n.writer_gid_seq) {
        node.writer_gids.push_back(to_endpoint_gid(gid));
      }
      nodes.push_back(std::move(node));
    }
    table_.update(participant, nodes);
  }
}

}  // namespace fastdds_transport_viz
