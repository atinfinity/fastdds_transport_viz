// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Fast DDS 2.14 (ROS 2 Jazzy) / 3.x (Kilted, Rolling) compatibility: header names,
// namespaces, return codes and the discovery listener API differ between the two.
#ifndef FASTDDS_TRANSPORT_VIZ__FASTDDS_COMPAT_HPP_
#define FASTDDS_TRANSPORT_VIZ__FASTDDS_COMPAT_HPP_

#include <string>

#if __has_include(<fastdds/config.hpp>)
#include <fastdds/config.hpp>          // Fast DDS 3.x: FASTDDS_VERSION_MAJOR
#else
#include <fastrtps/config.h>           // Fast DDS 2.x
#define FASTDDS_VERSION_MAJOR FASTRTPS_VERSION_MAJOR
#endif

#define FTV_FASTDDS_3 (FASTDDS_VERSION_MAJOR >= 3)

#if FTV_FASTDDS_3
#include <fastdds/dds/core/ReturnCode.hpp>
#include <fastdds/rtps/attributes/ResourceManagement.hpp>
#include <fastdds/rtps/builtin/data/ParticipantBuiltinTopicData.hpp>
#include <fastdds/rtps/builtin/data/PublicationBuiltinTopicData.hpp>
#include <fastdds/rtps/builtin/data/SubscriptionBuiltinTopicData.hpp>
#include <fastdds/rtps/common/Guid.hpp>
#include <fastdds/rtps/common/Locator.hpp>
#include <fastdds/rtps/common/RemoteLocators.hpp>
#include <fastdds/rtps/participant/ParticipantDiscoveryInfo.hpp>
#include <fastdds/rtps/reader/ReaderDiscoveryStatus.hpp>
#include <fastdds/rtps/writer/WriterDiscoveryStatus.hpp>
#include <fastdds/utils/IPLocator.hpp>
#else
#include <fastdds/rtps/common/Guid.h>
#include <fastdds/rtps/common/Locator.h>
#include <fastdds/rtps/common/RemoteLocators.hpp>
#include <fastdds/rtps/participant/ParticipantDiscoveryInfo.h>
#include <fastdds/rtps/reader/ReaderDiscoveryInfo.h>
#include <fastdds/rtps/writer/WriterDiscoveryInfo.h>
#include <fastrtps/types/TypesBase.h>   // ReturnCode_t
#include <fastrtps/utils/IPLocator.h>
#endif

/// Fast DDS 3.x always ships the statistics module; 2.x only when built with
/// FASTDDS_STATISTICS (ROS 2 Jazzy: yes; Humble's 2.6 binary: no).
#if FTV_FASTDDS_3 || defined(FASTDDS_STATISTICS)
#define FTV_HAS_STATISTICS 1
#else
#define FTV_HAS_STATISTICS 0
#endif

/// Fast DDS below 2.10 delivers only the SHM locator of a participant on the same host
/// (its UDP locators are filtered out of the discovery data the tool receives).
#if !FTV_FASTDDS_3 && (FASTRTPS_VERSION_MAJOR < 2 || (FASTRTPS_VERSION_MAJOR == 2 && FASTRTPS_VERSION_MINOR < 10))
#define FTV_SAME_HOST_LOCATORS_FILTERED 1
#else
#define FTV_SAME_HOST_LOCATORS_FILTERED 0
#endif

namespace fastdds_transport_viz
{

#if FTV_FASTDDS_3
namespace ftv_rtps = eprosima::fastdds::rtps;
inline bool retcode_ok(eprosima::fastdds::dds::ReturnCode_t code)
{
  return code == eprosima::fastdds::dds::RETCODE_OK;
}
#else
namespace ftv_rtps = eprosima::fastrtps::rtps;
inline bool retcode_ok(const eprosima::fastrtps::types::ReturnCode_t & code)
{
  return code == eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK;
}
#endif

/// Accessors over the discovery data of a remote endpoint (2.x: WriterProxyData /
/// ReaderProxyData; 3.x: PublicationBuiltinTopicData / SubscriptionBuiltinTopicData).
#if FTV_FASTDDS_3
template<typename D> const ftv_rtps::GUID_t & disc_guid(const D & d) {return d.guid;}
template<typename D> std::string disc_topic(const D & d) {return d.topic_name.to_string();}
template<typename D> std::string disc_type(const D & d) {return d.type_name.to_string();}
template<typename D> const auto & disc_locators(const D & d) {return d.remote_locators;}
template<typename D> const auto & disc_reliability(const D & d) {return d.reliability;}
template<typename D> const auto & disc_durability(const D & d) {return d.durability;}
template<typename D> const auto & disc_data_sharing(const D & d) {return d.data_sharing;}
template<typename D> const auto & disc_deadline(const D & d) {return d.deadline;}
template<typename D> const auto & disc_liveliness(const D & d) {return d.liveliness;}
template<typename D> const auto & disc_ownership(const D & d) {return d.ownership;}
template<typename D> const auto & disc_partition(const D & d) {return d.partition;}
#else
template<typename D> const ftv_rtps::GUID_t & disc_guid(const D & d) {return d.guid();}
template<typename D> std::string disc_topic(const D & d) {return d.topicName().to_string();}
template<typename D> std::string disc_type(const D & d) {return d.typeName().to_string();}
template<typename D> const auto & disc_locators(const D & d) {return d.remote_locators();}
template<typename D> const auto & disc_reliability(const D & d) {return d.m_qos.m_reliability;}
template<typename D> const auto & disc_durability(const D & d) {return d.m_qos.m_durability;}
template<typename D> const auto & disc_data_sharing(const D & d) {return d.m_qos.data_sharing;}
template<typename D> const auto & disc_deadline(const D & d) {return d.m_qos.m_deadline;}
template<typename D> const auto & disc_liveliness(const D & d) {return d.m_qos.m_liveliness;}
template<typename D> const auto & disc_ownership(const D & d) {return d.m_qos.m_ownership;}
template<typename D> const auto & disc_partition(const D & d) {return d.m_qos.m_partition;}
#endif

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__FASTDDS_COMPAT_HPP_
