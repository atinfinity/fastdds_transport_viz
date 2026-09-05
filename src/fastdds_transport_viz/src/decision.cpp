// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/decision.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fastdds_transport_viz
{

namespace
{

bool has_kind(const Endpoint & e, LocatorKind kind)
{
  auto pred = [kind](const Locator & l) {return l.kind == kind;};
  return std::any_of(e.unicast.begin(), e.unicast.end(), pred) ||
         std::any_of(e.multicast.begin(), e.multicast.end(), pred);
}

std::set<std::string> ip_addresses(const Endpoint & e)
{
  std::set<std::string> out;
  for (const auto & l : e.unicast) {
    if (l.kind == LocatorKind::UDPv4 || l.kind == LocatorKind::UDPv6 ||
      l.kind == LocatorKind::TCPv4 || l.kind == LocatorKind::TCPv6)
    {
      out.insert(l.address);
    }
  }
  return out;
}

Transport transport_for(LocatorKind kind)
{
  switch (kind) {
    case LocatorKind::UDPv4: return Transport::UDPv4;
    case LocatorKind::UDPv6: return Transport::UDPv6;
    case LocatorKind::TCPv4: return Transport::TCPv4;
    case LocatorKind::TCPv6: return Transport::TCPv6;
    case LocatorKind::SHM: return Transport::SHM;
    default: return Transport::None;
  }
}

std::string reason_for(LocatorKind kind)
{
  switch (kind) {
    case LocatorKind::UDPv4: return "common-udpv4-locator";
    case LocatorKind::UDPv6: return "common-udpv6-locator";
    case LocatorKind::TCPv4: return "common-tcpv4-locator";
    case LocatorKind::TCPv6: return "common-tcpv6-locator";
    default: return "no-common-transport";
  }
}

/// Pick the network (non-SHM) locator kind the writer will use to reach the
/// reader: walk the reader's unicast locators in announced order, then its
/// multicast locators, and take the first kind the writer can also speak.
bool pick_network_kind(const Endpoint & writer, const Endpoint & reader, LocatorKind & out)
{
  static const LocatorKind network_kinds[] = {
    LocatorKind::UDPv4, LocatorKind::UDPv6, LocatorKind::TCPv4, LocatorKind::TCPv6};
  for (const auto * list : {&reader.unicast, &reader.multicast}) {
    for (const auto & l : *list) {
      bool is_network = std::find(std::begin(network_kinds), std::end(network_kinds), l.kind) !=
        std::end(network_kinds);
      if (is_network && has_kind(writer, l.kind)) {
        out = l.kind;
        return true;
      }
    }
  }
  return false;
}

bool domains_intersect(const std::vector<uint64_t> & a, const std::vector<uint64_t> & b)
{
  for (auto x : a) {
    if (std::find(b.begin(), b.end(), x) != b.end()) {
      return true;
    }
  }
  return false;
}

}  // namespace

Verdict decide(const Endpoint & writer, const Endpoint & reader)
{
  Verdict v;
  const bool same_host = writer.host_id == reader.host_id;
  const bool w_shm = has_kind(writer, LocatorKind::SHM);
  const bool r_shm = has_kind(reader, LocatorKind::SHM);

  if (same_host) {
    v.reasons.push_back("same-host-guid");

    const auto wip = ip_addresses(writer);
    const auto rip = ip_addresses(reader);
    if (!wip.empty() && !rip.empty()) {
      bool common = std::any_of(wip.begin(), wip.end(), [&](const std::string & a) {
            return rip.count(a) > 0;
          });
      if (!common) {
        v.warnings.push_back("host-id-match-but-ip-differs");
      }
    }

    // Data-sharing delivery (zero-copy) bypasses every transport. Fast DDS
    // announces the *effective* data-sharing configuration (AUTO is resolved
    // against type boundedness / memory policy before discovery), so both
    // sides announcing non-OFF is strong evidence.
    const auto wd = writer.qos.data_sharing;
    const auto rd = reader.qos.data_sharing;
    if (wd == DataSharingKind::Off) {
      v.reasons.push_back("datasharing-disabled-writer");
    } else if (rd == DataSharingKind::Off) {
      v.reasons.push_back("datasharing-disabled-reader");
    } else if (wd == DataSharingKind::Unknown || rd == DataSharingKind::Unknown) {
      v.reasons.push_back("datasharing-qos-unknown");
    } else {
      const bool have_domains = !writer.qos.data_sharing_domains.empty() &&
        !reader.qos.data_sharing_domains.empty();
      if (have_domains &&
        !domains_intersect(writer.qos.data_sharing_domains, reader.qos.data_sharing_domains))
      {
        v.reasons.push_back("datasharing-domain-ids-mismatch");
      } else {
        v.transport = Transport::DataSharing;
        v.confidence = Confidence::Likely;
        v.reasons.push_back("datasharing-qos-enabled-both");
        v.reasons.push_back(
          have_domains ? "datasharing-domain-ids-match" : "datasharing-domain-ids-unknown");
        v.reasons.push_back("datasharing-unverified-by-traffic");
        return v;
      }
    }

    if (w_shm && r_shm) {
      v.transport = Transport::SHM;
      v.reasons.push_back("both-shm-locators");
      return v;
    }
    if (!w_shm) {
      v.reasons.push_back("writer-no-shm-locator");
    }
    if (!r_shm) {
      v.reasons.push_back("reader-no-shm-locator");
    }
  } else {
    v.reasons.push_back("different-host");
    if (w_shm && r_shm) {
      v.reasons.push_back("shm-locators-ignored-across-hosts");
    }
  }

  LocatorKind kind;
  if (pick_network_kind(writer, reader, kind)) {
    v.transport = transport_for(kind);
    v.reasons.push_back(reason_for(kind));
  } else {
    v.transport = Transport::None;
    v.reasons.push_back("no-common-transport");
  }
  return v;
}

std::vector<TopicSummary> summarize(const std::vector<Endpoint> & endpoints)
{
  std::map<std::string, TopicSummary> by_topic;
  for (const auto & e : endpoints) {
    auto & t = by_topic[e.dds_topic];
    if (t.dds_topic.empty()) {
      t.dds_topic = e.dds_topic;
      t.is_ros_topic = !e.ros_topic.empty();
      t.display_topic = t.is_ros_topic ? e.ros_topic : e.dds_topic;
      t.display_type = !e.ros_type.empty() ? e.ros_type : e.dds_type;
    }
    (e.is_writer ? t.writers : t.readers).push_back(&e);
  }

  std::vector<TopicSummary> out;
  out.reserve(by_topic.size());
  for (auto & kv : by_topic) {
    auto & t = kv.second;
    bool type_mismatch = false;
    for (const auto * w : t.writers) {
      for (const auto * r : t.readers) {
        if (w->dds_type != r->dds_type) {
          type_mismatch = true;
          continue;
        }
        Pair p;
        p.writer = w;
        p.reader = r;
        p.verdict = decide(*w, *r);
        t.pairs.push_back(p);
      }
    }
    if (t.writers.empty()) {
      t.unmatched_reasons.push_back("no-matching-writer");
    }
    if (t.readers.empty()) {
      t.unmatched_reasons.push_back("no-matching-reader");
    }
    if (type_mismatch) {
      t.unmatched_reasons.push_back("type-name-mismatch");
    }
    out.push_back(std::move(t));
  }
  std::sort(out.begin(), out.end(), [](const TopicSummary & a, const TopicSummary & b) {
      return a.display_topic < b.display_topic;
    });
  return out;
}

void filter_by_node(
  std::vector<TopicSummary> & topics,
  const std::function<bool(const Endpoint &)> & node_matches)
{
  std::vector<TopicSummary> out;
  out.reserve(topics.size());
  for (auto & t : topics) {
    std::set<const Endpoint *> matching;     // endpoints of matching nodes
    for (const auto * e : t.writers) {
      if (node_matches(*e)) {matching.insert(e);}
    }
    for (const auto * e : t.readers) {
      if (node_matches(*e)) {matching.insert(e);}
    }
    std::set<const Endpoint *> keep = matching;   // + partners of the kept pairs
    std::vector<Pair> pairs;
    for (const auto & p : t.pairs) {
      if (matching.count(p.writer) || matching.count(p.reader)) {
        keep.insert(p.writer);
        keep.insert(p.reader);
        pairs.push_back(p);
      }
    }
    if (keep.empty()) {
      continue;
    }
    auto prune = [&keep](std::vector<const Endpoint *> & v) {
        v.erase(
          std::remove_if(v.begin(), v.end(), [&keep](const Endpoint * e) {return !keep.count(e);}),
          v.end());
      };
    prune(t.writers);
    prune(t.readers);
    t.pairs = std::move(pairs);
    const bool type_mismatch = std::find(
      t.unmatched_reasons.begin(), t.unmatched_reasons.end(), "type-name-mismatch") !=
      t.unmatched_reasons.end();
    t.unmatched_reasons.clear();
    if (t.writers.empty()) {
      t.unmatched_reasons.push_back("no-matching-writer");
    }
    if (t.readers.empty()) {
      t.unmatched_reasons.push_back("no-matching-reader");
    }
    if (type_mismatch && t.pairs.empty()) {
      t.unmatched_reasons.push_back("type-name-mismatch");
    }
    out.push_back(std::move(t));
  }
  topics = std::move(out);
}

namespace
{

bool same_locator(const Locator & a, const Locator & b)
{
  return a.kind == b.kind && a.port == b.port && (a.kind == LocatorKind::SHM || a.address == b.address);
}

bool reader_has_locator(const Endpoint & reader, const Locator & l)
{
  for (const auto * list : {&reader.unicast, &reader.multicast}) {
    for (const auto & rl : *list) {
      if (same_locator(rl, l)) {return true;}
    }
  }
  return false;
}

Transport transport_for_kind(LocatorKind kind)
{
  switch (kind) {
    case LocatorKind::UDPv4: return Transport::UDPv4;
    case LocatorKind::UDPv6: return Transport::UDPv6;
    case LocatorKind::TCPv4: return Transport::TCPv4;
    case LocatorKind::TCPv6: return Transport::TCPv6;
    case LocatorKind::SHM: return Transport::SHM;
    default: return Transport::None;
  }
}

std::string measured_reason(Transport t)
{
  switch (t) {
    case Transport::UDPv4: return "measured-udpv4-traffic";
    case Transport::UDPv6: return "measured-udpv6-traffic";
    case Transport::TCPv4: return "measured-tcpv4-traffic";
    case Transport::TCPv6: return "measured-tcpv6-traffic";
    case Transport::SHM: return "measured-shm-traffic";
    default: return "measured-unknown-traffic";
  }
}

constexpr size_t kStatsWriterInstanceLimit = 10;   // Fast DDS default resource_limits.max_instances

void replace_code(std::vector<std::string> & codes, const std::string & from, const std::string & to)
{
  for (auto & c : codes) {
    if (c == from) {c = to; return;}
  }
  codes.push_back(to);
}

}  // namespace

void apply_stats(std::vector<TopicSummary> & topics, const StatsData & stats)
{
  if (!stats.enabled) {
    return;
  }
  // Distinct destination locators reported per source participant. The Fast DDS
  // statistics DataWriter keeps the default resource limit of 10 instances, so a
  // participant talking to more than 10 locators silently stops reporting new
  // ones - exactly 10 reported locators plus a missing one is the signature.
  std::map<std::string, size_t> locators_per_source;
  for (const auto & s : stats.traffic) {
    locators_per_source[s.src_participant_prefix]++;
  }
  for (auto & t : topics) {
    t.throughput = 0.0;
    t.throughput_available = false;
    for (const auto * w : t.writers) {
      if (auto th = stats.throughput.find(w->guid); th != stats.throughput.end()) {
        t.throughput += th->second.mean();
        t.throughput_available = true;
      }
    }
    for (auto & p : t.pairs) {
      Measurement & m = p.measured;
      const std::string & src = p.writer->participant_guid_prefix;
      m.available = stats.participants_with_stats.count(src) > 0;
      if (auto th = stats.throughput.find(p.writer->guid); th != stats.throughput.end()) {
        m.throughput_available = true;
        m.throughput = th->second.mean();
      }
      if (auto d = stats.delivered.find({p.writer->guid, p.reader->guid}); d != stats.delivered.end()) {
        m.delivered_samples = d->second;
        m.delivered = d->second > 0;
      }
      // DATA_COUNT is published only when the writer actually sends a DATA submessage,
      // so "the participant has a DATA_COUNT writer but no sample arrived" means zero.
      if (auto dc = stats.data_count.find(p.writer->guid); dc != stats.data_count.end()) {
        m.data_count_available = true;
        m.data_submessages = dc->second.last - dc->second.first;
      } else if (stats.statistics_writers.count({src, kStatsDataCountTopic})) {
        m.data_count_available = true;
      }
      for (const auto & s : stats.traffic) {
        if (s.src_participant_prefix != src || !reader_has_locator(*p.reader, s.dst)) {
          continue;
        }
        if (s.packets == 0) {
          continue;
        }
        Transport tr = transport_for_kind(s.dst.kind);
        if (std::find(m.transports.begin(), m.transports.end(), tr) == m.transports.end()) {
          m.transports.push_back(tr);
        }
        // counters are cumulative since the writer's participant started; the
        // difference to the first sample is what happened during the observation
        m.packets += s.packets - std::min(s.packets, s.packets_first);
        m.bytes += std::max(0.0, s.bytes - s.bytes_first);
        m.packets_total += s.packets;
        m.bytes_total += s.bytes;
      }

      Verdict & v = p.verdict;
      if (!m.available) {
        v.warnings.push_back("stats-not-enabled-on-writer");
        continue;
      }
      if (v.transport == Transport::DataSharing) {
        // Zero-copy delivery leaves no RTPS trace. Delivery confirmed by
        // HISTORY_LATENCY plus silence on every locator of the reader => certain.
        if (m.delivered && m.transports.empty()) {
          v.confidence = Confidence::Certain;
          replace_code(v.reasons, "datasharing-unverified-by-traffic", "datasharing-confirmed-no-traffic");
        } else if (m.delivered && m.data_count_available) {
          // Reliable data-sharing endpoints still exchange heartbeats, so traffic on the
          // link proves nothing; the writer's DATA_COUNT (DATA submessages sent through a
          // transport) does, as long as every reader of the writer uses data-sharing.
          const bool all_readers_datasharing = std::all_of(
            t.pairs.begin(), t.pairs.end(), [&p](const Pair & q) {
              return q.writer != p.writer || q.verdict.transport == Transport::DataSharing;
            });
          if (!all_readers_datasharing) {
            replace_code(v.reasons, "datasharing-unverified-by-traffic", "datasharing-ambiguous-mixed-readers");
          } else if (m.data_submessages == 0) {
            v.confidence = Confidence::Certain;
            replace_code(v.reasons, "datasharing-unverified-by-traffic", "datasharing-confirmed-no-data-submessages");
          } else {
            // DATA left through a transport although only data-sharing readers exist:
            // Fast DDS did not use zero-copy. Report what was measured.
            v.transport = m.transports.empty() ? Transport::SHM : m.transports.front();
            v.confidence = m.transports.empty() ? Confidence::Likely : Confidence::Certain;
            replace_code(v.reasons, "datasharing-unverified-by-traffic", "datasharing-data-submessages-sent");
            for (auto tr : m.transports) {
              v.reasons.push_back(measured_reason(tr));
            }
            v.warnings.push_back("datasharing-not-used");
          }
        } else if (!m.transports.empty()) {
          replace_code(v.reasons, "datasharing-unverified-by-traffic", "datasharing-ambiguous-participant-traffic");
        } else {
          replace_code(v.reasons, "datasharing-unverified-by-traffic", "datasharing-no-delivery-observed");
        }
        continue;
      }
      if (m.transports.empty()) {
        // Samples proven delivered (HISTORY_LATENCY) but no RTPS_SENT entry for any of the
        // reader's locators: the statistics did not attribute the packets, which is not
        // the same as an idle link.
        v.warnings.push_back(
          locators_per_source[src] >= kStatsWriterInstanceLimit ?
          "stats-writer-instance-limit-suspected" :
          m.delivered ? "delivered-without-measured-traffic" : "no-traffic-observed");
        continue;
      }
      bool matches = std::find(m.transports.begin(), m.transports.end(), v.transport) != m.transports.end();
      for (auto tr : m.transports) {
        v.reasons.push_back(measured_reason(tr));
      }
      if (matches) {
        v.confidence = Confidence::Certain;
      } else {
        v.warnings.push_back("measured-transport-mismatch");
      }
    }
  }
}

PairKey pair_key(const TopicSummary & topic, const Pair & pair)
{
  return PairKey{topic.display_topic, pair.writer->guid, pair.reader->guid};
}

PairState pair_state(const Pair & pair)
{
  PairState s;
  s.transport = pair.verdict.transport;
  s.confidence = pair.verdict.confidence;
  s.measured = pair.measured.transports;
  s.warnings = pair.verdict.warnings;
  return s;
}

std::map<PairKey, PairState> pair_states(const Snapshot & snap)
{
  std::map<PairKey, PairState> out;
  for (const auto & t : snap.topics) {
    for (const auto & p : t.pairs) {
      out[pair_key(t, p)] = pair_state(p);
    }
  }
  return out;
}

Changes diff(const std::map<PairKey, PairState> & previous, const std::map<PairKey, PairState> & current)
{
  Changes c;
  for (const auto & kv : current) {
    auto it = previous.find(kv.first);
    if (it == previous.end()) {
      c.added.push_back(kv.first);
    } else if (it->second != kv.second) {
      c.changed.push_back(PairChange{kv.first, it->second, kv.second});
    }
  }
  for (const auto & kv : previous) {
    if (!current.count(kv.first)) {
      c.removed.push_back(kv.first);
    }
  }
  return c;
}

namespace
{
const std::map<std::string, std::string> & explanations()
{
  static const std::map<std::string, std::string> m = {
    {"same-host-guid",
      "Writer and reader GUID prefixes share the same first 4 bytes, which is how Fast DDS "
      "decides both participants run on the same host."},
    {"different-host",
      "GUID prefixes differ in the first 4 bytes, so Fast DDS treats the endpoints as being on "
      "different hosts; shared memory is not an option."},
    {"both-shm-locators",
      "Both endpoints announce a SHM locator. On the same host Fast DDS then uses the shared "
      "memory transport exclusively for user data (discovery still goes over UDP)."},
    {"writer-no-shm-locator",
      "The writer's participant announces no SHM locator: SHM transport is not instantiated on "
      "its side (e.g. FASTDDS_BUILTIN_TRANSPORTS=UDPv4, or an XML profile without SHM)."},
    {"reader-no-shm-locator",
      "The reader's participant announces no SHM locator: SHM transport is not instantiated on "
      "its side (e.g. FASTDDS_BUILTIN_TRANSPORTS=UDPv4, or an XML profile without SHM)."},
    {"shm-locators-ignored-across-hosts",
      "Both endpoints announce SHM locators, but they are on different hosts so the SHM "
      "locators are discarded."},
    {"common-udpv4-locator", "The reader announces a UDPv4 locator and the writer speaks UDPv4."},
    {"common-udpv6-locator", "The reader announces a UDPv6 locator and the writer speaks UDPv6."},
    {"common-tcpv4-locator", "The reader announces a TCPv4 locator and the writer speaks TCPv4."},
    {"common-tcpv6-locator", "The reader announces a TCPv6 locator and the writer speaks TCPv6."},
    {"no-common-transport",
      "No locator kind is shared by both endpoints; user data cannot flow between them."},
    {"datasharing-disabled-writer",
      "The writer announces data-sharing OFF (explicitly disabled, or AUTO resolved to OFF "
      "because the type is unbounded / the history memory policy is not preallocated)."},
    {"datasharing-disabled-reader",
      "The reader announces data-sharing OFF (explicitly disabled, or AUTO resolved to OFF "
      "because the type is unbounded / the history memory policy is not preallocated)."},
    {"datasharing-qos-unknown",
      "The data-sharing QoS of at least one endpoint could not be read from discovery data."},
    {"datasharing-qos-enabled-both",
      "Both endpoints announce data-sharing ON or AUTO. Fast DDS resolves AUTO before "
      "discovery, so this means both sides consider themselves data-sharing capable."},
    {"datasharing-domain-ids-match",
      "The data-sharing domain ids announced by writer and reader intersect, which is required "
      "for zero-copy delivery to be matched."},
    {"datasharing-domain-ids-unknown",
      "At least one side announced no data-sharing domain id; matching cannot be confirmed "
      "from discovery data alone."},
    {"datasharing-domain-ids-mismatch",
      "Both sides are data-sharing capable but their domain ids do not intersect, so Fast DDS "
      "falls back to the transports."},
    {"datasharing-unverified-by-traffic",
      "Zero-copy delivery produces no RTPS traffic; run with --stats to confirm that no user "
      "data is sent over a transport for this pair."},
    {"no-matching-writer", "No publisher was discovered for this topic."},
    {"no-matching-reader", "No subscription was discovered for this topic."},
    {"type-name-mismatch",
      "A writer and a reader on this topic announce different type names, so they do not match."},
    {"measured-udpv4-traffic", "Statistics show RTPS packets from the writer's participant to the reader's UDPv4 locator."},
    {"measured-udpv6-traffic", "Statistics show RTPS packets from the writer's participant to the reader's UDPv6 locator."},
    {"measured-tcpv4-traffic", "Statistics show RTPS packets from the writer's participant to the reader's TCPv4 locator."},
    {"measured-tcpv6-traffic", "Statistics show RTPS packets from the writer's participant to the reader's TCPv6 locator."},
    {"measured-shm-traffic", "Statistics show RTPS packets from the writer's participant to the reader's SHM locator."},
    {"measured-unknown-traffic", "Statistics show RTPS packets to a locator of unknown kind."},
    {"measured-transport-mismatch",
      "The transport predicted from discovery data differs from the locator kind(s) that actually "
      "carried packets. Please report this with the --json output."},
    {"stats-writer-instance-limit-suspected",
      "The writer's participant reports traffic to 10 or more locators but none to this reader. "
      "The Fast DDS statistics DataWriter keeps the default resource limit of 10 instances "
      "(one per destination locator), so counters for further locators are never published. "
      "Raise it with a data_writer XML profile named after the alias used in FASTDDS_STATISTICS "
      "(RTPS_SENT_TOPIC) whose <resourceLimitsQos> sets max_instances to 0; the package ships "
      "config/statistics.xml for this."},
    {"delivered-without-measured-traffic",
      "HISTORY_LATENCY statistics prove that samples reached the reader, but RTPS_SENT reported no "
      "packets to any of the reader's locators during the observation, so the transport that "
      "carried them could not be measured (seen with large samples over SHM on slow machines)."},
    {"no-traffic-observed",
      "The writer's participant publishes statistics but sent no packets to any locator of the "
      "reader during the observation window (idle topic, or a longer --timeout is needed)."},
    {"stats-not-enabled-on-writer",
      "No statistics were received from the writer's participant. Start it with "
      "FASTDDS_STATISTICS=\"RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC\"."},
    {"datasharing-confirmed-no-traffic",
      "HISTORY_LATENCY statistics prove samples reached the reader while no RTPS packets went to "
      "any of its locators: zero-copy data-sharing delivery is confirmed."},
    {"datasharing-confirmed-no-data-submessages",
      "HISTORY_LATENCY statistics prove samples reached the reader while the writer's DATA_COUNT "
      "did not grow: no DATA submessage left through a transport, so zero-copy data-sharing "
      "delivery is confirmed (needs DATA_COUNT_TOPIC on the observed nodes)."},
    {"datasharing-ambiguous-mixed-readers",
      "The writer also serves readers without data-sharing, so its DATA_COUNT mixes both delivery "
      "paths and cannot confirm zero-copy for this pair."},
    {"datasharing-data-submessages-sent",
      "Every reader of this writer announces data-sharing, yet the writer sent DATA submessages "
      "through a transport during the observation: Fast DDS did not use zero-copy delivery."},
    {"datasharing-not-used",
      "Data-sharing was announced by both sides but the writer's DATA_COUNT grew while it only had "
      "data-sharing readers; the verdict shows the transport that was measured instead."},
    {"datasharing-ambiguous-participant-traffic",
      "The writer's participant did send packets to the reader's locators. Statistics are per "
      "participant, and reliable data-sharing endpoints still exchange heartbeats/acknacks over "
      "the transport, so this does not disprove zero-copy delivery; it just cannot confirm it."},
    {"datasharing-no-delivery-observed",
      "No HISTORY_LATENCY sample for this pair yet (needs HISTORY_LATENCY_TOPIC enabled and at "
      "least one published sample), so data-sharing remains unconfirmed."},
    {"shm-stale-files",
      "Fast DDS files in the shared-memory directory whose lock nobody holds: their owner "
      "process ended without cleaning up (a crash or a kill). They keep consuming /dev/shm; "
      "remove them with 'fastdds shm clean'."},
    {"shm-nearly-full",
      "The shared-memory directory is at least 90% full or has less than 16 MB free. Fast DDS "
      "cannot create its segment when /dev/shm is full, so participants fail to start or fall "
      "back to UDP (Docker's default /dev/shm is only 64 MB: use --shm-size or --ipc=host)."},
    {"shm-not-visible",
      "Observed nodes announce SHM locators that are not open in this process's shared-memory "
      "directory (nobody holds the port's lock, the port number collides with the tool's own, "
      "or the node has another host id): they run in another IPC namespace or on another host, "
      "so the shared-memory figures describe this environment, not theirs, and SHM cannot be "
      "used between them and here."},
    {"host-id-match-but-ip-differs",
      "The endpoints share a host id but announce no common IP address (typical for containers "
      "with separate network namespaces on one machine). SHM works only if /dev/shm is shared."},
  };
  return m;
}
}  // namespace

std::string explain(const std::string & code)
{
  const auto & m = explanations();
  auto it = m.find(code);
  return it == m.end() ? std::string("(no description)") : it->second;
}

std::vector<std::string> known_codes()
{
  std::vector<std::string> out;
  for (const auto & kv : explanations()) {
    out.push_back(kv.first);
  }
  return out;
}

}  // namespace fastdds_transport_viz
