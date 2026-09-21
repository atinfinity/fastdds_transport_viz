// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// transport_viz: show which Fast DDS transport each ROS 2 topic uses and why.

#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <map>
#include <sstream>
#include <ctime>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>

#include "rclcpp/rclcpp.hpp"
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/fastdds_compat.hpp"
#include "fastdds_transport_viz/fastdds_util.hpp"
#include "fastdds_transport_viz/discovery_observer.hpp"
#include "fastdds_transport_viz/model.hpp"
#include "fastdds_transport_viz/parse_json.hpp"
#include "fastdds_transport_viz/render.hpp"
#include "fastdds_transport_viz/rmw_check.hpp"
#include "fastdds_transport_viz/ros_discovery_info_observer.hpp"
#include "fastdds_transport_viz/ros_graph_resolver.hpp"
#include "fastdds_transport_viz/shm_info.hpp"
#include "fastdds_transport_viz/stats_observer.hpp"

namespace
{

// Sends fd 1 to fd 2 for the lifetime of the object (or until restore()).
class StdoutToStderr
{
public:
  explicit StdoutToStderr(bool enable)
  {
    if (enable) {
      std::fflush(stdout);
      saved_ = dup(STDOUT_FILENO);
      dup2(STDERR_FILENO, STDOUT_FILENO);
    }
  }
  ~StdoutToStderr() {restore();}
  void restore()
  {
    if (saved_ >= 0) {
      std::fflush(stdout);
      dup2(saved_, STDOUT_FILENO);
      close(saved_);
      saved_ = -1;
    }
  }

private:
  int saved_ = -1;
};

}  // namespace

using namespace std::chrono_literals;
using fastdds_transport_viz::Endpoint;
using fastdds_transport_viz::GhostPair;
using fastdds_transport_viz::KeyMode;
using fastdds_transport_viz::PairKey;
using fastdds_transport_viz::PairState;
using fastdds_transport_viz::RenderOptions;
using fastdds_transport_viz::Snapshot;
using fastdds_transport_viz::WatchDecorations;

