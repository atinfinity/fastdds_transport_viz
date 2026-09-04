// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/model.hpp"

#include <cstdio>

namespace fastdds_transport_viz
{

std::string to_string(LocatorKind kind)
{
  switch (kind) {
    case LocatorKind::UDPv4: return "UDPv4";
    case LocatorKind::UDPv6: return "UDPv6";
    case LocatorKind::TCPv4: return "TCPv4";
    case LocatorKind::TCPv6: return "TCPv6";
    case LocatorKind::SHM: return "SHM";
    default: return "INVALID";
  }
}

std::string to_string(Transport transport)
{
  switch (transport) {
    case Transport::UDPv4: return "UDPv4";
    case Transport::UDPv6: return "UDPv6";
    case Transport::TCPv4: return "TCPv4";
    case Transport::TCPv6: return "TCPv6";
    case Transport::SHM: return "SHM";
    case Transport::DataSharing: return "DATA_SHARING";
    default: return "NONE";
  }
}

std::string to_string(Confidence confidence)
{
  return confidence == Confidence::Certain ? "certain" : "likely";
}

std::string to_string(DataSharingKind kind)
{
  switch (kind) {
    case DataSharingKind::Off: return "OFF";
    case DataSharingKind::On: return "ON";
    case DataSharingKind::Auto: return "AUTO";
    default: return "UNKNOWN";
  }
}

std::string host_id_hex(const HostId & id)
{
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02x%02x%02x%02x", id[0], id[1], id[2], id[3]);
  return buf;
}

}  // namespace fastdds_transport_viz
