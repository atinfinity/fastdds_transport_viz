// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/stats_observer.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>

#include <fastdds/dds/core/policy/QosPolicies.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/topic/qos/TopicQos.hpp>
#include <fastdds/statistics/topic_names.hpp>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/fastdds_compat.hpp"
#include "fastdds_transport_viz/fastdds_util.hpp"
#if FTV_FASTDDS_3
#include "typesPubSubTypes.hpp"
#else
#include "typesPubSubTypes.h"
#endif

namespace fastdds_transport_viz
{

namespace dds = eprosima::fastdds::dds;
namespace rtps = ftv_rtps;
namespace st = eprosima::fastdds::statistics;

namespace
{

rtps::GUID_t to_rtps(const st::detail::GUID_s & g)
{
  rtps::GUID_t out;
  for (int i = 0; i < 12; ++i) {
    out.guidPrefix.value[i] = g.guidPrefix().value()[i];
  }
  for (int i = 0; i < 4; ++i) {
    out.entityId.value[i] = g.entityId().value()[i];
  }
  return out;
}

rtps::Locator_t to_rtps(const st::detail::Locator_s & l)
{
  rtps::Locator_t out;
  out.kind = l.kind();
  out.port = l.port();
  std::memcpy(out.address, l.address().data(), 16);
  return out;
}

}  // namespace

void StatsObserver::Listener::on_sample_lost(
  dds::DataReader *, const dds::SampleLostStatus & status)
{
  const std::chrono::steady_clock::duration since(
    std::chrono::steady_clock::now().time_since_epoch().count() - last_match_ticks);
  const double seconds = std::chrono::duration<double>(since).count();
  const auto n = static_cast<uint64_t>(status.total_count_change);
  if (statistics_late_join_window_open(any_match, seconds)) {
    lost_at_start += n;
  } else {
    lost += n;
  }
}

void StatsObserver::Listener::on_sample_rejected(
  dds::DataReader *, const dds::SampleRejectedStatus & status)
{
  rejected += static_cast<uint64_t>(status.total_count_change);
}

void StatsObserver::Listener::on_subscription_matched(
  dds::DataReader *, const dds::SubscriptionMatchedStatus & status)
{
  // Every new writer brings its own burst of history it had already dropped, so each match
  // opens the window again for the grace period; a writer that goes away does not.
  if (status.current_count_change <= 0) {return;}
  last_match_ticks = std::chrono::steady_clock::now().time_since_epoch().count();
  any_match = true;
}

std::string StatsObserver::required_env_value()
{
  return "RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;"
         "DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC;RESENT_DATAS_TOPIC;"
         "HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC";
}

StatsObserver::StatsObserver(dds::DomainParticipant * participant)
: participant_(participant)
{
  subscriber_ = participant_->create_subscriber(dds::SUBSCRIBER_QOS_DEFAULT);
  if (subscriber_ == nullptr) {
    throw std::runtime_error("failed to create statistics subscriber");
  }
  rtps_sent_ = create_reader(
    st::RTPS_SENT_TOPIC, dds::TypeSupport(new st::Entity2LocatorTrafficPubSubType()));
  history_latency_ = create_reader(
    st::HISTORY_LATENCY_TOPIC, dds::TypeSupport(new st::WriterReaderDataPubSubType()));
  physical_data_ = create_reader(
    st::PHYSICAL_DATA_TOPIC, dds::TypeSupport(new st::PhysicalDataPubSubType()));
  data_count_ = create_reader(
    st::DATA_COUNT_TOPIC, dds::TypeSupport(new st::EntityCountPubSubType()));
  throughput_ = create_reader(
    st::PUBLICATION_THROUGHPUT_TOPIC, dds::TypeSupport(new st::EntityDataPubSubType()));
  rtps_lost_ = create_reader(
    st::RTPS_LOST_TOPIC, dds::TypeSupport(new st::Entity2LocatorTrafficPubSubType()));
  resent_datas_ = create_reader(
    st::RESENT_DATAS_TOPIC, dds::TypeSupport(new st::EntityCountPubSubType()));
  heartbeat_count_ = create_reader(
    st::HEARTBEAT_COUNT_TOPIC, dds::TypeSupport(new st::EntityCountPubSubType()));
  gap_count_ = create_reader(
    st::GAP_COUNT_TOPIC, dds::TypeSupport(new st::EntityCountPubSubType()));
  acknack_count_ = create_reader(
    st::ACKNACK_COUNT_TOPIC, dds::TypeSupport(new st::EntityCountPubSubType()));
  nackfrag_count_ = create_reader(
    st::NACKFRAG_COUNT_TOPIC, dds::TypeSupport(new st::EntityCountPubSubType()));
  data_.enabled = true;
}

StatsObserver::~StatsObserver()
{
  for (auto * r : {&rtps_sent_, &history_latency_, &physical_data_, &data_count_, &throughput_,
      &rtps_lost_, &resent_datas_, &heartbeat_count_, &gap_count_, &acknack_count_,
      &nackfrag_count_})
  {
    if (r->reader) {subscriber_->delete_datareader(r->reader);}
    if (r->topic && r->owns_topic) {participant_->delete_topic(r->topic);}
  }
  if (subscriber_) {
    participant_->delete_subscriber(subscriber_);
  }
}

StatsObserver::Reader StatsObserver::create_reader(
  const std::string & topic_name, dds::TypeSupport type)
{
  Reader r;
  type.register_type(participant_);
  // When FASTDDS_STATISTICS is set in our own environment (the docs recommend running the
  // tool in the nodes' environment), Fast DDS has already created the statistics topics
  // on this participant; creating them again fails, so reuse them.
  if (auto * existing = participant_->lookup_topicdescription(topic_name); existing != nullptr) {
    r.topic = dynamic_cast<dds::Topic *>(existing);
    r.owns_topic = false;
  } else {
    r.topic = participant_->create_topic(topic_name, type.get_type_name(), dds::TOPIC_QOS_DEFAULT);
    r.owns_topic = true;
  }
  if (r.topic == nullptr) {
    throw std::runtime_error("failed to create statistics topic " + topic_name);
  }
  // Based on STATISTICS_DATAREADER_QOS (Fast DDS 2.14): reliable, transient-local,
  // preallocated-with-realloc. The statistics topics are keyed (one instance per
  // (source, destination locator) / per participant), and the Fast DDS default
  // resource limits allow only 10 instances per reader - enough to silently drop
  // every sample of a participant once ten (src, locator) pairs are seen. We only
  // need the latest cumulative counter per instance, so: unlimited instances,
  // keep-last 1.
  dds::DataReaderQos qos = dds::DATAREADER_QOS_DEFAULT;
  qos.reliability().kind = dds::RELIABLE_RELIABILITY_QOS;
  qos.durability().kind = dds::TRANSIENT_LOCAL_DURABILITY_QOS;
  qos.history().kind = dds::KEEP_LAST_HISTORY_QOS;
  qos.history().depth = 1;
  qos.resource_limits().max_samples = 0;             // 0 = unlimited in Fast DDS
  qos.resource_limits().max_instances = 0;
  qos.resource_limits().max_samples_per_instance = 0;
  qos.resource_limits().allocated_samples = 100;
  qos.endpoint().history_memory_policy = rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
  // A writer with our host id picks SHM when the reader announces it. From another IPC
  // namespace it then writes into its own /dev/shm and its statistics never arrive. So
  // the readers announce only the non-SHM unicast locators the participant listens on
  // (UDP, or TCP with LARGE_DATA), read once from a probe reader.
  if (!reader_locators_probed_) {
    reader_locators_probed_ = true;
    reader_locators_ = probe_non_shm_unicast_locators(subscriber_, r.topic, qos);
  }
  if (!reader_locators_.empty()) {
    qos.endpoint().unicast_locator_list = reader_locators_;
  }
  r.reader = subscriber_->create_datareader(
    r.topic, qos, &listener_,
    dds::StatusMask::sample_lost() << dds::StatusMask::sample_rejected() <<
      dds::StatusMask::subscription_matched());
  if (r.reader == nullptr) {
    throw std::runtime_error("failed to create statistics reader for " + topic_name);
  }
  return r;
}

void StatsObserver::drain()
{
  dds::SampleInfo info;
  // A statistics source is the participant that published a sample. The payload can name
  // another one (RTPS_LOST src_guid, HISTORY_LATENCY writer_guid), which says nothing about
  // whether that participant has statistics enabled.
  auto count_sample = [&]() {
      ++data_.samples;
      data_.participants_with_stats.insert(sample_publisher_prefix(info));
    };

  st::Entity2LocatorTraffic traffic;
  while (retcode_ok(rtps_sent_.reader->take_next_sample(&traffic, &info))) {
    if (!info.valid_data) {continue;}
    count_sample();
    rtps::GUID_t src = to_rtps(traffic.src_guid());
    Locator dst = convert_locator(to_rtps(traffic.dst_locator()));
    TrafficSample s;
    s.src_participant_prefix = prefix_to_string(src.guidPrefix);
    s.dst = dst;
    s.packets = traffic.packet_count();
    // byte_count is the cumulative byte total; byte_magnitude_order is only
    // floor(log10(byte_count)) (see StatisticsParticipantImpl::on_rtps_sent).
    s.bytes = static_cast<double>(traffic.byte_count());
    auto & slot = traffic_[
      TrafficKey{s.src_participant_prefix, static_cast<int>(dst.kind), dst.address, dst.port}];
    if (slot.samples == 0) {   // TRANSIENT_LOCAL: the first sample is the value before we started
      s.packets_first = s.packets;
      s.bytes_first = s.bytes;
    } else {
      s.packets_first = slot.packets_first;
      s.bytes_first = slot.bytes_first;
    }
    s.samples = slot.samples + 1;
    slot = s;
  }

  st::EntityData throughput;
  while (retcode_ok(throughput_.reader->take_next_sample(&throughput, &info))) {
    if (!info.valid_data) {continue;}
    count_sample();
    rtps::GUID_t g = to_rtps(throughput.guid());
    auto & t = data_.throughput[guid_to_string(g)];
    t.sum += throughput.data();
    t.last = throughput.data();
    ++t.samples;
  }

  st::WriterReaderData latency;
  while (retcode_ok(history_latency_.reader->take_next_sample(&latency, &info))) {
    if (!info.valid_data) {continue;}
    count_sample();
    rtps::GUID_t w = to_rtps(latency.writer_guid());   // remote writer
    rtps::GUID_t r = to_rtps(latency.reader_guid());   // the publisher's reader
    data_.delivered[{guid_to_string(w), guid_to_string(r)}]++;
    // write-to-notification latency, nanoseconds as float
    data_.latency[{guid_to_string(w), guid_to_string(r)}].add(
      static_cast<double>(latency.data()) * 1e-9);
  }

  // Cumulative per-entity counters: DATA_COUNT and the reliability counters. The first
  // sample (TRANSIENT_LOCAL) is the value before we started.
  auto drain_counter = [&](Reader & reader, std::map<std::string, DataCountSample> & into) {
      st::EntityCount count;
      while (retcode_ok(reader.reader->take_next_sample(&count, &info))) {
        if (!info.valid_data) {continue;}
        count_sample();
        rtps::GUID_t g = to_rtps(count.guid());
        auto & d = into[guid_to_string(g)];
        if (d.samples == 0) {d.first = count.count();}
        d.last = count.count();
        ++d.samples;
      }
    };
  drain_counter(data_count_, data_.data_count);
  drain_counter(resent_datas_, data_.resent_datas);
  drain_counter(heartbeat_count_, data_.heartbeats);
  drain_counter(gap_count_, data_.gaps);
  drain_counter(acknack_count_, data_.acknacks);
  drain_counter(nackfrag_count_, data_.nackfrags);

  // RTPS_LOST: the receiving participant publishes the packets it missed (sequence-number
  // gaps) from a remote sender (src_guid) addressed to one of its own locators (dst_locator).
  // The payload does not name the receiver: it is the sample's publisher.
  st::Entity2LocatorTraffic lost;
  while (retcode_ok(rtps_lost_.reader->take_next_sample(&lost, &info))) {
    if (!info.valid_data) {continue;}
    count_sample();
    rtps::GUID_t src = to_rtps(lost.src_guid());
    Locator dst = convert_locator(to_rtps(lost.dst_locator()));
    TrafficSample s;
    s.reporter_participant_prefix = sample_publisher_prefix(info);
    s.src_participant_prefix = prefix_to_string(src.guidPrefix);
    s.dst = dst;
    s.packets = lost.packet_count();
    s.bytes = static_cast<double>(lost.byte_count());
    auto & slot = lost_[LostKey{s.reporter_participant_prefix, s.src_participant_prefix,
          static_cast<int>(dst.kind), dst.address, dst.port}];
    if (slot.samples == 0) {
      s.packets_first = s.packets;
      s.bytes_first = s.bytes;
    } else {
      s.packets_first = slot.packets_first;
      s.bytes_first = slot.bytes_first;
    }
    s.samples = slot.samples + 1;
    slot = s;
  }

  st::PhysicalData physical;
  while (retcode_ok(physical_data_.reader->take_next_sample(&physical, &info))) {
    if (!info.valid_data) {continue;}
    count_sample();
    rtps::GUID_t g = to_rtps(physical.participant_guid());
    std::string prefix = prefix_to_string(g.guidPrefix);
    data_.physical[prefix] = HostInfo{physical.host(), physical.user(), physical.process()};
  }
}

void StatsObserver::poll()
{
  std::lock_guard<std::mutex> lock(mutex_);
  drain();
}

StatsData StatsObserver::snapshot()
{
  std::lock_guard<std::mutex> lock(mutex_);
  drain();
  StatsData out = data_;
  out.writer_instance_limit = FTV_STATS_WRITER_INSTANCE_LIMIT;
  out.samples_lost = listener_.lost;
  out.samples_lost_at_start = listener_.lost_at_start;
  out.samples_rejected = listener_.rejected;
  if (statistics_samples_were_lost(out)) {out.warnings.push_back("stats-samples-lost");}
  out.traffic.clear();
  for (const auto & kv : traffic_) {
    out.traffic.push_back(kv.second);
  }
  out.lost.clear();
  for (const auto & kv : lost_) {
    out.lost.push_back(kv.second);
  }
  return out;
}

}  // namespace fastdds_transport_viz