namespace
{

struct Options
{
  int domain{-1};             // -1 => ROS_DOMAIN_ID / 0
  double timeout{-1.0};       // seconds to wait for discovery (-1: 3, or 20 with --stats)
  bool stats{false};
  double quiet{1.0};          // stop early after this many silent seconds
  bool json{false};
  bool verbose{false};
  bool explain{false};
  bool locators{false};
  bool advise{false};
  bool all{false};
  bool watch{false};
  double interval{2.0};
  std::string topic_regex;
  std::string node_regex;
  bool list_codes{false};
  enum class Color { Auto, Always, Never } color{Color::Auto};
  // `transport_viz diff <before> <after>`: compare two saved --json documents
  std::string command;              // "" (observe) or "diff"
  std::vector<std::string> files;   // the two documents ('-' = stdin)
  KeyMode key{KeyMode::Node};
  bool changes_only{false};
};

void usage()
{
  std::cout <<
    "Usage: transport_viz [options]\n"
    "       transport_viz diff <before.json> <after.json> [options]\n"
    "\n"
    "Show which Fast DDS transport each ROS 2 topic is communicated over and why.\n"
    "Run it in the same environment (env vars, XML profile, network/IPC namespace)\n"
    "as the nodes you want to observe.\n"
    "\n"
    "Options:\n"
    "  --domain <id>      DDS domain id (default: $ROS_DOMAIN_ID or 0)\n"
    "  --timeout <sec>    max time to wait for discovery (default: 3, 30 with --stats)\n"
    "  --quiet <sec>      stop early after this many seconds without discovery events\n"
    "                     (default: 1; with --stats also once every participant with\n"
    "                     statistics has been heard from and the traffic entries measured\n"
    "                     towards a discovered reader stop growing for max(--quiet, 3) s,\n"
    "                     never before 5 s;\n"
    "                     --watch --stats draws its first frame once discovery is quiet\n"
    "                     and 5 s have passed)\n"
    "  --topic <regex>    only show topics whose (ROS) name matches the regex\n"
    "  --node <regex>     only show pairs where the writer or the reader belongs to a\n"
    "                     node whose full name matches the regex (that node's unpaired\n"
    "                     endpoints are kept too)\n"
    "  --all              include services/actions and non-ROS DDS topics\n"
    "  -v, --verbose      expand writer -> reader pairs under each topic\n"
    "  --explain          print a legend for every reason code used\n"
    "  --locators         add a line under each pair with the locator the tool selected\n"
    "                     and the locators that actually carried packets (implies -v;\n"
    "                     ignored with --json, which always carries them)\n"
    "  --advise           add a 'fix <code>: ...' line under each pair for its reason codes\n"
    "                     that have a remedy, and the remedy under each code of the legend\n"
    "                     (implies -v and --explain; ignored with --json, which always\n"
    "                     carries them as reason_code_remedies)\n"
    "  --json             emit JSON (schema_version 1) instead of a table\n"
    "  --stats            also subscribe to the Fast DDS statistics topics and show the\n"
    "                     transport that actually carried packets; observed nodes must run\n"
    "                     with FASTDDS_STATISTICS=\"RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;"
    "HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;"
    "RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;"
    "GAP_COUNT_TOPIC\"\n"
    "  --color <mode>     auto|always|never: ANSI colors for transports and warnings\n"
    "                     (default: auto = only when stdout is a terminal; honours NO_COLOR)\n"
    "  --watch            keep observing and re-render every --interval seconds, marking\n"
    "                     added (+), changed (~) and removed (-) pairs; on a terminal, keys:\n"
    "                     q quit, p pause, v pairs, e legend, a all, l locators, f fixes.\n"
    "                     With --json, emits one\n"
    "                     compact document per line (JSON Lines) with a `changes` object\n"
    "  --interval <sec>   refresh period for --watch (default: 2)\n"
    "  --list-codes       list all reason codes with descriptions and remedies and exit\n"
    "  -h, --help         this help\n"
    "\n"
    "diff compares two --json documents without observing anything (a JSON Lines file of\n"
    "--watch --json counts by its last document; '-' reads one of them from stdin). It\n"
    "prints the after document with the pairs that were added (+), changed (~) or removed\n"
    "(-) marked as in --watch, or with --json the after document plus a `changes` object.\n"
    "Exit status: 0 no changes, 1 changes, 2 error. Takes --json, --color, --topic, --node,\n"
    "--all, -v, --explain, --locators, --advise and:\n"
    "  --key <mode>       node|guid: match the pairs of the two documents by (topic, writer\n"
    "                     node, reader node), which survives restarting the nodes (default;\n"
    "                     locator port numbers, renumbered by a restart, are ignored), or\n"
    "                     by their GUIDs as --watch does\n"
    "  --changes-only     only the topics with an added, changed or removed pair (with\n"
    "                     --json, `topics` is pruned the same way)\n";
}

bool parse(int argc, char ** argv, Options & o)
{
  auto need = [&](int & i, const char * flag) -> const char * {
      if (i + 1 >= argc) {
        std::cerr << flag << " requires a value\n";
        std::exit(2);
      }
      return argv[++i];
    };
  int first = 1;
  if (argc > 1 && argv[1][0] != '-') {
    o.command = argv[1];
    if (o.command != "diff") {
      std::cerr << "unknown command: " << o.command << "\n";
      usage();
      return false;
    }
    first = 2;
  }
  std::set<std::string> seen;   // flags given, for the ones diff does not take
  for (int i = first; i < argc; ++i) {
    std::string a = argv[i];
    if (a.size() > 1 && a[0] == '-') {seen.insert(a);}
    if (o.command == "diff" && (a == "-" || a[0] != '-')) {
      o.files.push_back(a);
      continue;
    }
    if (a == "--domain") {o.domain = std::atoi(need(i, "--domain"));} else if (a == "--timeout") {
      o.timeout = std::atof(need(i, "--timeout"));
    } else if (a == "--quiet") {o.quiet = std::atof(need(i, "--quiet"));} else if (a == "--topic") {
      o.topic_regex = need(i, "--topic");
    } else if (a == "--node") {
      o.node_regex = need(i, "--node");
    } else if (a == "--interval") {
      o.interval = std::atof(need(i, "--interval"));
    } else if (a == "--color") {
      std::string m = need(i, "--color");
      if (m == "auto") {o.color = Options::Color::Auto;} else if (m == "always") {
        o.color = Options::Color::Always;
      } else if (m == "never") {o.color = Options::Color::Never;} else {
        std::cerr << "--color expects auto, always or never\n";
        return false;
      }
    } else if (a == "--all") {
      o.all = true;
    } else if (a == "-v" || a == "--verbose") {o.verbose = true;} else if (a == "--explain") {
      o.explain = true;
    } else if (a == "--locators") {
      o.locators = true;
      o.verbose = true;   // the line hangs under a pair row, which only -v prints
    } else if (a == "--advise") {
      o.advise = true;
      o.verbose = true;   // same: the fix lines hang under pair rows
      o.explain = true;   // and the legend carries the remedy of every code in use
    } else if (a == "--json") {
      o.json = true;
    } else if (a == "--stats") {
      o.stats = true;
    } else if (a == "--watch") {
      o.watch = true;
    } else if (a == "--list-codes") {
      o.list_codes = true;
    } else if (a == "--key") {
      std::string m = need(i, "--key");
      if (m == "node") {o.key = KeyMode::Node;} else if (m == "guid") {
        o.key = KeyMode::Guid;
      } else {
        std::cerr << "--key expects node or guid\n";
        return false;
      }
    } else if (a == "--changes-only") {
      o.changes_only = true;
    } else if (a == "-h" || a == "--help") {
      usage(); std::exit(0);
    } else if (a == "--ros-args") {
      break;   // leave ROS args to rclcpp
    } else {
      std::cerr << "unknown option: " << a << "\n";
      usage();
      return false;
    }
  }
  for (const auto & [name, pattern] : {
      std::pair<const char *, const std::string *>{"--topic", &o.topic_regex},
      std::pair<const char *, const std::string *>{"--node", &o.node_regex}})
  {
    if (pattern->empty()) {continue;}
    try {
      std::regex{*pattern};
    } catch (const std::regex_error & e) {
      std::cerr << name << ": invalid regex '" << *pattern << "': " << e.what() << "\n";
      return false;
    }
  }
  if (o.command == "diff") {
    if (o.files.size() != 2) {
      std::cerr << "diff needs two documents: transport_viz diff <before.json> <after.json>\n";
      return false;
    }
    if (o.files[0] == "-" && o.files[1] == "-") {
      std::cerr << "diff: only one of the two documents can come from stdin\n";
      return false;
    }
    for (const char * flag :
      {"--domain", "--timeout", "--quiet", "--interval", "--watch", "--list-codes"})
    {
      if (seen.count(flag)) {
        std::cerr << flag << " does not apply to diff: the documents were already observed\n";
        return false;
      }
    }
  } else {
    for (const char * flag : {"--key", "--changes-only"}) {
      if (seen.count(flag)) {
        std::cerr << flag << " applies to 'transport_viz diff' only\n";
        return false;
      }
    }
  }
  return true;
}

/// FTV_PROFILE=1 (development only, docs/development.md "Scale verification"): one JSON line
/// per phase on stderr, e.g. {"ftv_profile":"summarize","ms":12.3,"topics":512,"pairs":4870}.
class Profiler
{
public:
  using Clock = std::chrono::steady_clock;
  Profiler()
  {
    const char * env = std::getenv("FTV_PROFILE");
    enabled_ = env != nullptr && *env != '\0' && std::string(env) != "0";
  }
  bool enabled() const {return enabled_;}
  Clock::time_point now() const {return enabled_ ? Clock::now() : Clock::time_point{};}
  /// Emit `phase` timed from `since` (a now() of this profiler), with integer fields.
  void emit(
    const char * phase, Clock::time_point since,
    std::initializer_list<std::pair<const char *, uint64_t>> fields = {}) const
  {
    if (!enabled_) {return;}
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - since).count();
    std::ostringstream os;
    os << "{\"ftv_profile\":\"" << phase << "\",\"ms\":" << ms;
    for (const auto & [key, value] : fields) {
      os << ",\"" << key << "\":" << value;
    }
    os << "}\n";
    std::cerr << os.str() << std::flush;
  }

private:
  bool enabled_{false};
};

const Profiler & profiler()
{
  static const Profiler p;
  return p;
}

uint64_t line_count(const std::string & text)
{
  return static_cast<uint64_t>(std::count(text.begin(), text.end(), '\n'));
}

uint64_t count_pairs(const std::vector<fastdds_transport_viz::TopicSummary> & topics)
{
  uint64_t n = 0;
  for (const auto & t : topics) {
    n += t.pairs.size();
  }
  return n;
}

bool use_color(const Options & o)
{
  return o.color == Options::Color::Always ||
         (o.color == Options::Color::Auto && isatty(STDOUT_FILENO) &&
         std::getenv("NO_COLOR") == nullptr);
}

std::string now_iso8601()
{
  std::time_t t = std::time(nullptr);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
  return buf;
}

/// Without --all, the topics the default view hides after summarize(): native-buffer companion
/// topics folded into their parents (collect() drops services and raw DDS topics earlier).
void apply_default_view(
  std::vector<fastdds_transport_viz::TopicSummary> & topics, const Options & o)
{
  if (o.all) {return;}
  topics.erase(
    std::remove_if(
      topics.begin(), topics.end(),
      [](const fastdds_transport_viz::TopicSummary & t) {
        return !fastdds_transport_viz::in_default_view(t);
      }),
    topics.end());
}

