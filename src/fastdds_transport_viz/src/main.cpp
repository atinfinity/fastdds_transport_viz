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
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <ctime>
#include <iostream>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "rclcpp/rclcpp.hpp"

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/fastdds_compat.hpp"
#include "fastdds_transport_viz/discovery_observer.hpp"
#include "fastdds_transport_viz/model.hpp"
#include "fastdds_transport_viz/render.hpp"
#include "fastdds_transport_viz/ros_graph_resolver.hpp"
#include "fastdds_transport_viz/shm_info.hpp"
#include "fastdds_transport_viz/stats_observer.hpp"

using namespace std::chrono_literals;
using fastdds_transport_viz::Endpoint;
using fastdds_transport_viz::GhostPair;
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
  bool all{false};
  bool watch{false};
  double interval{2.0};
  std::string topic_regex;
  std::string node_regex;
  bool list_codes{false};
  enum class Color { Auto, Always, Never } color{Color::Auto};
};

void usage()
{
  std::cout <<
    "Usage: transport_viz [options]\n"
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
    "                     q quit, p pause, v pairs, e legend, a all. With --json, emits one\n"
    "                     compact document per line (JSON Lines) with a `changes` object\n"
    "  --interval <sec>   refresh period for --watch (default: 2)\n"
    "  --list-codes       list all reason codes with descriptions and exit\n"
    "  -h, --help         this help\n";
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
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
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
    } else if (a == "--json") {
      o.json = true;
    } else if (a == "--stats") {
      o.stats = true;
    } else if (a == "--watch") {
      o.watch = true;
    } else if (a == "--list-codes") {
      o.list_codes = true;
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
  return true;
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
  const std::string own = resolver.own_node_name();

  std::vector<Endpoint> endpoints = observer.snapshot();
  std::vector<Endpoint> kept;
  std::regex re;
  if (!o.topic_regex.empty()) {
    re = std::regex(o.topic_regex);
  }
  fastdds_transport_viz::ShmScanInput shm_in;   // SHM ports of every endpoint, filtered or not
  const auto local_host = observer.local_host_id();
  // Participants of our own rclcpp node: the node's topic endpoints resolve to our name;
  // its participant-level endpoints (ros_discovery_info) do not, so match by prefix too.
  std::set<std::string> own_prefixes;
  std::set<std::string> other_host_prefixes;
  {
    // ... and the discovery/statistics participant of the tool itself
    const auto & prefix = observer.participant()->guid().guidPrefix;
    std::string s;
    char buf[4];
    for (int i = 0; i < 12; ++i) {
      std::snprintf(buf, sizeof(buf), "%02x", prefix.value[i]);
      s += (i ? "." : "") + std::string(buf);
    }
    own_prefixes.insert(s);
  }
  for (auto & e : endpoints) {
    e.node_name = resolver.node_for_guid(e.guid_bytes);
    if (e.node_name == own || e.ros_topic.rfind(own + "/", 0) == 0) {
      own_prefixes.insert(e.participant_guid_prefix);
    }
  }
  for (auto & e : endpoints) {
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
    // Our own rclcpp node's endpoints: topic endpoints resolve to our node
    // name, service endpoints live under our node name (services are not
    // covered by the graph API).
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
            std::string label = fastdds_transport_viz::to_string(p.verdict.transport) +
              (p.verdict.confidence == fastdds_transport_viz::Confidence::Likely ? "?" : "");
            deco.ghosts.push_back(
              GhostPair{k, t.display_type,
                fastdds_transport_viz::endpoint_label(last_snapshot, *p.writer, ropt),
                fastdds_transport_viz::endpoint_label(last_snapshot, *p.reader, ropt), label});
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
    const auto & c = snap.changes;
    if (!have_previous) {
      deco.summary = "first frame";
    } else if (c.empty()) {
      deco.summary = "none";
    } else {
      std::ostringstream os;
      if (!c.added.empty()) {
        os << "+" << c.added.size() << (c.added.size() == 1 ? " pair  " : " pairs  ");
      }
      if (!c.removed.empty()) {
        os << "-" << c.removed.size() << (c.removed.size() == 1 ? " pair  " : " pairs  ");
      }
      if (!c.changed.empty()) {os << "~" << c.changed.size() << " changed";}
      deco.summary = os.str();
    }
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

}  // namespace

int main(int argc, char ** argv)
{
  Options o;
  if (!parse(argc, argv, o)) {
    return 2;
  }
  if (o.list_codes) {
    for (const auto & c : fastdds_transport_viz::known_codes()) {
      std::cout << c << "\n    " << fastdds_transport_viz::explain(c) << "\n";
    }
    return 0;
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
    auto node = std::make_shared<rclcpp::Node>(
      "_transport_viz_" + std::to_string(getpid()), node_opts);
    fastdds_transport_viz::RosGraphResolver resolver(node);
    fastdds_transport_viz::DiscoveryObserver observer(domain);
    std::unique_ptr<fastdds_transport_viz::StatsObserver> stats;
    if (o.stats) {
      stats = std::make_unique<fastdds_transport_viz::StatsObserver>(observer.participant());
    }

    RenderOptions ropt;
    ropt.verbose = o.verbose;
    ropt.explain = o.explain;
    ropt.compact = o.json && o.watch;   // JSON Lines: one document per line
    ropt.color = o.color == Options::Color::Always ||
      (o.color == Options::Color::Auto && isatty(STDOUT_FILENO) &&
      std::getenv("NO_COLOR") == nullptr);

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
              frame << "\n q quit   p " << (paused ? "resume" : "pause") <<
                "   v pairs   e legend   a all\n";
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
          default: break;
        }
      }
    }
  }
  rclcpp::shutdown();
  return rc;
}
