// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// A raw Fast DDS DomainParticipant whose listener records every discovered
// writer / reader together with its announced locators and QoS.

#ifndef FASTDDS_TRANSPORT_VIZ__DISCOVERY_OBSERVER_HPP_
#define FASTDDS_TRANSPORT_VIZ__DISCOVERY_OBSERVER_HPP_

#include <chrono>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantListener.hpp>

#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

class DiscoveryObserver : public eprosima::fastdds::dds::DomainParticipantListener
{
public:
  explicit DiscoveryObserver(int domain_id);
  ~DiscoveryObserver() override;

  DiscoveryObserver(const DiscoveryObserver &) = delete;
  DiscoveryObserver & operator=(const DiscoveryObserver &) = delete;

  /// Copy of every currently known remote endpoint (excluding this participant).
  std::vector<Endpoint> snapshot() const;

  std::chrono::steady_clock::time_point last_event() const;
  size_t event_count() const;

  /// Host id (first 4 bytes of our GUID prefix) - "local" for display purposes.
  HostId local_host_id() const;

  /// The underlying participant (used by StatsObserver to add readers).
  eprosima::fastdds::dds::DomainParticipant * participant() const {return participant_;}

  void on_participant_discovery(
    eprosima::fastdds::dds::DomainParticipant * participant,
    eprosima::fastrtps::rtps::ParticipantDiscoveryInfo && info) override;

  void on_subscriber_discovery(
    eprosima::fastdds::dds::DomainParticipant * participant,
    eprosima::fastrtps::rtps::ReaderDiscoveryInfo && info) override;

  void on_publisher_discovery(
    eprosima::fastdds::dds::DomainParticipant * participant,
    eprosima::fastrtps::rtps::WriterDiscoveryInfo && info) override;

private:
  void upsert(Endpoint && e);
  void erase(const std::string & guid);
  void touch();

  eprosima::fastdds::dds::DomainParticipant * participant_{nullptr};
  HostId local_host_id_{};
  mutable std::mutex mutex_;
  std::map<std::string, Endpoint> endpoints_;
  std::chrono::steady_clock::time_point last_event_;
  size_t event_count_{0};
};

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__DISCOVERY_OBSERVER_HPP_