void apply_node_filter(std::vector<fastdds_transport_viz::TopicSummary> & topics, const Options & o)
{
  if (o.node_regex.empty()) {return;}
  const std::regex re(o.node_regex);
  fastdds_transport_viz::filter_by_node(
    topics, [&re](const fastdds_transport_viz::Endpoint & e) {
      return !e.node_name.empty() && std::regex_search(e.node_name, re);
    });
}

/// Textual IPv4 / IPv6 addresses of every interface of this host.
std::set<std::string> local_ip_addresses()
{
  std::set<std::string> out;
  struct ifaddrs * ifs = nullptr;
  if (::getifaddrs(&ifs) != 0) {return out;}
  for (auto * ifa = ifs; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {continue;}
    char buf[INET6_ADDRSTRLEN] = {};
    if (ifa->ifa_addr->sa_family == AF_INET) {
      auto * sin = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
      if (::inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {out.insert(buf);}
    } else if (ifa->ifa_addr->sa_family == AF_INET6) {
      auto * sin6 = reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr);
      if (::inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf))) {out.insert(buf);}
    }
  }
  ::freeifaddrs(ifs);
  return out;
}

// Fast DDS reads ROS_SUPER_CLIENT as "TRUE" / "true" / "1" (any case)
bool env_is_true(const char * value)
{
  std::string v(value);
  std::transform(v.begin(), v.end(), v.begin(), ::tolower);
  return v == "true" || v == "1";
}

// The servers of ROS_DISCOVERY_SERVER with their host names resolved the way Fast DDS does
// (getaddrinfo, first address of the locator's family); a name that does not resolve is
// kept as text (#86)
std::vector<fastdds_transport_viz::Locator> resolve_discovery_servers(
  std::vector<fastdds_transport_viz::Locator> servers)
{
  using fastdds_transport_viz::LocatorKind;
  for (auto & l : servers) {
    const bool v6 = l.kind == LocatorKind::UDPv6 || l.kind == LocatorKind::TCPv6;
    char probe[sizeof(struct in6_addr)];
    if (::inet_pton(v6 ? AF_INET6 : AF_INET, l.address.c_str(), probe) == 1) {continue;}
    struct addrinfo hints = {};
    hints.ai_family = v6 ? AF_INET6 : AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo * res = nullptr;
    if (::getaddrinfo(l.address.c_str(), nullptr, &hints, &res) != 0 || res == nullptr) {
      continue;
    }
    char buf[INET6_ADDRSTRLEN] = {};
    if (res->ai_family == AF_INET) {
      auto * sin = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
      if (::inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {l.address = buf;}
    } else if (res->ai_family == AF_INET6) {
      auto * sin6 = reinterpret_cast<struct sockaddr_in6 *>(res->ai_addr);
      if (::inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf))) {l.address = buf;}
    }
    ::freeaddrinfo(res);
  }
  return servers;
}

// Participants of the tool itself: the rmw participant of our rclcpp node and the
// discovery/statistics participant. Matching endpoints by our node name misses the rmw
// participant where rclcpp creates no endpoint on it (Lyrical/Rolling with rosout and
// parameter services off), so ask the factory for every participant of this process.
std::set<std::string> own_participant_prefixes(int domain)
{
  std::set<std::string> prefixes;
  for (const auto * p : eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->
    lookup_participants(static_cast<eprosima::fastdds::dds::DomainId_t>(domain)))
  {
    prefixes.insert(fastdds_transport_viz::prefix_to_string(p->guid().guidPrefix));
  }
  return prefixes;
}

