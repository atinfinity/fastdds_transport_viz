// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/decision.hpp"

#include <fnmatch.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
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

/// The endpoint's first announced locator of `kind`, or nullptr. Unicast before
/// multicast, matching the order Fast DDS walks them in.
const Locator * find_locator(const Endpoint & e, LocatorKind kind)
{
  for (const auto * list : {&e.unicast, &e.multicast}) {
    for (const auto & l : *list) {
      if (l.kind == kind) {return &l;}
    }
  }
  return nullptr;
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

/// Pick the network (non-SHM) locator the writer will use to reach the reader: walk the
/// reader's unicast locators in announced order, then its multicast locators, and take
/// the first one whose kind the writer can also speak.
bool pick_network_locator(
  const Endpoint & writer, const Endpoint & reader, Locator & out, bool & multicast)
{
  static const LocatorKind network_kinds[] = {
    LocatorKind::UDPv4, LocatorKind::UDPv6, LocatorKind::TCPv4, LocatorKind::TCPv6};
  bool in_multicast = false;
  for (const auto * list : {&reader.unicast, &reader.multicast}) {
    for (const auto & l : *list) {
      bool is_network = std::find(std::begin(network_kinds), std::end(network_kinds), l.kind) !=
        std::end(network_kinds);
      if (is_network && has_kind(writer, l.kind)) {
        out = l;
        multicast = in_multicast;
        return true;
      }
    }
    in_multicast = true;
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

namespace
{
int durability_rank(const std::string & d)
{
  if (d == "VOLATILE") {return 0;}
  if (d == "TRANSIENT_LOCAL") {return 1;}
  if (d == "TRANSIENT") {return 2;}
  if (d == "PERSISTENT") {return 3;}
  return -1;   // unknown: do not judge
}

int liveliness_rank(const std::string & l)
{
  if (l == "AUTOMATIC") {return 0;}
  if (l == "MANUAL_BY_PARTICIPANT") {return 1;}
  if (l == "MANUAL_BY_TOPIC") {return 2;}
  return -1;
}

/// DDS partition matching: a name may be a pattern ('*', '?', '[...]' as in fnmatch) and
/// matches the other side's plain name; an empty list is the default partition "".
bool partitions_match(const std::vector<std::string> & w, const std::vector<std::string> & r)
{
  const std::vector<std::string> empty{""};
  const auto & ws = w.empty() ? empty : w;
  const auto & rs = r.empty() ? empty : r;
  for (const auto & a : ws) {
    for (const auto & b : rs) {
      if (a == b || ::fnmatch(a.c_str(), b.c_str(), 0) == 0 ||
        ::fnmatch(b.c_str(), a.c_str(), 0) == 0)
      {
        return true;
      }
    }
  }
  return false;
}
}  // namespace

std::vector<std::string> qos_incompatibilities(const Endpoint & writer, const Endpoint & reader)
{
  std::vector<std::string> out;
  const auto & w = writer.qos;
  const auto & r = reader.qos;
  if (w.reliability == "BEST_EFFORT" && r.reliability == "RELIABLE") {
    out.push_back("reliability");
  }
  if (durability_rank(w.durability) >= 0 && durability_rank(r.durability) >= 0 &&
    durability_rank(w.durability) < durability_rank(r.durability))
  {
    out.push_back("durability");
  }
  // the writer must promise at least the requested rate
  if (w.deadline_s > r.deadline_s) {
    out.push_back("deadline");
  }
  if ((liveliness_rank(w.liveliness) >= 0 && liveliness_rank(r.liveliness) >= 0 &&
    liveliness_rank(w.liveliness) < liveliness_rank(r.liveliness)) ||
    w.liveliness_lease_s > r.liveliness_lease_s)
  {
    out.push_back("liveliness");
  }
  if (w.ownership != r.ownership) {
    out.push_back("ownership");
  }
  if (!partitions_match(w.partitions, r.partitions)) {
    out.push_back("partition");
  }
  return out;
}

namespace
{

std::set<uint32_t> shm_ports(const Endpoint & e)
{
  std::set<uint32_t> out = e.participant_shm_ports;
  for (const auto & l : e.unicast) {
    if (l.kind == LocatorKind::SHM) {out.insert(l.port);}
  }
  return out;
}

/// Evidence that two same-host participants listen for SHM in different IPC namespaces.
/// A unicast SHM port is listened on by one participant per namespace, so the same number
/// announced by both proves it from any of the host's IPC namespaces; otherwise one side
/// visible from the tool's namespace and the other not does.
std::vector<std::string> ipc_split_reasons(const Endpoint & writer, const Endpoint & reader)
{
  std::vector<std::string> out;
  if (writer.participant_guid_prefix == reader.participant_guid_prefix) {
    return out;
  }
  const auto wp = shm_ports(writer);
  const auto rp = shm_ports(reader);
  if (std::any_of(wp.begin(), wp.end(), [&rp](uint32_t p) {return rp.count(p) > 0;})) {
    out.push_back("shm-port-collision");
  }
  const auto wv = writer.participant_shm_visibility;
  const auto rv = reader.participant_shm_visibility;
  if (wv == ShmVisibility::Visible && rv == ShmVisibility::NotVisible) {
    out.push_back("shm-reader-port-not-visible");
  } else if (wv == ShmVisibility::NotVisible && rv == ShmVisibility::Visible) {
    out.push_back("shm-writer-port-not-visible");
  }
  return out;
}

/// Evidence that two data-sharing endpoints use different /dev/shm: the writer's history
/// and the reader's notification segment are both created by their endpoints, so one in
/// the tool's /dev/shm and the other not means two IPC namespaces.
std::vector<std::string> datasharing_split_reasons(
  const Endpoint & writer,
  const Endpoint & reader)
{
  std::vector<std::string> out;
  if (writer.participant_guid_prefix == reader.participant_guid_prefix) {
    return out;
  }
  const auto wv = writer.datasharing_segment_visibility;
  const auto rv = reader.datasharing_segment_visibility;
  if (wv == ShmVisibility::Visible && rv == ShmVisibility::NotVisible) {
    out.push_back("datasharing-reader-segment-not-visible");
  } else if (wv == ShmVisibility::NotVisible && rv == ShmVisibility::Visible) {
    out.push_back("datasharing-writer-segment-not-visible");
  }
  return out;
}

}  // namespace

Verdict decide(const Endpoint & writer, const Endpoint & reader)
{
  Verdict v;
  const auto incompatible = qos_incompatibilities(writer, reader);
  if (!incompatible.empty()) {
    // Fast DDS does not match these endpoints at all: no transport carries user data.
    v.transport = Transport::None;
    v.confidence = Confidence::Certain;
    for (const auto & policy : incompatible) {
      v.reasons.push_back("qos-incompatible-" + policy);
    }
    v.warnings.push_back("qos-incompatible");
    return v;
  }
  const bool same_host = writer.host_id == reader.host_id;
  const bool w_shm = has_kind(writer, LocatorKind::SHM);
  const bool r_shm = has_kind(reader, LocatorKind::SHM);

  if (same_host) {
    v.reasons.push_back("same-host-guid");

    const auto wip = ip_addresses(writer);
    const auto rip = ip_addresses(reader);
    if (!wip.empty() && !rip.empty()) {
      bool common = std::any_of(
        wip.begin(), wip.end(), [&](const std::string & a) {
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
        v.reasons.push_back("datasharing-qos-enabled-both");
        v.reasons.push_back(
          have_domains ? "datasharing-domain-ids-match" : "datasharing-domain-ids-unknown");
        // Fast DDS pairs data-sharing endpoints on QoS alone. In two IPC namespaces the
        // reader cannot open the writer's history and rejects the writer, and the writer
        // sends it nothing through a transport: every sample is lost.
        const bool both_shm = w_shm && r_shm;
        auto split = both_shm ? ipc_split_reasons(writer, reader) : std::vector<std::string>{};
        const auto ds_split = datasharing_split_reasons(writer, reader);
        split.insert(split.end(), ds_split.begin(), ds_split.end());
        if (!split.empty()) {
          v.transport = Transport::None;
          v.confidence = Confidence::Certain;
          if (both_shm) {v.reasons.push_back("both-shm-locators");}
          v.reasons.insert(v.reasons.end(), split.begin(), split.end());
          v.warnings.push_back("shm-ipc-namespace-split");
          return v;
        }
        v.transport = Transport::DataSharing;
        v.confidence = Confidence::Likely;
        v.reasons.push_back("datasharing-unverified-by-traffic");
        return v;
      }
    }

    if (w_shm && r_shm) {
      v.reasons.push_back("both-shm-locators");
      const auto split = ipc_split_reasons(writer, reader);
      if (!split.empty()) {
        // Fast DDS still selects SHM, but the writer pushes into a port of its own
        // /dev/shm that nobody on the reader's side listens to: nothing arrives.
        v.transport = Transport::None;
        v.confidence = Confidence::Certain;
        v.reasons.insert(v.reasons.end(), split.begin(), split.end());
        v.warnings.push_back("shm-ipc-namespace-split");
        return v;
      }
      v.transport = Transport::SHM;
      // The reader's SHM locator names the /dev/shm port the writer will write into,
      // exactly as a network locator names the address it will send to.
      if (const Locator * l = find_locator(reader, LocatorKind::SHM)) {v.locator = *l;}
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

  Locator selected;
  bool selected_multicast = false;
  const bool same_host_locators_hidden =
    same_host && (writer.same_host_locators_filtered || reader.same_host_locators_filtered) &&
    (w_shm != r_shm);
  if (pick_network_locator(writer, reader, selected, selected_multicast)) {
    v.transport = transport_for(selected.kind);
    v.locator = selected;
    v.locator_multicast = selected_multicast;
    v.reasons.push_back(reason_for(selected.kind));
  } else if (same_host_locators_hidden) {
    // Fast DDS < 2.10 shows the tool only the SHM locator of a same-host participant; the
    // side without SHM is UDP-only, and the SHM side has the builtin UDP transport as
    // well, so Fast DDS falls back to UDPv4 between them.
    v.transport = Transport::UDPv4;
    v.confidence = Confidence::Likely;
    v.reasons.push_back("same-host-locators-hidden");
  } else {
    v.transport = Transport::None;
    v.reasons.push_back("no-common-transport");
  }
  return v;
}

namespace
{

bool is_buffer_companion_topic(const std::string & dds_topic)
{
  const std::string suffix = kBufferCompanionSuffix;
  return dds_topic.size() > suffix.size() &&
         dds_topic.compare(dds_topic.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/// The entity key of an endpoint GUID (bytes 12..14, big-endian; byte 15 is the kind).
uint32_t entity_key(const Endpoint & e)
{
  return (static_cast<uint32_t>(e.guid_bytes[12]) << 16) |
         (static_cast<uint32_t>(e.guid_bytes[13]) << 8) | e.guid_bytes[14];
}

/// The native-buffer codes of a pair's (or an unpaired topic's) endpoints.
std::vector<std::string> buffer_codes(const std::vector<const Endpoint *> & endpoints)
{
  bool folded = false, companion = false, unmatched = false;
  for (const auto * e : endpoints) {
    folded |= !e->buffer_companion_guids.empty();
    companion |= !e->buffer_parent_guid.empty();
    unmatched |= e->buffer_parent_guid.empty() && is_buffer_companion_topic(e->dds_topic);
  }
  std::vector<std::string> out;
  if (folded) {out.push_back("buffer-companion-folded");}
  if (companion) {out.push_back("buffer-companion");}
  if (unmatched) {out.push_back("buffer-companion-unmatched");}
  return out;
}

void add_buffer_codes(std::vector<std::string> & codes, const std::vector<const Endpoint *> & e)
{
  const auto more = buffer_codes(e);
  codes.insert(codes.end(), more.begin(), more.end());
}

}  // namespace

void link_buffer_companions(std::vector<Endpoint> & endpoints)
{
  for (auto & e : endpoints) {
    e.buffer_parent_guid.clear();
    e.buffer_companion_guids.clear();
  }
  const size_t suffix_size = std::string(kBufferCompanionSuffix).size();
  for (auto & c : endpoints) {
    if (!is_buffer_companion_topic(c.dds_topic)) {continue;}
    const std::string parent_topic = c.dds_topic.substr(0, c.dds_topic.size() - suffix_size);
    std::vector<Endpoint *> candidates;
    for (auto & p : endpoints) {
      if (&p != &c && p.is_writer == c.is_writer &&
        p.participant_guid_prefix == c.participant_guid_prefix && p.dds_type == c.dds_type &&
        p.dds_topic == parent_topic)
      {
        candidates.push_back(&p);
      }
    }
    Endpoint * parent = candidates.size() == 1 ? candidates.front() : nullptr;
    if (candidates.size() > 1) {
      // rmw_fastrtps creates the companion right after its parent under one mutex, and the
      // participant numbers writers and readers from one counter
      for (auto * p : candidates) {
        if (entity_key(*p) + 1 == entity_key(c) && p->guid_bytes[15] == c.guid_bytes[15]) {
          parent = p;
          break;
        }
      }
    }
    if (parent != nullptr) {
      c.buffer_parent_guid = parent->guid;
      parent->buffer_companion_guids.push_back(c.guid);
    }
  }
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
        add_buffer_codes(p.verdict.reasons, {w, r});
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
    if (t.pairs.empty()) {
      std::vector<const Endpoint *> all = t.writers;
      all.insert(all.end(), t.readers.begin(), t.readers.end());
      add_buffer_codes(t.unmatched_reasons, all);
    }
    out.push_back(std::move(t));
  }
  std::sort(
    out.begin(), out.end(), [](const TopicSummary & a, const TopicSummary & b) {
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
    if (t.pairs.empty()) {
      std::vector<const Endpoint *> all = t.writers;
      all.insert(all.end(), t.readers.begin(), t.readers.end());
      add_buffer_codes(t.unmatched_reasons, all);
    }
    out.push_back(std::move(t));
  }
  topics = std::move(out);
}

bool in_default_view(const TopicSummary & topic)
{
  if (!topic.is_ros_topic || topic.dds_topic.rfind("rt/", 0) != 0) {return false;}
  if (topic.writers.empty() && topic.readers.empty()) {return true;}
  const auto folded = [](const Endpoint * e) {return !e->buffer_parent_guid.empty();};
  return !(std::all_of(topic.writers.begin(), topic.writers.end(), folded) &&
         std::all_of(topic.readers.begin(), topic.readers.end(), folded));
}

namespace
{

bool same_locator(const Locator & a, const Locator & b)
{
  return a.kind == b.kind && a.port == b.port &&
         (a.kind == LocatorKind::SHM || a.address == b.address);
}

bool is_loopback(const std::string & address)
{
  return address == "127.0.0.1" || address == "::1";
}

/// same_locator() plus the localhost transformation: Fast DDS rewrites the locators of a
/// participant on the tool's host to 127.0.0.1 / ::1 in the discovery data the tool
/// receives, while a writer on another host reports its traffic to that participant's
/// real address. A loopback locator therefore also matches any address of this host.
bool same_locator_local(const Locator & a, const Locator & b, const std::set<std::string> & local)
{
  if (same_locator(a, b)) {return true;}
  if (a.kind != b.kind || a.port != b.port) {return false;}
  return (is_loopback(a.address) && local.count(b.address)) ||
         (is_loopback(b.address) && local.count(a.address));
}

bool reader_has_locator(
  const Endpoint & reader, const Locator & l, const std::set<std::string> & local)
{
  for (const auto * list : {&reader.unicast, &reader.multicast}) {
    for (const auto & rl : *list) {
      if (same_locator_local(rl, l, local)) {return true;}
    }
  }
  return false;
}

bool reader_has_unicast_locator(
  const Endpoint & reader, const Locator & l, const std::set<std::string> & local)
{
  return std::any_of(
    reader.unicast.begin(), reader.unicast.end(),
    [&](const Locator & rl) {return same_locator_local(rl, l, local);});
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

constexpr size_t kStatsWriterInstanceLimit = 10;   // Fast DDS < 3.5 default max_instances

void replace_code(
  std::vector<std::string> & codes, const std::string & from, const std::string & to)
{
  for (auto & c : codes) {
    if (c == from) {c = to; return;}
  }
  codes.push_back(to);
}

/// The GUID of an endpoint followed by those of its native-buffer companions.
std::vector<std::string> entity_guids(const Endpoint & e)
{
  std::vector<std::string> out{e.guid};
  out.insert(out.end(), e.buffer_companion_guids.begin(), e.buffer_companion_guids.end());
  return out;
}

bool is_datasharing_split(const Verdict & v)
{
  const auto has = [](const std::vector<std::string> & codes, const char * code) {
      return std::find(codes.begin(), codes.end(), code) != codes.end();
    };
  return has(v.warnings, "shm-ipc-namespace-split") &&
         has(v.reasons, "datasharing-qos-enabled-both");
}

}  // namespace

void apply_stats(std::vector<TopicSummary> & topics, const StatsData & stats)
{
  if (!stats.enabled) {
    return;
  }
  // Distinct destination locators reported per source participant. Before Fast DDS 3.5 the
  // statistics DataWriter keeps the default resource limit of 10 instances, so a
  // participant talking to more than 10 locators silently stops reporting new
  // ones - exactly 10 reported locators plus a missing one is the signature.
  // StatsData::writer_instance_limit is false when the tool is built with 3.5 or later.
  std::map<std::string, size_t> locators_per_source;
  for (const auto & s : stats.traffic) {
    locators_per_source[s.src_participant_prefix]++;
  }
  for (auto & t : topics) {
    t.throughput = 0.0;
    t.throughput_available = false;
    t.latency = 0.0;
    t.latency_available = false;
    t.reliability_available = false;
    t.lost_available = false;
    t.lost_packets = 0;
    t.resent = 0;
    // RTPS_LOST entries already in t.lost_packets: several pairs of the topic between the
    // same two participants share one entry
    std::set<const TrafficSample *> topic_lost;
    for (const auto * w : t.writers) {
      for (const auto & g : entity_guids(*w)) {
        if (auto th = stats.throughput.find(g); th != stats.throughput.end()) {
          t.throughput += th->second.mean();
          t.throughput_available = true;
        }
      }
    }
    for (auto & p : t.pairs) {
      Measurement & m = p.measured;
      const std::string & src = p.writer->participant_guid_prefix;
      m.available = stats.participants_with_stats.count(src) > 0;
      // a writer (reader) and its native-buffer companions count as one entity: the samples
      // go through the companions, the heartbeats of the parent still through the parent
      const std::vector<std::string> writer_guids = entity_guids(*p.writer);
      const std::vector<std::string> reader_guids = entity_guids(*p.reader);
      for (const auto & g : writer_guids) {
        if (auto th = stats.throughput.find(g); th != stats.throughput.end()) {
          m.throughput_available = true;
          m.throughput += th->second.mean();
        }
      }
      {
        bool found = false;
        size_t delivered = 0;
        LatencyStat latency;
        for (const auto & wg : writer_guids) {
          for (const auto & rg : reader_guids) {
            if (auto d = stats.delivered.find({wg, rg}); d != stats.delivered.end()) {
              found = true;
              delivered += d->second;
            }
            if (auto l = stats.latency.find({wg, rg});
              l != stats.latency.end() && l->second.samples > 0)
            {
              // the parent pair comes first: `last` is the companions' when they have one
              latency.sum += l->second.sum;
              latency.samples += l->second.samples;
              latency.min = std::min(latency.min, l->second.min);
              latency.max = std::max(latency.max, l->second.max);
              latency.last = l->second.last;
            }
          }
        }
        if (found) {
          m.delivered_samples = delivered;
          m.delivered = delivered > 0;
        }
        if (latency.samples > 0) {
          m.latency_available = true;
          m.latency = latency;
        }
      }
      {
        // Reliability: RTPS_LOST reported by the reader's participant for packets from the
        // writer's participant to the reader's locators, plus the per-entity counters of
        // writer and reader (window deltas).
        Reliability & rel = m.reliability;
        auto delta = [](
          const std::map<std::string, DataCountSample> & map,
          const std::vector<std::string> & guids, uint64_t & out) {
            bool found = false;
            uint64_t sum = 0;
            for (const auto & g : guids) {
              auto it = map.find(g);
              if (it == map.end()) {continue;}
              found = true;
              sum += it->second.last - std::min(it->second.last, it->second.first);
            }
            out = sum;
            return found;
          };
        rel.available |= delta(stats.resent_datas, writer_guids, rel.resent);
        rel.available |= delta(stats.heartbeats, writer_guids, rel.heartbeats);
        rel.available |= delta(stats.gaps, writer_guids, rel.gaps);
        rel.available |= delta(stats.acknacks, reader_guids, rel.acknacks);
        rel.available |= delta(stats.nackfrags, reader_guids, rel.nackfrags);
        // RTPS_LOST is published by the receiving participant only when it sees a
        // sequence-number gap, so its RTPS_LOST writer without a sample means nothing lost.
        // The sequence is per sender participant and destination locator: the loss belongs
        // to the participant pair, not to this writer. Multicast destinations are left out,
        // Fast DDS may number one multicast send once per socket.
        const std::string & dst = p.reader->participant_guid_prefix;
        rel.lost_available = stats.statistics_writers.count({dst, kStatsRtpsLostTopic}) > 0;
        for (const auto & s : stats.lost) {
          if (s.reporter_participant_prefix != dst || s.src_participant_prefix != src ||
            !reader_has_unicast_locator(*p.reader, s.dst, stats.local_addresses))
          {
            continue;
          }
          rel.lost_available = true;
          const uint64_t packets = s.packets - std::min(s.packets, s.packets_first);
          rel.lost_packets += packets;
          if (topic_lost.insert(&s).second) {t.lost_packets += packets;}
        }
        rel.available |= rel.lost_available;
        if (rel.available) {
          t.reliability_available = true;
          t.lost_available |= rel.lost_available;
          t.resent += rel.resent;
          if (rel.lost_packets > 0) {p.verdict.warnings.push_back("rtps-packets-lost");}
        }
      }
      if (m.latency_available) {
        if (!t.latency_available || m.latency.mean() > t.latency) {t.latency = m.latency.mean();}
        t.latency_available = true;
        if (m.latency.mean() < 0.0) {
          // the reader's clock is behind the writer's: hosts without synchronized clocks
          p.verdict.warnings.push_back("latency-clock-skew-suspected");
        }
      }
      // DATA_COUNT is published only when the writer actually sends a DATA submessage,
      // so "the participant has a DATA_COUNT writer but no sample arrived" means zero.
      for (const auto & g : writer_guids) {
        if (auto dc = stats.data_count.find(g); dc != stats.data_count.end()) {
          m.data_count_available = true;
          m.data_submessages += dc->second.last - dc->second.first;
        }
      }
      if (!m.data_count_available &&
        stats.statistics_writers.count({src, kStatsDataCountTopic}))
      {
        m.data_count_available = true;
      }
      for (const auto & s : stats.traffic) {
        if (s.src_participant_prefix != src ||
          !reader_has_locator(*p.reader, s.dst, stats.local_addresses))
        {
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
        const uint64_t packets = s.packets - std::min(s.packets, s.packets_first);
        const double bytes = std::max(0.0, s.bytes - s.bytes_first);
        m.locators.push_back(MeasuredLocator{s.dst, packets, bytes});
        m.packets += packets;
        m.bytes += bytes;
        m.packets_total += s.packets;
        m.bytes_total += s.bytes;
      }

      Verdict & v = p.verdict;
      if (std::find(v.warnings.begin(), v.warnings.end(), "qos-incompatible") != v.warnings.end()) {
        // Nothing should flow. A delivery proof means the rules above are wrong for
        // this Fast DDS version: report it rather than hide it.
        if (m.delivered) {
          v.warnings.push_back("qos-incompatible-but-delivered");
        }
        continue;
      }
      if (std::find(v.warnings.begin(), v.warnings.end(), "shm-ipc-namespace-split") !=
        v.warnings.end())
      {
        // The writer does send over SHM (into its own /dev/shm), so traffic is expected and
        // confirms nothing; only a delivery proof contradicts the split.
        for (auto tr : m.transports) {
          v.reasons.push_back(measured_reason(tr));
        }
        if (m.delivered) {
          v.warnings.push_back("shm-ipc-namespace-split-but-delivered");
        }
        // Fast DDS keeps same-host traffic between two SHM participants on SHM, so non-SHM
        // packets in the window contradict the split too. Not without SHM on one side: the
        // other endpoints of the two participants then legitimately talk over UDP.
        const auto & rs = v.reasons;
        const bool both_shm = std::find(rs.begin(), rs.end(), "both-shm-locators") != rs.end();
        auto non_shm_in_window = [](const MeasuredLocator & l) {
            return l.locator.kind != LocatorKind::SHM && l.packets > 0;
          };
        if (both_shm && std::any_of(m.locators.begin(), m.locators.end(), non_shm_in_window)) {
          v.warnings.push_back("shm-ipc-namespace-split-but-non-shm-traffic");
        }
        continue;
      }
      if (!m.available) {
        v.warnings.push_back("stats-not-enabled-on-writer");
        continue;
      }
      if (v.transport == Transport::DataSharing) {
        // Zero-copy delivery leaves no RTPS trace. Delivery confirmed by
        // HISTORY_LATENCY plus silence on every locator of the reader => certain.
        if (m.delivered && m.transports.empty()) {
          v.confidence = Confidence::Certain;
          replace_code(
            v.reasons, "datasharing-unverified-by-traffic", "datasharing-confirmed-no-traffic");
        } else if (m.delivered && m.data_count_available) {
          // Reliable data-sharing endpoints still exchange heartbeats, so traffic on the
          // link proves nothing; the writer's DATA_COUNT (DATA submessages sent through a
          // transport) does, as long as every reader of the writer uses data-sharing.
          const bool all_readers_datasharing = std::all_of(
            t.pairs.begin(), t.pairs.end(), [&p](const Pair & q) {
              if (q.writer != p.writer) {return true;}
              // a data-sharing reader in another IPC namespace gets no DATA either
              const auto & qv = q.verdict;
              return qv.transport == Transport::DataSharing || is_datasharing_split(qv);
            });
          if (!all_readers_datasharing) {
            replace_code(
              v.reasons, "datasharing-unverified-by-traffic",
              "datasharing-ambiguous-mixed-readers");
          } else if (m.data_submessages == 0) {
            v.confidence = Confidence::Certain;
            replace_code(
              v.reasons, "datasharing-unverified-by-traffic",
              "datasharing-confirmed-no-data-submessages");
          } else {
            // DATA left through a transport although only data-sharing readers exist:
            // Fast DDS did not use zero-copy. Report what was measured.
            v.transport = m.transports.empty() ? Transport::SHM : m.transports.front();
            v.confidence = m.transports.empty() ? Confidence::Likely : Confidence::Certain;
            replace_code(
              v.reasons, "datasharing-unverified-by-traffic",
              "datasharing-data-submessages-sent");
            for (auto tr : m.transports) {
              v.reasons.push_back(measured_reason(tr));
            }
            v.warnings.push_back("datasharing-not-used");
          }
        } else if (!m.transports.empty()) {
          replace_code(
            v.reasons, "datasharing-unverified-by-traffic",
            "datasharing-ambiguous-participant-traffic");
        } else {
          replace_code(
            v.reasons, "datasharing-unverified-by-traffic", "datasharing-no-delivery-observed");
        }
        continue;
      }
      if (m.transports.empty()) {
        // Samples proven delivered (HISTORY_LATENCY) but no RTPS_SENT entry for any of the
        // reader's locators: the statistics did not attribute the packets, which is not
        // the same as an idle link.
        v.warnings.push_back(
          (stats.writer_instance_limit && locators_per_source[src] >= kStatsWriterInstanceLimit) ?
          "stats-writer-instance-limit-suspected" :
          m.delivered ? "delivered-without-measured-traffic" : "no-traffic-observed");
        continue;
      }
      bool matches =
        std::find(m.transports.begin(), m.transports.end(), v.transport) != m.transports.end();
      for (auto tr : m.transports) {
        v.reasons.push_back(measured_reason(tr));
      }
      if (matches) {
        v.confidence = Confidence::Certain;
      } else {
        v.warnings.push_back("measured-transport-mismatch");
      }
      // The locator the prediction selected is one of several the reader announced;
      // Fast DDS may legitimately use another one. Only its complete absence from the
      // measured traffic says something (a multi-homed reader reached over a different
      // interface), so compare against the whole set rather than a single locator.
      // Only worth asking once the kind agrees: a kind that does not is already
      // measured-transport-mismatch, and saying it twice adds nothing.
      if (matches && v.locator.kind != LocatorKind::Invalid &&
        std::none_of(
          m.locators.begin(), m.locators.end(),
          [&](const MeasuredLocator & ml) {
            return same_locator_local(ml.locator, v.locator, stats.local_addresses);
          }))
      {
        v.warnings.push_back("measured-locator-mismatch");
      }
    }
  }
}

DiscoveryStatus discovery_completeness(
  const std::map<ParticipantPrefix, std::vector<EndpointGid>> & announced,
  const std::set<ParticipantPrefix> & live_participants,
  const std::vector<Endpoint> & discovered)
{
  DiscoveryStatus out;
  out.endpoints = discovered.size();
  std::set<EndpointGid> seen;
  for (const auto & e : discovered) {
    seen.insert(e.guid_bytes);
  }
  size_t compared = 0;
  for (const auto & [participant, gids] : announced) {
    // A participant that has left announced endpoints that no longer exist: its row stays in
    // the table on purpose (a gid is unique, so it never names another endpoint).
    if (live_participants.count(participant) == 0) {continue;}
    ++compared;
    size_t missing = 0;
    for (const auto & gid : gids) {
      if (seen.count(gid) == 0) {++missing;}
    }
    if (missing > 0) {
      out.announced_not_discovered += missing;
      out.announced_by_incomplete += gids.size();
      ++out.participants_incomplete;
    }
  }
  if (compared == 0) {
    return out;   // complete stays unset: nothing was announced, so nothing can be judged
  }
  out.complete = out.announced_not_discovered == 0;
  return out;
}

std::string incomplete_discovery_warning(
  const DiscoveryStatus & status, double quiet, double timeout, bool stats)
{
  if (!status.complete.has_value() || *status.complete) {return {};}
  auto longer = [](double current, int64_t least) {
      return std::to_string(std::max(least, static_cast<int64_t>(std::ceil(current * 2.0))));
    };
  std::ostringstream out;
  out << "warning: discovery was still in progress ("
      << status.announced_not_discovered << " of " << status.announced_by_incomplete
      << " endpoints announced by " << status.participants_incomplete
      << (status.participants_incomplete == 1 ? " participant" : " participants")
      << " were not seen); ";
  if (stats) {
    out << "pass --timeout " << longer(timeout, 15);   // --stats ignores the quiet window
  } else if (quiet > 0) {
    out << "pass --quiet " << longer(quiet, 3) << " or --timeout " << longer(timeout, 10);
  } else {
    out << "pass --timeout " << longer(timeout, 10);
  }
  out << " for a complete view";
  return out.str();
}

PairKey pair_key(const TopicSummary & topic, const Pair & pair)
{
  return PairKey{topic.display_topic, pair.writer->guid, pair.reader->guid,
    pair.writer->node_name, pair.reader->node_name};
}

PairState pair_state(const Pair & pair)
{
  PairState s;
  s.transport = pair.verdict.transport;
  s.confidence = pair.verdict.confidence;
  s.measured = pair.measured.transports;
  s.locator = pair.verdict.locator;
  for (const auto & ml : pair.measured.locators) {
    s.measured_locators.push_back(ml.locator);
  }
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

Changes diff(
  const std::map<PairKey, PairState> & previous, const std::map<PairKey, PairState> & current)
{
  Changes c;
  for (const auto & kv : current) {
    auto it = previous.find(kv.first);
    if (it == previous.end()) {
      c.added.push_back(kv.first);
    } else if (it->second != kv.second) {
      c.changed.push_back(PairChange{kv.first, it->second, kv.second, it->first});
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
/// The pairs of a snapshot under the node key: (topic, writer identity, reader identity)
/// with the identity "<node>#<n>" (n-th endpoint of that node on the topic, in GUID
/// order) or "guid:<guid>" for an endpoint without a node name.
std::map<PairKey, std::pair<PairKey, PairState>> node_keyed(const Snapshot & snap)
{
  std::map<PairKey, std::pair<PairKey, PairState>> out;
  for (const auto & t : snap.topics) {
    // ordinal of every endpoint within its node on this topic
    std::map<std::string, std::vector<std::string>> by_node;   // node -> guids
    for (const auto * lists : {&t.writers, &t.readers}) {
      for (const auto * e : *lists) {
        if (!e->node_name.empty()) {by_node[e->node_name].push_back(e->guid);}
      }
    }
    for (auto & kv : by_node) {
      std::sort(kv.second.begin(), kv.second.end());
    }
    auto identity = [&](const Endpoint & e) {
        if (e.node_name.empty()) {return "guid:" + e.guid;}
        const auto & guids = by_node[e.node_name];
        auto pos = std::find(guids.begin(), guids.end(), e.guid) - guids.begin();
        return e.node_name + "#" + std::to_string(pos);
      };
    for (const auto & p : t.pairs) {
      PairKey ident{t.display_topic, identity(*p.writer), identity(*p.reader)};
      out[ident] = {pair_key(t, p), pair_state(p)};
    }
  }
  return out;
}

/// PairState equality for the node key: a restart renumbers the ports of a participant's
/// locators (7413, 7415, ... by participant id), which says nothing about how the pair
/// talks, so the port numbers of the selected and measured locators are left out. The
/// kinds and addresses still count.
bool same_ignoring_ports(const PairState & a, const PairState & b)
{
  auto strip = [](PairState s) {
      s.locator.port = 0;
      for (auto & l : s.measured_locators) {
        l.port = 0;
      }
      return s;
    };
  return strip(a) == strip(b);
}
}  // namespace

Changes diff_snapshots(const Snapshot & before, const Snapshot & after, KeyMode mode)
{
  Changes c;
  c.key = mode;
  if (mode == KeyMode::Guid) {
    auto by_guid = diff(pair_states(before), pair_states(after));
    c.added = std::move(by_guid.added);
    c.removed = std::move(by_guid.removed);
    c.changed = std::move(by_guid.changed);
    return c;
  }
  const auto prev = node_keyed(before);
  const auto cur = node_keyed(after);
  for (const auto & kv : cur) {
    auto it = prev.find(kv.first);
    if (it == prev.end()) {
      c.added.push_back(kv.second.first);
    } else if (!same_ignoring_ports(it->second.second, kv.second.second)) {
      c.changed.push_back(
        PairChange{kv.second.first, it->second.second, kv.second.second, it->second.first});
    }
  }
  for (const auto & kv : prev) {
    if (!cur.count(kv.first)) {
      c.removed.push_back(kv.second.first);
    }
  }
  return c;
}

namespace
{
/// Description ("what happened") and remedy ("what to change") of a code. Every code has
/// a remedy or an explicit std::nullopt: a normal state (same-host-guid), a measured fact
/// (measured-shm-traffic) or a request for a bug report has nothing to change.
struct CodeInfo
{
  std::string description;
  std::optional<std::string> remedy;
};

const char kStatsEnv[] =
  "FASTDDS_STATISTICS=\"RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;"
  "PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC;RESENT_DATAS_TOPIC;"
  "HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC\"";

const std::map<std::string, CodeInfo> & explanations()
{
  static const std::map<std::string, CodeInfo> m = {
    // ---- transport selection
    {"same-host-guid", {
        "Writer and reader GUID prefixes share the same first 4 bytes, which is how Fast DDS "
        "decides both participants run on the same host.",
        std::nullopt}},
    {"different-host", {
        "GUID prefixes differ in the first 4 bytes, so Fast DDS treats the endpoints as being on "
        "different hosts; shared memory is not an option.",
        "For SHM between containers on one machine, run them in the host's network and IPC "
        "namespaces (--network host --ipc host); between machines only network transports "
        "apply."}},
    {"both-shm-locators", {
        "Both endpoints announce a SHM locator. On the same host Fast DDS then uses the shared "
        "memory transport exclusively for user data (discovery still goes over UDP).",
        std::nullopt}},
    {"writer-no-shm-locator", {
        "The writer's participant announces no SHM locator: SHM transport is not instantiated on "
        "its side (e.g. FASTDDS_BUILTIN_TRANSPORTS=UDPv4, or an XML profile without SHM).",
        "Enable SHM on the writer's participant: unset FASTDDS_BUILTIN_TRANSPORTS or set it to "
        "DEFAULT / LARGE_DATA (Fast DDS >= 2.11), or add a SHM <transport_descriptor> to its "
        "XML participant profile."}},
    {"reader-no-shm-locator", {
        "The reader's participant announces no SHM locator: SHM transport is not instantiated on "
        "its side (e.g. FASTDDS_BUILTIN_TRANSPORTS=UDPv4, or an XML profile without SHM).",
        "Enable SHM on the reader's participant: unset FASTDDS_BUILTIN_TRANSPORTS or set it to "
        "DEFAULT / LARGE_DATA (Fast DDS >= 2.11), or add a SHM <transport_descriptor> to its "
        "XML participant profile."}},
    {"shm-locators-ignored-across-hosts", {
        "Both endpoints announce SHM locators, but they are on different hosts so the SHM "
        "locators are discarded.",
        std::nullopt}},
    {"shm-ipc-namespace-split", {
        "The writer and the reader have the same host id, but their shared-memory ports or "
        "data-sharing segments are in different IPC namespaces: Fast DDS still selects SHM or "
        "data-sharing and every sample between them is lost.",
        "Put both nodes in one IPC namespace (ipc: host on both containers, or the same "
        "container), or disable SHM and data-sharing on one side (FASTDDS_BUILTIN_TRANSPORTS="
        "UDPv4 on Fast DDS >= 2.11 or an XML transport profile, and data_sharing OFF in its "
        "QoS profile)."}},
    {"shm-port-collision", {
        "The writer's and the reader's participants announce the same SHM port number. Only one "
        "participant per IPC namespace can listen on a port, so the two use different /dev/shm.",
        std::nullopt}},
    {"shm-reader-port-not-visible", {
        "The writer's SHM ports are open in the tool's IPC namespace and a port of the reader's "
        "is not (no lock file here, or it is one of the tool's own ports), so the two use "
        "different /dev/shm.",
        std::nullopt}},
    {"shm-writer-port-not-visible", {
        "The reader's SHM ports are open in the tool's IPC namespace and a port of the writer's "
        "is not (no lock file here, or it is one of the tool's own ports), so the two use "
        "different /dev/shm.",
        std::nullopt}},
    {"datasharing-reader-segment-not-visible", {
        "The writer's data-sharing history is in the tool's /dev/shm and the reader's "
        "notification segment is not, so the two use different /dev/shm: the reader cannot "
        "open the writer's history and rejects the writer.",
        std::nullopt}},
    {"datasharing-writer-segment-not-visible", {
        "The reader's data-sharing notification segment is in the tool's /dev/shm and the "
        "writer's history is not, so the two use different /dev/shm: the reader cannot open "
        "the writer's history and rejects the writer.",
        std::nullopt}},
    {"shm-ipc-namespace-split-but-delivered", {
        "HISTORY_LATENCY statistics prove that samples reached the reader although the writer and "
        "the reader were judged to be in different IPC namespaces: the tool's split detection is "
        "wrong for this setup. Please report this with the --json output.",
        std::nullopt}},
    {"shm-ipc-namespace-split-but-non-shm-traffic", {
        "RTPS_SENT statistics show non-SHM packets from the writer's participant to the "
        "reader's locators during the observation, although both announce SHM and were judged "
        "to be in different IPC namespaces: Fast DDS sends same-host traffic over SHM only, so "
        "the split detection may be wrong for this setup. Please report this with the --json "
        "output.",
        std::nullopt}},
    {"common-udpv4-locator", {
        "The reader announces a UDPv4 locator and the writer speaks UDPv4.", std::nullopt}},
    {"common-udpv6-locator", {
        "The reader announces a UDPv6 locator and the writer speaks UDPv6.", std::nullopt}},
    {"common-tcpv4-locator", {
        "The reader announces a TCPv4 locator and the writer speaks TCPv4.", std::nullopt}},
    {"common-tcpv6-locator", {
        "The reader announces a TCPv6 locator and the writer speaks TCPv6.", std::nullopt}},
    {"no-common-transport", {
        "No locator kind is shared by both endpoints; user data cannot flow between them.",
        "Give both participants a transport in common: the same FASTDDS_BUILTIN_TRANSPORTS "
        "value on both nodes (Fast DDS >= 2.11), or matching <transport_descriptor> entries in "
        "both XML participant profiles."}},
    {"host-id-match-but-ip-differs", {
        "The endpoints share a host id but announce no common IP address (typical for containers "
        "with separate network namespaces on one machine). SHM works only if /dev/shm is shared "
        "(the pair gets shm-ipc-namespace-split when the tool can tell that it is not).",
        "Share /dev/shm between the containers (--ipc host on both, or --ipc container:<name>) so "
        "that the SHM verdict holds; otherwise put them on one network for UDP."}},
    {"same-host-locators-hidden", {
        "Fast DDS below 2.10 (ROS 2 Humble) announces only the SHM locator of a participant on the "
        "same host to this tool, so the network locators of one side are not visible. The other "
        "side has no SHM locator, and both keep the builtin UDPv4 transport, so Fast DDS falls "
        "back to UDPv4 between them; the tool cannot see it in discovery data.",
        "Observe with Fast DDS >= 2.10 (ROS 2 Jazzy or newer) to see the network locators of "
        "same-host participants; on Humble the verdict stays 'likely'."}},
    // ---- data-sharing (prediction)
    {"datasharing-disabled-writer", {
        "The writer announces data-sharing OFF (explicitly disabled, or AUTO resolved to OFF "
        "because the type is unbounded / the history memory policy is not preallocated).",
        "Enable data-sharing on the writer: <data_sharing><kind>ON</kind> in its data_writer XML "
        "profile, a bounded (fixed-size) message type and a PREALLOCATED or "
        "PREALLOCATED_WITH_REALLOC <historyMemoryPolicy>."}},
    {"datasharing-disabled-reader", {
        "The reader announces data-sharing OFF (explicitly disabled, or AUTO resolved to OFF "
        "because the type is unbounded / the history memory policy is not preallocated).",
        "Enable data-sharing on the reader: <data_sharing><kind>ON</kind> in its data_reader XML "
        "profile, a bounded (fixed-size) message type and a PREALLOCATED or "
        "PREALLOCATED_WITH_REALLOC <historyMemoryPolicy>."}},
    {"datasharing-qos-unknown", {
        "The data-sharing QoS of at least one endpoint could not be read from discovery data.",
        std::nullopt}},
    {"datasharing-qos-enabled-both", {
        "Both endpoints announce data-sharing ON or AUTO. Fast DDS resolves AUTO before "
        "discovery, so this means both sides consider themselves data-sharing capable.",
        std::nullopt}},
    {"datasharing-domain-ids-match", {
        "The data-sharing domain ids announced by writer and reader intersect, which is required "
        "for zero-copy delivery to be matched.",
        std::nullopt}},
    {"datasharing-domain-ids-unknown", {
        "At least one side announced no data-sharing domain id; matching cannot be confirmed "
        "from discovery data alone.",
        std::nullopt}},
    {"datasharing-domain-ids-mismatch", {
        "Both sides are data-sharing capable but their domain ids do not intersect, so Fast DDS "
        "falls back to the transports.",
        "Make the <data_sharing><domain_ids> lists of writer and reader intersect, or leave both "
        "empty so that Fast DDS derives the id from the host."}},
    {"datasharing-unverified-by-traffic", {
        "Zero-copy delivery produces no RTPS traffic, so discovery data alone cannot tell whether "
        "user data is sent over a transport for this pair.",
        "Run the tool with --stats (the observed nodes need FASTDDS_STATISTICS, see --help) to "
        "confirm that no user data crosses a transport."}},
    // ---- data-sharing (measured)
    {"datasharing-confirmed-no-traffic", {
        "HISTORY_LATENCY statistics prove samples reached the reader while no RTPS packets went to "
        "any of its locators: zero-copy data-sharing delivery is confirmed.",
        std::nullopt}},
    {"datasharing-confirmed-no-data-submessages", {
        "HISTORY_LATENCY statistics prove samples reached the reader while the writer's DATA_COUNT "
        "did not grow: no DATA submessage left through a transport, so zero-copy data-sharing "
        "delivery is confirmed (needs DATA_COUNT_TOPIC on the observed nodes).",
        std::nullopt}},
    {"datasharing-ambiguous-mixed-readers", {
        "The writer also serves readers without data-sharing, so its DATA_COUNT mixes both "
        "delivery paths and cannot confirm zero-copy for this pair.",
        "To confirm zero-copy, observe while every reader of this writer announces data-sharing "
        "(stop or --node-filter the others), or enable data-sharing on those readers too."}},
    {"datasharing-data-submessages-sent", {
        "Every reader of this writer announces data-sharing, yet the writer sent DATA submessages "
        "through a transport during the observation: Fast DDS did not use zero-copy delivery.",
        "Check that writer and reader share a data-sharing domain id and that the sample fits the "
        "preallocated payload of the writer's history; otherwise Fast DDS falls back to the "
        "transport."}},
    {"datasharing-not-used", {
        "Data-sharing was announced by both sides but the writer's DATA_COUNT grew while it only "
        "had data-sharing readers; the verdict shows the transport that was measured instead.",
        std::nullopt}},
    {"datasharing-ambiguous-participant-traffic", {
        "The writer's participant did send packets to the reader's locators. Statistics are per "
        "participant, and reliable data-sharing endpoints still exchange heartbeats/acknacks over "
        "the transport, so this does not disprove zero-copy delivery; it just cannot confirm it.",
        "Enable DATA_COUNT_TOPIC in FASTDDS_STATISTICS on the observed nodes so that DATA "
        "submessages can be told from heartbeats and acknacks, or use BEST_EFFORT reliability on "
        "the pair."}},
    {"datasharing-no-delivery-observed", {
        "No HISTORY_LATENCY sample for this pair yet (needs HISTORY_LATENCY_TOPIC enabled and at "
        "least one published sample), so data-sharing remains unconfirmed.",
        "Start the observed nodes with HISTORY_LATENCY_TOPIC in FASTDDS_STATISTICS and let the "
        "writer publish at least one sample during the observation (longer --timeout)."}},
    // ---- QoS request/offer
    {"qos-incompatible-reliability", {
        "The writer offers BEST_EFFORT while the reader requests RELIABLE; Fast DDS does not "
        "match them.",
        "Offer RELIABLE on the writer or request BEST_EFFORT on the reader: the reliability of the "
        "QoS profile passed to create_publisher / create_subscription in ROS 2."}},
    {"qos-incompatible-durability", {
        "The writer offers a weaker durability (VOLATILE < TRANSIENT_LOCAL < TRANSIENT < "
        "PERSISTENT) than the reader requests; Fast DDS does not match them.",
        "Offer at least the requested durability on the writer (e.g. TRANSIENT_LOCAL) or request "
        "VOLATILE on the reader: the durability of the ROS 2 QoS profile."}},
    {"qos-incompatible-deadline", {
        "The writer's deadline period is longer than the one the reader requests; Fast DDS does "
        "not match them.",
        "Shorten the writer's deadline period to at most the reader's, or lengthen the reader's "
        "request: the deadline of the ROS 2 QoS profile."}},
    {"qos-incompatible-liveliness", {
        "The writer's liveliness kind is weaker "
        "(AUTOMATIC < MANUAL_BY_PARTICIPANT < MANUAL_BY_TOPIC) "
        "or its lease duration longer than the reader requests; Fast DDS does not match them.",
        "Offer at least the requested liveliness kind and a lease no longer than requested on the "
        "writer, or relax the reader's request: liveliness / liveliness_lease_duration of the "
        "ROS 2 QoS profile."}},
    {"qos-incompatible-ownership", {
        "Writer and reader announce different ownership kinds (SHARED / EXCLUSIVE); Fast DDS does "
        "not match them.",
        "Use the same ownership kind (SHARED or EXCLUSIVE) on both sides; ROS 2 does not expose "
        "it, so check the <ownership> element of the nodes' XML profiles."}},
    {"qos-incompatible-partition", {
        "No partition name of the writer matches one of the reader (an empty list is the default "
        "partition); Fast DDS does not match them.",
        "Give writer and reader a common partition name, or none on both; ROS 2 does not set "
        "partitions, so check the <partition> element of the nodes' XML profiles."}},
    {"qos-incompatible", {
        "The pair's QoS are incompatible, so the endpoints are never matched and no data flows; "
        "the ROS 2 side reports this as an incompatible QoS event.",
        std::nullopt}},
    {"qos-incompatible-but-delivered", {
        "HISTORY_LATENCY statistics prove that samples reached the reader although the QoS were "
        "judged incompatible: the tool's matching rules disagree with this Fast DDS version. "
        "Please report this with the --json output.",
        std::nullopt}},
    // ---- topics without pairs
    {"no-matching-writer", {
        "No publisher was discovered for this topic.",
        "Start a publisher on this topic, or check the topic name, namespace and remappings of "
        "the node expected to publish it."}},
    {"no-matching-reader", {
        "No subscription was discovered for this topic.",
        "Start a subscription on this topic, or check the topic name, namespace and remappings of "
        "the node expected to subscribe to it."}},
    {"type-name-mismatch", {
        "A writer and a reader on this topic announce different type names, so they do not match.",
        "Use the same message type (package and name) on both sides of the topic; ROS 2 announces "
        "it as <pkg>::msg::dds_::<Name>_."}},
    // ---- rmw native-buffer companion topics
    {"buffer-companion-folded", {
        "rmw_fastrtps_cpp (ROS 2 Lyrical and later) gives a writer or reader of a type with an "
        "unbounded uint8[] field a companion on the hidden topic <topic>/_buf_cpu and sends the "
        "samples there whenever every subscription supports native buffers; the statistics "
        "counters of the companions are added to this pair.",
        std::nullopt}},
    {"buffer-companion", {
        "This is a native-buffer companion topic (<topic>/_buf_cpu) of rmw_fastrtps_cpp: its "
        "endpoints belong to the writers and readers of the parent topic in the same "
        "participants, whose pairs also count its statistics. The default view hides it; --all "
        "shows it with its own counters.",
        std::nullopt}},
    {"buffer-companion-unmatched", {
        "An endpoint on this <topic>/_buf_cpu topic has no single writer or reader of the parent "
        "topic with the same type in its participant, so its statistics counters are not added "
        "to a parent pair.",
        "Nothing needs to change in the nodes: read the counters of this topic together with "
        "those of the writer or reader of the parent topic (the name without /_buf_cpu) in the "
        "same participant."}},
    // ---- measured traffic
    {"measured-udpv4-traffic", {
        "Statistics show RTPS packets from the writer's participant to the reader's UDPv4 "
        "locator.", std::nullopt}},
    {"measured-udpv6-traffic", {
        "Statistics show RTPS packets from the writer's participant to the reader's UDPv6 "
        "locator.", std::nullopt}},
    {"measured-tcpv4-traffic", {
        "Statistics show RTPS packets from the writer's participant to the reader's TCPv4 "
        "locator.", std::nullopt}},
    {"measured-tcpv6-traffic", {
        "Statistics show RTPS packets from the writer's participant to the reader's TCPv6 "
        "locator.", std::nullopt}},
    {"measured-shm-traffic", {
        "Statistics show RTPS packets from the writer's participant to the reader's SHM "
        "locator.", std::nullopt}},
    {"measured-unknown-traffic", {
        "Statistics show RTPS packets to a locator of unknown kind.", std::nullopt}},
    {"measured-transport-mismatch", {
        "The transport predicted from discovery data differs from the locator kind(s) that "
        "actually carried packets. Please report this with the --json output.",
        std::nullopt}},
    {"measured-locator-mismatch", {
        "The locator the tool selected from the reader's announced locators carried no packets; "
        "the traffic went to another locator of the same kind that the reader also announced - "
        "for example a multi-homed host reached over a different interface.",
        "If one interface is intended, restrict the reader's participant to it with an "
        "<interfaceWhiteList> in the <transport_descriptor> of its XML profile; otherwise nothing "
        "needs to change."}},
    // ---- statistics availability
    {"stats-writer-instance-limit-suspected", {
        "The writer's participant reports traffic to 10 or more locators but none to this reader. "
        "Before Fast DDS 3.5 the statistics DataWriter keeps the default resource limit of 10 "
        "instances (one per destination locator), so counters for further locators are never "
        "published.",
        "Start the observed nodes with FASTRTPS_DEFAULT_PROFILES_FILE (Fast DDS 2.x) or "
        "FASTDDS_DEFAULT_PROFILES_FILE (3.x, where ROS 2 nodes also accept FASTRTPS_) pointing at "
        "this package's config/statistics.xml (a data_writer profile per statistics alias whose "
        "<resourceLimitsQos> sets max_instances to 0)."}},
    {"delivered-without-measured-traffic", {
        "HISTORY_LATENCY statistics prove that samples reached the reader, but RTPS_SENT reported "
        "no packets to any of the reader's locators during the observation, so the transport that "
        "carried them could not be measured (seen with large samples over SHM on slow machines).",
        "Repeat with a longer --timeout so that RTPS_SENT catches up with the delivery; if it "
        "persists, the transport of this pair cannot be measured."}},
    {"no-traffic-observed", {
        "The writer's participant publishes statistics but sent no packets to any locator of the "
        "reader during the observation window.",
        "Make the writer publish during the observation (an idle topic has nothing to measure), "
        "or use a longer --timeout."}},
    {"stats-not-enabled-on-writer", {
        "No statistics were received from the writer's participant: it was started without "
        "FASTDDS_STATISTICS, its Fast DDS has no statistics module (ROS 2 Humble), or it has "
        "no transport in common with the tool's statistics readers, which announce no SHM "
        "locator (a participant with the SHM transport only).",
        std::string("Start the writer's node with ") + kStatsEnv +
        " set before it creates its participant."}},
    {"latency-clock-skew-suspected", {
        "The mean HISTORY_LATENCY of this pair is negative: the reader's host clock is behind the "
        "writer's. The latency is write time on the writer's host minus notification time on the "
        "reader's host, so across hosts it includes their clock offset.",
        "Synchronize the clocks of the two hosts (chrony / PTP), or read the latency only between "
        "nodes on one host."}},
    {"rtps-packets-lost", {
        "The reader's participant reported RTPS packets from the writer's participant to the "
        "reader's unicast locators as lost (RTPS_LOST, sequence-number gaps) during the "
        "observation: the link drops packets. The count covers all traffic between the two "
        "participants (other topics and discovery included), so every pair between them shows "
        "it. Reliable pairs recover the samples by resends (RESENT_DATAS), best-effort pairs "
        "lose them.",
        "Check the link (Wi-Fi, MTU, switch), raise the socket buffers (<sendBufferSize> / "
        "<receiveBufferSize> of the transport descriptor, net.core.rmem_max), and use RELIABLE "
        "reliability where samples must not be lost."}},
    // ---- shared memory of the environment
    {"shm-stale-files", {
        "Fast DDS files in the shared-memory directory whose lock nobody holds: their owner "
        "process ended without cleaning up (a crash or a kill). They keep consuming /dev/shm.",
        "Run 'fastdds shm clean' to remove the files whose owner is gone."}},
    {"shm-nearly-full", {
        "The shared-memory directory is at least 90% full or has less than 16 MB free. Fast DDS "
        "cannot create its segment when /dev/shm is full, so participants fail to start or fall "
        "back to UDP.",
        "Free /dev/shm ('fastdds shm clean' removes stale Fast DDS files) or enlarge it; Docker's "
        "default is only 64 MB, so start the container with --shm-size or --ipc=host."}},
    {"shm-not-visible", {
        "Observed nodes announce SHM locators that are not open in this process's shared-memory "
        "directory (nobody holds the port's lock, the port number collides with the tool's own, "
        "or the node has another host id): they run in another IPC namespace or on another host, "
        "so the shared-memory figures describe this environment, not theirs, and SHM cannot be "
        "used between them and here. Nodes with the same host id in different IPC namespaces "
        "still pick SHM between themselves and lose every message; their pairs get "
        "shm-ipc-namespace-split when the nodes announce the same SHM port number or the tool "
        "shares the IPC namespace of one of them, and cannot be checked from here otherwise.",
        "Run the tool where the nodes run (same host and IPC namespace: the same container, or "
        "--ipc=host on both) if the shared-memory line should describe their /dev/shm."}},
  };
  return m;
}
}  // namespace

std::string explain(const std::string & code)
{
  const auto & m = explanations();
  auto it = m.find(code);
  return it == m.end() ? std::string("(no description)") : it->second.description;
}

std::optional<std::string> remedy(const std::string & code)
{
  const auto & m = explanations();
  auto it = m.find(code);
  return it == m.end() ? std::nullopt : it->second.remedy;
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
