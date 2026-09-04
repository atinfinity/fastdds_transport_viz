// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Conversions between Fast DDS RTPS types and the plain model.

#ifndef FASTDDS_TRANSPORT_VIZ__FASTDDS_UTIL_HPP_
#define FASTDDS_TRANSPORT_VIZ__FASTDDS_UTIL_HPP_

#include <string>

#include <fastdds/rtps/common/Guid.h>
#include <fastdds/rtps/common/Locator.h>

#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

/// "xx.xx.(12 bytes)|xx.xx.xx.xx" - same format everywhere in the tool.
std::string guid_to_string(const eprosima::fastrtps::rtps::GUID_t & guid);
std::string prefix_to_string(const eprosima::fastrtps::rtps::GuidPrefix_t & prefix);
Locator convert_locator(const eprosima::fastrtps::rtps::Locator_t & l);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__FASTDDS_UTIL_HPP_