Snapshot collect(
  fastdds_transport_viz::DiscoveryObserver & observer,
  fastdds_transport_viz::RosGraphResolver & resolver,
  fastdds_transport_viz::RosDiscoveryInfoObserver * names,
  fastdds_transport_viz::StatsObserver * stats,
  const Options & o, int domain, double observation_seconds, const std::string & stopped_on,
  double settled_at_s = -1.0)
{
  const auto & prof = profiler();
  const auto collect_start = prof.now();
  fastdds_transport_viz::StatsData stats_data;
  if (stats != nullptr) {
    const auto t = prof.now();
    stats_data = stats->snapshot();
    prof.emit(
      "drain", t, {{"samples", stats_data.samples}, {"sample_lost", stats->samples_lost()},
        {"sample_lost_latency", stats->samples_lost_latency()},
        {"sample_lost_at_start", stats->samples_lost_at_start()},
        {"sample_rejected", stats->samples_rejected()},
        {"writers_incompatible_qos", stats->writers_incompatible_qos()},
        {"drain_errors", stats->drain_errors()}});
    stats_data.local_addresses = local_ip_addresses();
    stats_data.settled = settled_at_s >= 0.0;
    stats_data.settled_at_s = settled_at_s;
  }
  if (names != nullptr) {names->poll();}
  auto t_resolve = prof.now();
  // The rmw graph only changes with the endpoints and the ros_discovery_info samples, so a
  // quiet graph is not queried again (#135).
  const bool refreshed = resolver.refresh_if_changed(
    observer.event_count() + (names != nullptr ? names->samples_taken() : 0),
    std::chrono::duration<double>(
      std::chrono::steady_clock::now() - observer.last_event()).count());
  prof.emit("resolve", t_resolve, {{"refreshed", static_cast<uint64_t>(refreshed)}});

  std::vector<Endpoint> endpoints = observer.snapshot();
  // Was discovery still running when the wait ended (#74 saw one-shot runs print a fraction
  // of a large system)? Judged on the raw snapshot, before any filter drops an endpoint the
  // nodes did announce.
  fastdds_transport_viz::DiscoveryStatus discovery =
    fastdds_transport_viz::discovery_completeness(
    names != nullptr ? names->table().announced_by_participant() :
    std::map<fastdds_transport_viz::ParticipantPrefix,
    std::vector<fastdds_transport_viz::EndpointGid>>{},
    observer.live_participants(), endpoints);
  discovery.stopped_on = stopped_on;
  discovery.events = observer.event_count();
  // how our own participants take part in discovery, from the environment Fast DDS reads
  // (rmw_fastrtps sets neither option itself) (#86)
  if (const char * ds = std::getenv("ROS_DISCOVERY_SERVER"); ds != nullptr && *ds != '\0') {
    discovery.discovery_server_env = ds;
    discovery.discovery_servers = resolve_discovery_servers(
      fastdds_transport_viz::parse_discovery_server_env(ds));
    const char * sc = std::getenv("ROS_SUPER_CLIENT");
    discovery.observer_protocol =
      sc != nullptr && env_is_true(sc) ? "SUPER_CLIENT" : "CLIENT";
  }
  if (const char * em = std::getenv("ROS2_EASY_MODE"); em != nullptr && *em != '\0') {
    discovery.easy_mode = em;
    discovery.observer_protocol = "SUPER_CLIENT";
  }
  // before any filter, so that a parent keeps its companions whatever --topic / --all drop
  fastdds_transport_viz::link_buffer_companions(endpoints);
  std::vector<Endpoint> kept;
  std::regex re;
  if (!o.topic_regex.empty()) {
    re = std::regex(o.topic_regex);
  }
  fastdds_transport_viz::ShmScanInput shm_in;   // SHM ports of every endpoint, filtered or not
  const auto local_host = observer.local_host_id();
  std::set<std::string> own_prefixes = own_participant_prefixes(domain);
  // #201: how many pairs could show a packet at all, from the same raw snapshot the settle
  // rule used. 0 means every delivery in the system is intra-process.
  if (stats != nullptr) {
    stats_data.measurable_pairs =
      fastdds_transport_viz::count_measurable_pairs(endpoints, own_prefixes);
  }
  std::set<std::string> other_host_prefixes;
  // SHM ports per participant with our host id, from every endpoint (ros_discovery_info
  // is the only one announcing the 7000+ port on Jazzy and newer), and those announced by
  // an endpoint other than the ros_discovery_info reader: that reader's unique-flow port is
  // the first free 7000+ number of its IPC namespace in any domain, so a held lock of it
  // does not prove whose it is (#118)
  std::map<std::string, std::set<uint32_t>> shm_ports_by_participant;
  std::map<std::string, std::set<uint32_t>> shm_proof_ports_by_participant;
  // every discovered participant, before any view filter: the `participants` of the JSON
  std::map<std::string, fastdds_transport_viz::Participant> participants;
  std::map<std::string, std::set<uint32_t>> participant_ports;   // ... the tool's own included
  // The lock files of the ports our participants listen on, including those no endpoint
  // announces (the discovery participant has none), so a node port that collides with
  // one of them in another IPC namespace is not taken for visible.
  if (const auto held = fastdds_transport_viz::held_port_locks()) {
    shm_in.own_ports = *held;
  }
  // Every live participant first, so the ones without endpoints (Discovery Servers) are in
  // the report too (#86); the endpoints below add what only they carry.
  for (const auto & kv : observer.participant_data()) {
    auto & participant = participants[kv.first];
    participant.guid_prefix = kv.first;
    participant.host_id = kv.second.host_id;
    participant.own = own_prefixes.count(kv.first) > 0;
    participant.discovery_protocol = kv.second.discovery_protocol;
    participant.name = kv.second.name;
    participant.vendor = kv.second.vendor;
    participant.metatraffic_locators = kv.second.metatraffic_locators;
    // not counted among the other-host participants of the SHM verdict: that count is
    // about endpoints whose traffic SHM cannot carry, and a server has none
  }
  for (auto & e : endpoints) {
    e.node_name = fastdds_transport_viz::merge_node_name(
      resolver.node_for_guid(e.guid_bytes),
      names != nullptr ? names->node_for_guid(e.guid_bytes) : std::string());
    auto phys = stats_data.physical.find(e.participant_guid_prefix);
    if (phys != stats_data.physical.end()) {
      e.host_name = phys->second.host;
      e.process = phys->second.process;
    }
    if (e.is_writer && e.dds_topic.rfind("_fastdds_statistics_", 0) == 0) {
      // which statistics topics the participant publishes (DATA_COUNT is only sent on
      // change, so its mere presence matters)
      stats_data.statistics_writers.insert({e.participant_guid_prefix, e.dds_topic});
    }
    const bool ours = own_prefixes.count(e.participant_guid_prefix) > 0;
    auto & participant = participants[e.participant_guid_prefix];
    participant.guid_prefix = e.participant_guid_prefix;
    participant.host_id = e.host_id;
    participant.host_name = e.host_name;
    participant.own = ours;
    for (const auto & l : e.unicast) {
      if (l.kind != fastdds_transport_viz::LocatorKind::SHM) {continue;}
      if (e.host_id == local_host) {
        participant_ports[e.participant_guid_prefix].insert(l.port);
      }
      if (ours) {
        shm_in.own_ports.insert(l.port);
      } else if (e.host_id == local_host) {
        shm_in.node_ports.insert(l.port);
        shm_ports_by_participant[e.participant_guid_prefix].insert(l.port);
        if (e.is_writer || e.dds_topic != "ros_discovery_info") {
          shm_proof_ports_by_participant[e.participant_guid_prefix].insert(l.port);
        }
      }
    }
    if (ours) {
      continue;
    }
    if (e.host_id != local_host) {other_host_prefixes.insert(e.participant_guid_prefix);}
    if (!o.all) {
      // Default view: ROS topics only. Services (rq/rr) and raw DDS topics need --all.
      if (e.ros_topic.empty() || e.dds_topic.rfind("rt/", 0) != 0) {
        continue;
      }
    }
    if (e.dds_topic == "ros_discovery_info" || e.dds_topic.rfind("_fastdds_", 0) == 0) {
      continue;
    }
    if (!o.topic_regex.empty()) {
      const std::string & name = e.ros_topic.empty() ? e.dds_topic : e.ros_topic;
      if (!std::regex_search(name, re)) {
        continue;
      }
    }
    kept.push_back(std::move(e));
  }

  Snapshot snap;
  snap.domain = domain;
  snap.observed_at = now_iso8601();
  snap.observation_seconds = observation_seconds;
  snap.local_host_id = observer.local_host_id();
  snap.discovery = std::move(discovery);
  snap.endpoints = std::move(kept);
  {
    // Shared memory of this environment; data-sharing files are attributed to the
    // discovered writers (histories) and readers (notification segments) by name.
    for (const auto & e : snap.endpoints) {
      auto & by_name = e.is_writer ? shm_in.datasharing_writers : shm_in.datasharing_readers;
      by_name[fastdds_transport_viz::datasharing_segment_name(e.guid_bytes)] = e.guid;
    }
    shm_in.other_host_participants = other_host_prefixes.size();
    snap.shm = fastdds_transport_viz::scan_shm(fastdds_transport_viz::kDefaultShmDir, shm_in);
    std::map<uint32_t, size_t> participants_per_port;
    for (const auto & kv : shm_ports_by_participant) {
      for (uint32_t port : kv.second) {
        ++participants_per_port[port];
      }
    }
    for (auto & e : snap.endpoints) {
      auto it = snap.shm.datasharing_by_writer.find(e.guid);
      if (it != snap.shm.datasharing_by_writer.end()) {
        e.datasharing_history_available = true;
        e.datasharing_history_bytes = it->second;
      }
      if (snap.shm.listed && e.host_id == local_host &&
        e.qos.data_sharing != fastdds_transport_viz::DataSharingKind::Off)
      {
        // Fast DDS creates the segment with the endpoint whenever data-sharing is enabled
        // (ON or AUTO); decide() only looks at endpoints that announce it, and the JSON
        // reports `unprobed` for the rest so that `not-visible` means a missing segment (#163).
        const bool here = e.is_writer ? e.datasharing_history_available :
          snap.shm.datasharing_notification_readers.count(e.guid) > 0;
        e.datasharing_segment_visibility = here ?
          fastdds_transport_viz::ShmVisibility::Visible :
          fastdds_transport_viz::ShmVisibility::NotVisible;
      }
      auto ports = shm_ports_by_participant.find(e.participant_guid_prefix);
      if (ports != shm_ports_by_participant.end()) {
        e.participant_shm_ports = ports->second;
        e.participant_shm_visibility = fastdds_transport_viz::participant_shm_visibility(
          ports->second, shm_proof_ports_by_participant[e.participant_guid_prefix], snap.shm,
          participants_per_port);
      }
    }
    // The same inputs per participant, for the report (#125). `announced_by` counts every
    // discovered participant, the tool's own included; the verdict above counts nodes only,
    // which never differs where it matters (a node port that is the tool's own is missing).
    std::map<uint32_t, size_t> announcing;
    for (const auto & kv : participant_ports) {
      for (uint32_t port : kv.second) {
        ++announcing[port];
      }
    }
    for (auto & kv : participants) {
      auto & p = kv.second;
      auto ports = participant_ports.find(p.guid_prefix);
      if (ports == participant_ports.end()) {continue;}
      const auto & proof = shm_proof_ports_by_participant[p.guid_prefix];
      for (uint32_t port : ports->second) {
        fastdds_transport_viz::Participant::ShmPort sp;
        sp.port = port;
        auto lock = snap.shm.port_locks.find(port);
        sp.lock = lock != snap.shm.port_locks.end() ? lock->second :
          p.own && snap.shm.listed ? fastdds_transport_viz::PortLock::Own :
          fastdds_transport_viz::PortLock::Unprobed;
        sp.announced_by = announcing[port];
        sp.proof = !p.own && proof.count(port) > 0;
        p.shm_ports.push_back(sp);
      }
      if (!p.own) {
        p.shm_visibility = fastdds_transport_viz::participant_shm_visibility(
          ports->second, proof, snap.shm, participants_per_port);
      }
    }
    // A participant without endpoints (a Discovery Server) has no PHYSICAL_DATA of its
    // own; it takes the host name the other participants of its host id report, so it
    // lands in their column.
    std::map<fastdds_transport_viz::HostId, std::string> host_names;
    for (const auto & kv : participants) {
      if (!kv.second.host_name.empty()) {
        host_names.emplace(kv.second.host_id, kv.second.host_name);
      }
    }
    for (auto & kv : participants) {
      auto & p = kv.second;
      if (p.host_name.empty()) {
        auto it = host_names.find(p.host_id);
        if (it != host_names.end()) {p.host_name = it->second;}
      }
      snap.participants.push_back(std::move(p));
    }
    fastdds_transport_viz::attribute_discovery_servers(
      snap.participants, !snap.discovery.easy_mode.empty());
  }
  auto t = prof.now();
  snap.topics = fastdds_transport_viz::summarize(snap.endpoints);
  prof.emit(
    "summarize", t, {{"endpoints", snap.endpoints.size()}, {"topics", snap.topics.size()},
      {"pairs", count_pairs(snap.topics)}});
  apply_default_view(snap.topics, o);
  apply_node_filter(snap.topics, o);
  snap.stats = std::move(stats_data);
  t = prof.now();
  fastdds_transport_viz::apply_stats(snap.topics, snap.stats);
  fastdds_transport_viz::note_unmeasured_pairs(snap.topics, snap.stats);
  prof.emit(
    "apply_stats", t, {{"pairs_delivered", snap.stats.pairs_delivered},
      {"pairs_delivered_unmeasured", snap.stats.pairs_delivered_unmeasured},
      {"pairs_delivered_absent", snap.stats.pairs_delivered_absent}});
  prof.emit(
    "collect", collect_start, {{"endpoints", snap.endpoints.size()},
      {"topics", snap.topics.size()}, {"pairs", count_pairs(snap.topics)}});
  return snap;
}

