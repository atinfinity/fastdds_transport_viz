// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Conversions between Fast DDS RTPS types and the plain model.

#ifndef FASTDDS_TRANSPORT_VIZ__FASTDDS_UTIL_HPP_
#define FASTDDS_TRANSPORT_VIZ__FASTDDS_UTIL_HPP_

#include <string>

#include <fastdds/dds/core/policy/QosPolicies.hpp>

#include "fastdds_transport_viz/fastdds_compat.hpp"
#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

/// "xx.xx.(12 bytes)|xx.xx.xx.xx" - same format everywhere in the tool.
std::string guid_to_string(const ftv_rtps::GUID_t & guid);
std::string prefix_to_string(const ftv_rtps::GuidPrefix_t & prefix);
Locator convert_locator(const ftv_rtps::Locator_t & l);

/// Announced QoS -> model strings / kinds.
std::string reliability_to_string(const eprosima::fastdds::dds::ReliabilityQosPolicy & q);
std::string durability_to_string(const eprosima::fastdds::dds::DurabilityQosPolicy & q);
void fill_data_sharing(const eprosima::fastdds::dds::DataSharingQosPolicy & q, EndpointQos & out);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__FASTDDS_UTIL_HPP_
