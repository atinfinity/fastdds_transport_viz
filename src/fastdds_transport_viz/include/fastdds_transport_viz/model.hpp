// Copyright 2026 dandelion
// SPDX-License-Identifier: Apache-2.0
//
// Plain data model shared by the observers, the decision logic and the
// renderers. Deliberately free of Fast DDS / ROS types so that decision.cpp
// can be unit-tested without a DDS runtime.

#ifndef FASTDDS_TRANSPORT_VIZ__MODEL_HPP_
#define FASTDDS_TRANSPORT_VIZ__MODEL_HPP_

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace fastdds_transport_viz
{

/// Locator kinds as announced during discovery (Fast DDS LOCATOR_KIND_*).
enum class LocatorKind
{
  Invalid,
  UDPv4,
  UDPv6,
  TCPv4,
  TCPv6,
  SHM,
};

struct Locator
{
  LocatorKind kind{LocatorKind::Invalid};
  std::string address;   // dotted IPv4 / IPv6 text; empty for SHM
  uint32_t port{0};
};

enum class DataSharingKind
{
  Unknown,
  Off,
  On,
  Auto,
};

struct EndpointQos
{
  std::string reliability;   // "RELIABLE" / "BEST_EFFORT"
  std::string durability;    // "VOLATILE" / "TRANSIENT_LOCAL" / ...
  DataSharingKind data_sharing{DataSharingKind::Unknown};
  std::vector<uint64_t> data_sharing_domains;   // effective domain ids announced
};

/// Host identity = first 4 bytes of the GUID prefix (what Fast DDS itself uses
/// to decide "same host").
using HostId = std::array<uint8_t, 4>;

struct Endpoint
{
  bool is_writer{false};
  std::string guid;               // 16 bytes as "xx.xx....|xx.xx.xx.xx"
  std::array<uint8_t, 16> guid_bytes{};
  HostId host_id{};
  std::string participant_guid_prefix;   // 12 bytes hex, dotted
  std::string dds_topic;
  std::string dds_type;
  std::string ros_topic;          // demangled; empty when not a ROS topic
  std::string ros_type;           // demangled; empty when not a ROS type
  std::string node_name;          // fully-qualified ROS node name, may be empty
  std::vector<Locator> unicast;
  std::vector<Locator> multicast;
  EndpointQos qos;
};

enum class Transport
{
  UDPv4,
  UDPv6,
  TCPv4,
  TCPv6,
  SHM,
  DataSharing,
  None,
};

enum class Confidence
{
  Certain,
  Likely,
};

struct Verdict
{
  Transport transport{Transport::None};
  Confidence confidence{Confidence::Certain};
  std::vector<std::string> reasons;    // machine-readable reason codes
  std::vector<std::string> warnings;   // machine-readable warning codes
};

struct Pair
{
  const Endpoint * writer{nullptr};
  const Endpoint * reader{nullptr};
  Verdict verdict;
};

struct TopicSummary
{
  std::string dds_topic;
  std::string display_topic;    // ROS name when available, else DDS name
  std::string display_type;
  bool is_ros_topic{false};
  std::vector<const Endpoint *> writers;
  std::vector<const Endpoint *> readers;
  std::vector<Pair> pairs;
  std::vector<std::string> unmatched_reasons;   // e.g. no-matching-reader
};

struct Snapshot
{
  int domain{0};
  std::string observed_at;      // ISO-8601 UTC
  double observation_seconds{0.0};
  HostId local_host_id{};
  std::vector<Endpoint> endpoints;
  std::vector<TopicSummary> topics;
};

// ---- small helpers -------------------------------------------------------

std::string to_string(LocatorKind kind);
std::string to_string(Transport transport);
std::string to_string(Confidence confidence);
std::string to_string(DataSharingKind kind);
std::string host_id_hex(const HostId & id);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__MODEL_HPP_