/// #133: the wait ends on a window without discovery events, which a busy system produces
/// while its nodes are still announcing their endpoints. The table then looks normal but is
/// short of pairs, so say so in one line on stderr. Nothing is printed when the observation's
/// completeness could not be judged (no `ros_discovery_info` sample).
void warn_if_incomplete(const Snapshot & snap, const Options & o)
{
  const std::string line = fastdds_transport_viz::incomplete_discovery_warning(
    snap.discovery, o.quiet, o.timeout, o.stats);
  if (!line.empty()) {std::cerr << line << "\n";}
}

/// #134: statistics samples the tool never received are measurements that never come back, so
/// a one-shot run says on stderr how many it lost. `--watch` does not get this line: the count
/// only grows from frame to frame and the table's statistics footer already carries it.
void warn_if_statistics_lost(const Snapshot & snap)
{
  const std::string line = fastdds_transport_viz::statistics_loss_warning(snap.stats);
  if (!line.empty()) {std::cerr << line << "\n";}
  const std::string absent = fastdds_transport_viz::rtps_sent_absent_warning(snap.stats);
  if (!absent.empty()) {std::cerr << absent << "\n";}
}

/// Raw-mode keyboard input and alternate screen for --watch on a terminal.
class Terminal
{
public:
  explicit Terminal(bool enable)
  : enabled_(enable && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))
  {
    if (!enabled_) {return;}
    tcgetattr(STDIN_FILENO, &saved_);
    termios raw = saved_;
    // No line buffering, no echo, and no signal generation: Ctrl-C arrives as key 3 and
    // ends the loop like 'q' (an orderly shutdown instead of a signal racing with it).
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    std::cout << "\033[?1049h\033[?25l" << std::flush;   // alternate screen, hide cursor
  }
  ~Terminal()
  {
    if (!enabled_) {return;}
    std::cout << "\033[?25h\033[?1049l" << std::flush;
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
  }
  bool enabled() const {return enabled_;}

  /// Wait up to timeout_ms for a key; returns 0 when none.
  char read_key(int timeout_ms)
  {
    if (!enabled_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
      return 0;
    }
    pollfd fd{STDIN_FILENO, POLLIN, 0};
    if (poll(&fd, 1, timeout_ms) > 0) {
      char c = 0;
      if (read(STDIN_FILENO, &c, 1) == 1) {return c;}
    }
    return 0;
  }

  void size(size_t & rows, size_t & cols) const
  {
    winsize ws{};
    if (enabled_ && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
      rows = ws.ws_row;
      cols = ws.ws_col;
    } else {
      rows = cols = 0;
    }
  }

  /// Paint a frame in place: home the cursor, overwrite line by line, clear the rest.
  void paint(const std::string & frame, size_t max_rows)
  {
    std::ostringstream os;
    os << "\033[H";
    std::istringstream in(frame);
    std::string line;
    size_t n = 0;
    while (std::getline(in, line) && (max_rows == 0 || n < max_rows)) {
      os << line << "\033[K\n";
      ++n;
    }
    os << "\033[J";
    std::cout << os.str() << std::flush;
  }

