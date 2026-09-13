// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// transport_viz: show which Fast DDS transport each ROS 2 topic uses and why.

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
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
  double timeout{-1.0};       // seconds to wait for discovery (-1: 3, or 5 with --stats)
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
    "  --timeout <sec>    max time to wait for discovery (default: 3, 5 with --stats)\n"
    "  --quiet <sec>      stop early after this many seconds without discovery events\n"
    "                     (default: 1; ignored with --stats)\n"
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
    "HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC;"
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

Snapshot collect(
  fastdds_transport_viz::DiscoveryObserver & observer,
  fastdds_transport_viz::RosGraphResolver & resolver,
  fastdds_transport_viz::StatsObserver * stats,
  const Options & o, int domain, double observation_seconds)
{
  fastdds_transport_viz::StatsData stats_data;
  if (stats != nullptr) {
    stats_data = stats->snapshot();
    stats_data.local_addresses = local_ip_addresses();
  }
  resolver.refresh();

  std::vector<Endpoint> endpoints = observer.snapshot();
  std::vector<Endpoint> kept;
  std::regex re;
  if (!o.topic_regex.empty()) {
    re = std::regex(o.topic_regex);
  }
  fastdds_transport_viz::ShmScanInput shm_in;   // SHM ports of every endpoint, filtered or not
  const auto local_host = observer.local_host_id();
  // Participants of the tool itself: the rmw participant of our rclcpp node and the
  // discovery/statistics participant. Matching endpoints by our node name misses the rmw
  // participant where rclcpp creates no endpoint on it (Lyrical/Rolling with rosout and
  // parameter services off), so ask the factory for every participant of this process.
  std::set<std::string> own_prefixes;
  std::set<std::string> other_host_prefixes;
  for (const auto * p : eprosima::fastdds::dds::DomainParticipantFactory::get_instance()->
    lookup_participants(static_cast<eprosima::fastdds::dds::DomainId_t>(domain)))
  {
    own_prefixes.insert(fastdds_transport_viz::prefix_to_string(p->guid().guidPrefix));
  }
  // The lock files of the ports our participants listen on, including those no endpoint
  // announces (the discovery participant has none), so a node port that collides with
  // one of them in another IPC namespace is not taken for visible.
  if (const auto held = fastdds_transport_viz::held_port_locks()) {
    shm_in.own_ports = *held;
  }
  for (auto & e : endpoints) {
    e.node_name = resolver.node_for_guid(e.guid_bytes);
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
    for (const auto & l : e.unicast) {
      if (l.kind != fastdds_transport_viz::LocatorKind::SHM) {continue;}
      if (ours) {
        shm_in.own_ports.insert(l.port);
      } else if (e.host_id == local_host) {
        shm_in.node_ports.insert(l.port);
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
  snap.endpoints = std::move(kept);
  {
    // Shared memory of this environment; data-sharing history files are attributed to
    // the discovered writers by name.
    for (const auto & e : snap.endpoints) {
      if (e.is_writer) {
        shm_in.datasharing_writers[
          fastdds_transport_viz::datasharing_segment_name(e.guid_bytes)] = e.guid;
      }
    }
    shm_in.other_host_participants = other_host_prefixes.size();
    snap.shm = fastdds_transport_viz::scan_shm(fastdds_transport_viz::kDefaultShmDir, shm_in);
    for (auto & e : snap.endpoints) {
      auto it = snap.shm.datasharing_by_writer.find(e.guid);
      if (it != snap.shm.datasharing_by_writer.end()) {
        e.datasharing_history_available = true;
        e.datasharing_history_bytes = it->second;
      }
    }
  }
  snap.topics = fastdds_transport_viz::summarize(snap.endpoints);
  apply_node_filter(snap.topics, o);
  snap.stats = std::move(stats_data);
  fastdds_transport_viz::apply_stats(snap.topics, snap.stats);
  return snap;
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

/// Frame-to-frame highlight state for --watch.
struct WatchState
{
  static constexpr int kHoldFrames = 3;
  std::map<PairKey, PairState> last_rendered;
  Snapshot last_snapshot;
  bool have_previous{false};
  std::map<PairKey, int> mark_ttl;
  std::map<PairKey, int> ghost_ttl;
  WatchDecorations deco;

  /// Apply the diff between the previously rendered frame and `snap`.
  void update(Snapshot & snap, const RenderOptions & ropt, const Options & o)
  {
    auto current = fastdds_transport_viz::pair_states(snap);
    snap.has_changes = true;
    if (have_previous) {
      snap.changes = fastdds_transport_viz::diff(last_rendered, current);
    }
    // age existing marks / ghosts
    for (auto it = mark_ttl.begin(); it != mark_ttl.end(); ) {
      if (--it->second <= 0) {deco.marks.erase(it->first); it = mark_ttl.erase(it);} else {++it;}
    }
    for (auto it = ghost_ttl.begin(); it != ghost_ttl.end(); ) {
      if (--it->second <= 0) {
        auto & g = deco.ghosts;
        g.erase(
          std::remove_if(
            g.begin(), g.end(), [&](const GhostPair & x) {return x.key == it->first;}),
          g.end());
        it = ghost_ttl.erase(it);
      } else {++it;}
    }
    for (const auto & k : snap.changes.added) {deco.marks[k] = '+'; mark_ttl[k] = kHoldFrames;}
    for (const auto & c : snap.changes.changed) {
      deco.marks[c.key] = '~';
      mark_ttl[c.key] = kHoldFrames;
    }
    for (const auto & k : snap.changes.removed) {
      // a pair that came back is no ghost any more
      deco.ghosts.erase(
        std::remove_if(
          deco.ghosts.begin(), deco.ghosts.end(),
          [&](const GhostPair & x) {return x.key == k;}), deco.ghosts.end());
      for (const auto & t : last_snapshot.topics) {
        for (const auto & p : t.pairs) {
          if (fastdds_transport_viz::pair_key(t, p) == k) {
            deco.ghosts.push_back(fastdds_transport_viz::ghost_pair(last_snapshot, t, p, ropt));
            ghost_ttl[k] = kHoldFrames;
          }
        }
      }
    }
    for (const auto & k : snap.changes.added) {
      deco.ghosts.erase(
        std::remove_if(
          deco.ghosts.begin(), deco.ghosts.end(),
          [&](const GhostPair & x) {return x.key == k;}), deco.ghosts.end());
      ghost_ttl.erase(k);
    }
    deco.summary = have_previous ?
      fastdds_transport_viz::changes_summary(snap.changes) : "first frame";
    last_rendered = std::move(current);
    // Keep the frame for ghost rows. TopicSummary holds pointers into
    // Snapshot::endpoints, so rebuild them against the copy.
    last_snapshot = snap;
    last_snapshot.topics = fastdds_transport_viz::summarize(last_snapshot.endpoints);
    apply_node_filter(last_snapshot.topics, o);
    fastdds_transport_viz::apply_stats(last_snapshot.topics, last_snapshot.stats);
    have_previous = true;
  }
};

/// The view options of a one-shot run applied to a loaded document: the default view keeps
/// ROS topics only (services and raw DDS topics need --all, as when observing), then the
/// --topic and --node filters.
void apply_view_filters(Snapshot & snap, const Options & o)
{
  std::regex topic_re;
  if (!o.topic_regex.empty()) {topic_re = std::regex(o.topic_regex);}
  snap.topics.erase(
    std::remove_if(
      snap.topics.begin(), snap.topics.end(),
      [&](const fastdds_transport_viz::TopicSummary & t) {
        if (!o.all && !(t.is_ros_topic && t.dds_topic.rfind("rt/", 0) == 0)) {return true;}
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
    o.timeout = o.stats ? 5.0 : 3.0;
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
    std::unique_ptr<fastdds_transport_viz::StatsObserver> stats;
    if (o.stats) {
      stats = std::make_unique<fastdds_transport_viz::StatsObserver>(observer.participant());
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
    // Wait until --timeout, or until discovery has been quiet for --quiet
    // seconds (but never less than --quiet seconds in total).
    for (;; ) {
      if (stats) {stats->poll();}
      std::this_thread::sleep_for(50ms);
      auto now = std::chrono::steady_clock::now();
      double elapsed = std::chrono::duration<double>(now - start).count();
      double since_last = std::chrono::duration<double>(now - observer.last_event()).count();
      if (elapsed >= o.timeout) {break;}
      // With --stats the whole window is needed for traffic counters to accumulate.
      if (!o.stats && o.quiet > 0 && elapsed >= o.quiet && since_last >= o.quiet) {break;}
      if (!rclcpp::ok()) {break;}
    }

    if (!o.watch) {
      double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      Snapshot snap = collect(observer, resolver, stats.get(), o, domain, elapsed);
      std::cout << (o.json ? fastdds_transport_viz::render_json(snap, ropt) :
      fastdds_transport_viz::render_table(snap, ropt)) << std::flush;
    } else {
      Terminal term(!o.json);
      WatchState ws;
      bool paused = false;
      bool quit = false;
      bool force = true;
      auto next_frame = std::chrono::steady_clock::now();
      while (rclcpp::ok() && !quit) {
        if (force || (!paused && std::chrono::steady_clock::now() >= next_frame)) {
          force = false;
          next_frame = std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(o.interval));
          double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
          Snapshot snap = collect(observer, resolver, stats.get(), o, domain, elapsed);
          ropt.verbose = o.verbose;
          ropt.explain = o.explain;
          ropt.locators = o.locators;
          ropt.advise = o.advise;
          ws.update(snap, ropt, o);
          if (o.json) {
            std::cout << fastdds_transport_viz::render_json(snap, ropt) << std::flush;
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
