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
#include <fastrtps/utils/IPLocator.h>
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
inline bool retcode_ok(const ReturnCode_t & code)
{
  return code == ReturnCode_t::RETCODE_OK;
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
#else
template<typename D> const ftv_rtps::GUID_t & disc_guid(const D & d) {return d.guid();}
template<typename D> std::string disc_topic(const D & d) {return d.topicName().to_string();}
template<typename D> std::string disc_type(const D & d) {return d.typeName().to_string();}
template<typename D> const auto & disc_locators(const D & d) {return d.remote_locators();}
template<typename D> const auto & disc_reliability(const D & d) {return d.m_qos.m_reliability;}
template<typename D> const auto & disc_durability(const D & d) {return d.m_qos.m_durability;}
template<typename D> const auto & disc_data_sharing(const D & d) {return d.m_qos.data_sharing;}
#endif

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__FASTDDS_COMPAT_HPP_