private:
  bool enabled_;
  termios saved_{};
};

/// The view options of a one-shot run applied to a loaded document: the default view keeps
/// ROS topics only (services, raw DDS topics and folded native-buffer companion topics need
/// --all, as when observing), then the --topic and --node filters.
void apply_view_filters(Snapshot & snap, const Options & o)
{
  std::regex topic_re;
  if (!o.topic_regex.empty()) {topic_re = std::regex(o.topic_regex);}
  snap.topics.erase(
    std::remove_if(
      snap.topics.begin(), snap.topics.end(),
      [&](const fastdds_transport_viz::TopicSummary & t) {
        if (!o.all && !fastdds_transport_viz::in_default_view(t)) {return true;}
        return !o.topic_regex.empty() && !std::regex_search(t.display_topic, topic_re);
      }),
    snap.topics.end());
  apply_node_filter(snap.topics, o);
}

/// `transport_viz diff <before> <after>`: no participant, no RMW; exit 0 when the two
/// documents show the same pairs and transports, 1 when something changed, 2 on an error.
int run_diff(const Options & o)
{
  RenderOptions ropt;
  ropt.verbose = o.verbose;
  ropt.explain = o.explain;
  ropt.locators = o.locators;
  ropt.advise = o.advise;
  ropt.color = use_color(o);

  auto load = [](const std::string & path, Snapshot & out) {
      std::string text;
      if (path == "-") {
        text.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
      } else {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
          std::cerr << "transport_viz diff: cannot read " << path << ": " << std::strerror(errno)
                    << "\n";
          return false;
        }
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
      }
      size_t documents = 0;
      try {
        out = fastdds_transport_viz::parse_json(text, &documents);
      } catch (const fastdds_transport_viz::ParseError & e) {
        std::cerr << "transport_viz diff: " << path << ": " << e.what() << "\n";
        return false;
      }
      if (documents > 1) {
        std::cerr << "transport_viz diff: " << path << ": " << documents
                  << " documents (JSON Lines), comparing the last one\n";
      }
      return true;
    };
  Snapshot before, after;
  if (!load(o.files[0], before) || !load(o.files[1], after)) {
    return 2;
  }
  if (before.domain != after.domain) {
    std::cerr << "transport_viz diff: warning: the documents are from different domains ("
              << before.domain << " and " << after.domain << ")\n";
  }
  apply_view_filters(before, o);
  apply_view_filters(after, o);

  fastdds_transport_viz::Changes changes =
    fastdds_transport_viz::diff_snapshots(before, after, o.key);
  changes.before = fastdds_transport_viz::SnapshotRef{before.observed_at, before.domain};
  const bool differ = !changes.empty();
  after.has_changes = true;
  after.changes = changes;
  WatchDecorations deco = fastdds_transport_viz::decorations_for(changes, before, ropt);
  if (o.changes_only) {
    fastdds_transport_viz::keep_changed_topics(after, deco);
  }
  if (o.json) {
    std::cout << fastdds_transport_viz::render_json(after, ropt) << std::flush;
  } else {
    ropt.watch = &deco;
    std::cout << fastdds_transport_viz::render_table(after, ropt) << std::flush;
  }
  return differ ? 1 : 0;
}

}  // namespace

int main(int argc, char ** argv)
{
  Options o;
  if (!parse(argc, argv, o)) {
    return 2;
  }
  if (o.command == "diff") {
    return run_diff(o);
  }
  if (o.list_codes) {
    for (const auto & c : fastdds_transport_viz::known_codes()) {
      std::cout << c << "\n    " << fastdds_transport_viz::explain(c) << "\n";
      if (auto r = fastdds_transport_viz::remedy(c)) {
        std::cout << "    fix: " << *r << "\n";
      }
    }
    return 0;
  }

  // The tool only makes sense on rmw_fastrtps_cpp: on another middleware the rclcpp node
  // (name resolution) would run elsewhere while the raw Fast DDS participant still sees
  // whatever Fast DDS nodes exist. Ask the RMW layer itself, before anything is created;
  // RMW_IMPLEMENTATION unset resolves to the distro's default. (An RMW that cannot be
  // loaded at all is normally stopped earlier, by rcl's load-time check; the nullptr
  // branch is kept for an rcl without it.)
  {
    const char * id = rmw_get_implementation_identifier();
    std::string load_error;
    if (id == nullptr) {
      load_error = rmw_get_error_string().str;
      rmw_reset_error();
    }
    const char * requested = std::getenv("RMW_IMPLEMENTATION");
    const auto rv = fastdds_transport_viz::rmw_verdict(
      id ? std::optional<std::string>(id) : std::nullopt, requested ? requested : "",
      load_error);
    if (rv.kind == fastdds_transport_viz::RmwVerdictKind::Reject) {
      std::cerr << "transport_viz: " << rv.message << "\n";
      return 1;
    }
  }

  if (o.timeout < 0) {
    o.timeout = o.stats ? 30.0 : 3.0;
  }
  // The tool is meant to run in the observed nodes' environment, which may carry
  // FASTDDS_STATISTICS. Fast DDS would then add statistics DataWriters to our own two
  // participants as well; on the discovery participant, which also hosts the statistics
  // readers, that deadlocks inside Fast DDS 2.14 (on_rtps_sent() -> statistics
  // DataWriter::write() while a reader sends its acknack). We never need statistics about
  // ourselves, so drop the variable before any participant is created.
  ::unsetenv("FASTDDS_STATISTICS");
  if (o.stats && !FTV_HAS_STATISTICS) {
    std::cerr << "warning: this Fast DDS was built without the statistics module (ROS 2 Humble's "
      "2.6 binary): the observed nodes cannot publish statistics and --stats measures nothing\n";
  }
  int domain = o.domain;
  if (domain < 0) {
    const char * env = std::getenv("ROS_DOMAIN_ID");
    domain = env ? std::atoi(env) : 0;
  }

  rclcpp::InitOptions init_opts;
  init_opts.set_domain_id(static_cast<size_t>(domain));
  // Discovery Server: a plain CLIENT only learns about the endpoints it matches, so an
  // observer must be a SUPER_CLIENT (Fast DDS reads ROS_SUPER_CLIENT for both our
  // participants). An explicit ROS_SUPER_CLIENT is respected.
  if (const char * ds = std::getenv("ROS_DISCOVERY_SERVER"); ds != nullptr && *ds != '\0') {
    if (std::getenv("ROS_SUPER_CLIENT") == nullptr) {
      setenv("ROS_SUPER_CLIENT", "TRUE", 1);
      std::cerr << "ROS_DISCOVERY_SERVER is set: observing as SUPER_CLIENT "
        "(set ROS_SUPER_CLIENT to override)\n";
    }
  }
  // Easy Mode (Fast DDS 3.2+): the nodes are SUPER_CLIENTs of a Discovery Server that Fast
  // DDS spawns per host; our participants get the same treatment from the environment.
  const char * easy_mode = std::getenv("ROS2_EASY_MODE");
  const bool easy_mode_on = easy_mode != nullptr && *easy_mode != '\0';
  if (easy_mode_on) {
    std::cerr << "ROS2_EASY_MODE=" << easy_mode
              << ": observing through this host's Discovery Server (Easy Mode)\n";
  }
  if (const char * range = std::getenv("ROS_AUTOMATIC_DISCOVERY_RANGE");
    range != nullptr && std::string(range) == "OFF")
  {
    std::cerr << "warning: ROS_AUTOMATIC_DISCOVERY_RANGE=OFF disables discovery entirely "
      "(rmw_fastrtps allows a single participant); nothing can be observed in this mode\n";
  }
  rclcpp::init(argc, argv, init_opts);
  int rc = 0;
  {
    rclcpp::NodeOptions node_opts;
    node_opts.start_parameter_services(false)
    .start_parameter_event_publisher(false)
    .enable_rosout(false)
    .parameter_overrides({rclcpp::Parameter("start_type_description_service", false)});
    // In Easy Mode Fast DDS runs `fastdds discovery auto` for every participant it creates,
    // and that CLI reports on stdout ("The Fast DDS daemon is already running." ...), which
    // would corrupt --json: route stdout to stderr while the participants come up.
    StdoutToStderr cli_noise(easy_mode_on);
    auto node = std::make_shared<rclcpp::Node>(
      "_transport_viz_" + std::to_string(getpid()), node_opts);
    fastdds_transport_viz::RosGraphResolver resolver(node);
    fastdds_transport_viz::DiscoveryObserver observer(domain);
    // node names the rclcpp participant misses (a same-host node in another IPC namespace
    // sends its ros_discovery_info over SHM into its own /dev/shm)
    std::unique_ptr<fastdds_transport_viz::RosDiscoveryInfoObserver> names;
    try {
      names = std::make_unique<fastdds_transport_viz::RosDiscoveryInfoObserver>(
        observer.participant());
    } catch (const std::exception & e) {
      std::cerr << "warning: " << e.what() << "; node names come from the ROS graph only\n";
    }
    std::unique_ptr<fastdds_transport_viz::StatsObserver> stats;
    if (o.stats) {
      stats = std::make_unique<fastdds_transport_viz::StatsObserver>(observer.participant());
      stats->set_rate_window(o.watch);   // --watch: last kStatsRateWindowSeconds only
    }
    cli_noise.restore();

    RenderOptions ropt;
    ropt.verbose = o.verbose;
    ropt.explain = o.explain;
    ropt.locators = o.locators;
    ropt.advise = o.advise;
    ropt.compact = o.json && o.watch;   // JSON Lines: one document per line
    ropt.color = use_color(o);

    const auto start = std::chrono::steady_clock::now();
    double first_event_s = -1.0;
    std::string stopped_on = "timeout";
    double settled_at_s = -1.0;
    size_t measured_instances = 0;
    double measured_changed_s = 0.0;
    // #179: the settle rule counts instances towards discovered readers only; the set is
    // rebuilt when discovery delivered something since the last check, not every 50 ms
    uint64_t reader_destinations_event_count = 0;
    // #201: pairs whose two ends are in different processes, from the same snapshot. An
    // intra-process-only system (or one without readers) publishes no RTPS_SENT at all, and
    // waiting for one would burn --timeout on every run.
    size_t measurable_pairs = 0;
    std::set<std::string> own_prefixes;
    // Wait until --timeout, or until discovery has been quiet for --quiet
    // seconds (but never less than --quiet seconds in total).
    for (;; ) {
      // The statistics readers are drained by the observer's own thread (#141).
      if (names) {names->poll();}
      std::this_thread::sleep_for(50ms);
      auto now = std::chrono::steady_clock::now();
      double elapsed = std::chrono::duration<double>(now - start).count();
      if (first_event_s < 0 && observer.event_count() > 0) {first_event_s = elapsed;}
      double since_last = std::chrono::duration<double>(now - observer.last_event()).count();
      if (elapsed >= o.timeout) {break;}
      // Quiet only counts once something was discovered: on a busy host the first participant
      // announcement can take longer than --quiet, which printed an empty table (#74).
      const bool quiet_now = o.quiet > 0 && elapsed >= o.quiet && since_last >= o.quiet &&
        observer.event_count() > 0;
      if (!o.stats && quiet_now) {
        stopped_on = "quiet";
        break;
      }
      // --watch --stats: the first frame needs a counter window, not the settle rule below,
      // which is for one-shot output - every second it waits is a frame not drawn (#177).
      if (o.watch && quiet_now && fastdds_transport_viz::watch_ready(true, elapsed, o.stats)) {
        stopped_on = "quiet";
        break;
      }
      // With --stats quiet is not enough: the counters need a window, and the statistics
      // writers hand their transient-local history to a late-joining reader one at a time,
      // which at 20 processes takes about 20 s (#168). The run ends once every matched
      // RTPS_SENT writer has been heard from and the measured instances they hand over have
      // stopped growing for --quiet (at least kStatsMeasuredQuietSeconds), and --timeout is
      // the cap.
      if (o.stats && !o.watch && o.quiet > 0 &&
        elapsed >= fastdds_transport_viz::kStatsSettleMinSeconds)
      {
        if (observer.event_count() != reader_destinations_event_count) {
          reader_destinations_event_count = observer.event_count();
          if (own_prefixes.empty()) {own_prefixes = own_participant_prefixes(domain);}
          const auto snapshot = observer.snapshot();
          stats->set_reader_destinations(
            fastdds_transport_viz::reader_destinations(snapshot, own_prefixes));
          measurable_pairs =
            fastdds_transport_viz::count_measurable_pairs(snapshot, own_prefixes);
        }
        const auto settle = stats->settle_status();
        if (settle.measured_instances != measured_instances) {
          measured_instances = settle.measured_instances;
          measured_changed_s = elapsed;
        }
        if (quiet_now && fastdds_transport_viz::stats_settled(
            true, elapsed, settle.announced, settle.heard, measurable_pairs,
            settle.measured_instances, elapsed - measured_changed_s,
            std::max(o.quiet, fastdds_transport_viz::kStatsMeasuredQuietSeconds)))
        {
          stopped_on = "settled";
          settled_at_s = elapsed;
          break;
        }
      }
      if (!rclcpp::ok()) {break;}
    }
    // #168: the cap was hit with RTPS_SENT writers still to hear from, which is the state the
    // settle rule was meant to end - say which, so a longer --timeout is an informed choice.
    if (o.stats && stopped_on == "timeout" && !o.watch) {
      const auto settle = stats->settle_status();
      // #179: ... or with every writer heard from and still no instance towards a reader.
      // Recomputed here rather than taken from the loop, which never runs below --timeout 5
      // (#201): with nothing measurable there is no RTPS_SENT entry to miss.
      if (own_prefixes.empty()) {own_prefixes = own_participant_prefixes(domain);}
      const bool measurable = fastdds_transport_viz::count_measurable_pairs(
        observer.snapshot(), own_prefixes) > 0;
      const bool unmeasured =
        measurable && settle.announced > 0 && settle.measured_instances == 0;
      if (!settle.unheard.empty() || unmeasured) {
        std::cerr << "warning: --timeout " << o.timeout << " ended the run with ";
        if (!settle.unheard.empty()) {
          std::cerr << settle.unheard.size() << " of " << settle.announced
                    << " statistics RTPS_SENT writers not heard from";
          const size_t shown = std::min<size_t>(settle.unheard.size(), 5);
          for (size_t i = 0; i < shown; ++i) {
            std::cerr << (i == 0 ? ": " : ", ") << settle.unheard[i];
          }
          if (settle.unheard.size() > shown) {std::cerr << ", ...";}
        }
        if (!settle.unheard.empty() && unmeasured) {std::cerr << " and ";}
        if (unmeasured) {std::cerr << "no measured RTPS_SENT entry to a discovered reader";}
        std::cerr << "; their pairs may read (idle) - pass a longer --timeout\n";
      }
    }

    const auto & prof = profiler();
    if (prof.enabled()) {
      const auto own = observer.snapshot().size();
      // first_event_ms is polled every 50 ms (0: nothing discovered); last_event_ms is exact.
      const auto last_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        observer.last_event() - start).count();
      prof.emit(
        "discovery", start,
        {{"endpoints", own}, {"events", observer.event_count()},
          {"first_event_ms", static_cast<uint64_t>(std::max(first_event_s, 0.0) * 1000.0)},
          {"last_event_ms", static_cast<uint64_t>(std::max<int64_t>(last_ms, 0))}});
    }
    if (!o.watch) {
      double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      Snapshot snap = collect(
        observer, resolver, names.get(), stats.get(), o, domain, elapsed, stopped_on,
        settled_at_s);
      const auto t = prof.now();
      const std::string out = o.json ? fastdds_transport_viz::render_json(snap, ropt) :
        fastdds_transport_viz::render_table(snap, ropt);
      prof.emit("render", t, {{"bytes", out.size()}, {"lines", line_count(out)}});
      std::cout << out << std::flush;
      warn_if_incomplete(snap, o);   // after the table: the last line stays in sight
      warn_if_statistics_lost(snap);
    } else {
      Terminal term(!o.json);
      fastdds_transport_viz::WatchState ws;
      bool first_frame = true;
      bool paused = false;
      bool quit = false;
      bool force = true;
      auto next_frame = std::chrono::steady_clock::now();
      while (rclcpp::ok() && !quit) {
        if (force || (!paused && std::chrono::steady_clock::now() >= next_frame)) {
          force = false;
          const auto frame_start = prof.now();
          next_frame = std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(o.interval));
          double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
          Snapshot snap =
            collect(observer, resolver, names.get(), stats.get(), o, domain, elapsed, stopped_on);
          ropt.verbose = o.verbose;
          ropt.explain = o.explain;
          ropt.locators = o.locators;
          ropt.advise = o.advise;
          // Ctrl-C during a slow collect: leave without painting a frame nobody waits for
          if (!rclcpp::ok()) {break;}
          auto t = prof.now();
          ws.diff(snap, ropt);
          prof.emit("update", t);
          t = prof.now();
          if (o.json) {
            const std::string out = fastdds_transport_viz::render_json(snap, ropt);
            prof.emit("render", t, {{"bytes", out.size()}, {"lines", line_count(out)}});
            std::cout << out << std::flush;
          } else {
            size_t rows = 0, cols = 0;
            term.size(rows, cols);
            ropt.max_width = cols;
            ropt.watch = &ws.deco;
            std::ostringstream header;
            header << "transport_viz  domain " << domain << "  " << snap.observed_at
                   << "  refresh " << o.interval << "s" << (paused ? "  [PAUSED]" : "")
                   << (o.all ? "  [all]" : "")
                   << (o.topic_regex.empty() ? "" : "  [topic: " + o.topic_regex + "]")
                   << (o.node_regex.empty() ? "" : "  [node: " + o.node_regex + "]");
            std::ostringstream frame;
            frame << fastdds_transport_viz::truncate_visible(header.str(), cols) << "\n\n";
            frame << fastdds_transport_viz::render_table(snap, ropt);
            if (prof.enabled()) {
              const std::string text = frame.str();
              prof.emit("render", t, {{"bytes", text.size()}, {"lines", line_count(text)}});
            }
            if (term.enabled()) {
              const std::string keys = std::string(" q quit   p ") +
                (paused ? "resume" : "pause") +
                "   v pairs   e legend   a all   l locators   f fixes";
              frame << "\n" << fastdds_transport_viz::truncate_visible(keys, cols) << "\n";
              term.paint(frame.str(), rows);
            } else {
              std::cout << frame.str() << "\n" << std::flush;
            }
          }
          prof.emit("frame", frame_start, {{"pairs", count_pairs(snap.topics)}});
          if (first_frame) {
            first_frame = false;
            // Only the first frame: later ones fill the view in by themselves, and the line
            // would scroll the terminal away. On an alternate screen the next repaint would
            // overwrite it anyway, so it is left to the JSON `discovery` object there.
            if (!term.enabled()) {warn_if_incomplete(snap, o);}
          }
          ws.keep(std::move(snap));
        }
        char key = term.read_key(50);
        switch (key) {
          case 'q': case 'Q': case 3: quit = true; break;     // 3 = Ctrl-C in raw mode
          case 'p': case 'P': paused = !paused; force = true; break;
          case 'v': case 'V': o.verbose = !o.verbose; force = true; break;
          case 'e': case 'E': o.explain = !o.explain; force = true; break;
          case 'a': case 'A': o.all = !o.all; force = true; break;
          case 'l': case 'L':
            o.locators = !o.locators;
            if (o.locators) {o.verbose = true;}
            force = true;
            break;
          case 'f': case 'F':
            o.advise = !o.advise;
            if (o.advise) {o.verbose = true; o.explain = true;}
            force = true;
            break;
          default: break;
        }
      }
    }
  }
  rclcpp::shutdown();
  return rc;
}
