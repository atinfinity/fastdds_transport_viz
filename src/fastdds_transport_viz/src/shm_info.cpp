// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/shm_info.hpp"

#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace fastdds_transport_viz
{

namespace
{

// Fast DDS 2.x names its files fastrtps_*, 3.x fastdds_*; data-sharing files kept their name.
constexpr const char * kSegmentPrefixes[] = {"fastrtps_", "fastdds_"};
constexpr const char * kPortPrefixes[] = {"fastrtps_port", "fastdds_port"};
constexpr const char * kSemPrefixes[] = {"sem.fastrtps_", "sem.fastdds_"};
constexpr const char * kDataSharingPrefix = "fast_datasharing_";
constexpr const char * kLockSuffix = "_el";
constexpr uint64_t kNearlyFullFreeBytes = 16ull * 1024 * 1024;   // one large-data segment
constexpr double kNearlyFullRatio = 0.9;

bool starts_with(const std::string & s, const char * prefix)
{
  return s.rfind(prefix, 0) == 0;
}

/// The prefix of `s` among `prefixes`, or nullptr.
template<size_t N>
const char * prefix_of(const std::string & s, const char * const (&prefixes)[N])
{
  for (const char * p : prefixes) {
    if (starts_with(s, p)) {return p;}
  }
  return nullptr;
}

bool ends_with(const std::string & s, const char * suffix)
{
  const std::string suf(suffix);
  return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

bool is_hex(const std::string & s)
{
  if (s.empty()) {return false;}
  for (char c : s) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {return false;}
  }
  return true;
}

bool is_digits(const std::string & s)
{
  if (s.empty()) {return false;}
  for (char c : s) {
    if (c < '0' || c > '9') {return false;}
  }
  return true;
}

/// Does nobody hold the lock file? Fast DDS keeps `<name>_el` flock()ed for as long as
/// the owner (segments: exclusive; ports: shared by every user) is alive; a lock that
/// can be taken means the owner died without cleaning up. Same probe `fastdds shm
/// clean` uses, and like it a missing lock file (removed by the last clean release) is
/// not treated as a zombie.
enum class LockState { Held, Free, Missing, Unknown };

LockState probe_lock(const std::string & lock_path)
{
  int fd = ::open(lock_path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return errno == ENOENT ? LockState::Missing : LockState::Unknown;
  }
  LockState st = LockState::Unknown;
  if (::flock(fd, LOCK_EX | LOCK_NB) == 0) {
    st = LockState::Free;
    ::flock(fd, LOCK_UN);
  } else if (errno == EWOULDBLOCK) {
    st = LockState::Held;
  }
  ::close(fd);
  return st;
}

}  // namespace

std::string datasharing_segment_name(const std::array<uint8_t, 16> & g)
{
  char buf[96];
  int n = std::snprintf(buf, sizeof(buf), "%s", kDataSharingPrefix);
  for (int i = 0; i < 12; ++i) {
    n += std::snprintf(buf + n, sizeof(buf) - n, "%02x%s", g[i], i < 11 ? "." : "_");
  }
  // Fast DDS streams the entity id byte by byte in hex without zero padding
  for (int i = 12; i < 16; ++i) {
    n += std::snprintf(buf + n, sizeof(buf) - n, "%x%s", g[i], i < 15 ? "." : "");
  }
  return buf;
}

ShmInfo scan_shm(const std::string & path, const ShmScanInput & in)
{
  ShmInfo info;
  info.path = path;

  struct statvfs vfs {};
  if (::statvfs(path.c_str(), &vfs) != 0) {
    return info;   // no /dev/shm (macOS, or a restricted environment): not available
  }
  info.available = true;
  info.total_bytes = static_cast<uint64_t>(vfs.f_blocks) * vfs.f_frsize;
  info.free_bytes = static_cast<uint64_t>(vfs.f_bavail) * vfs.f_frsize;
  info.used_bytes = info.total_bytes >= info.free_bytes ? info.total_bytes - info.free_bytes : 0;

  DIR * dir = ::opendir(path.c_str());
  if (dir == nullptr) {
    return info;
  }
  std::set<std::string> ports_held;   // port files whose lock a living process holds
  while (struct dirent * ent = ::readdir(dir)) {
    const std::string name = ent->d_name;
    const char * segment_prefix = prefix_of(name, kSegmentPrefixes);
    const char * port_prefix = prefix_of(name, kPortPrefixes);
    const bool fastdds_file = segment_prefix != nullptr ||
      starts_with(name, kDataSharingPrefix) || prefix_of(name, kSemPrefixes) != nullptr;
    if (!fastdds_file) {continue;}
    const std::string full = path + "/" + name;
    struct stat st {};
    if (::stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {continue;}
    info.fastdds_bytes += static_cast<uint64_t>(st.st_size);
    if (ends_with(name, kLockSuffix)) {continue;}

    if (port_prefix != nullptr) {
      const std::string port = name.substr(std::string(port_prefix).size());
      if (!is_digits(port)) {continue;}
      ++info.ports;
      const auto st = probe_lock(full + kLockSuffix);
      if (st == LockState::Free) {++info.stale_ports;}
      if (st == LockState::Held) {ports_held.insert(port);}
    } else if (segment_prefix != nullptr) {
      if (!is_hex(name.substr(std::string(segment_prefix).size()))) {continue;}
      ++info.segments;
      if (probe_lock(full + kLockSuffix) == LockState::Free) {++info.stale_segments;}
    } else if (starts_with(name, kDataSharingPrefix)) {
      ++info.datasharing_histories;
      auto it = in.datasharing_writers.find(name);
      if (it == in.datasharing_writers.end()) {
        ++info.datasharing_unmatched;
      } else {
        info.datasharing_by_writer[it->second] = static_cast<uint64_t>(st.st_size);
      }
    }
  }
  ::closedir(dir);

  // The nodes' SHM locators name ports. A node in this IPC namespace holds the lock of
  // its port file; a port nobody holds here, a port that collides with the tool's own
  // (same number in another network namespace) or a participant with another host id
  // means the node uses another /dev/shm, and this scan says nothing about its memory.
  for (uint32_t port : in.node_ports) {
    info.checked_ports.push_back(port);
    if (in.own_ports.count(port) || !ports_held.count(std::to_string(port))) {
      info.missing_ports.push_back(port);
    }
  }
  info.other_host_participants = in.other_host_participants;
  info.nodes_visible = info.missing_ports.empty() && info.other_host_participants == 0;
  if (!info.nodes_visible) {
    info.warnings.push_back("shm-not-visible");
    if (info.missing_ports.size() == info.checked_ports.size()) {
      // nothing here belongs to the observed nodes: leftovers are not their problem
      info.stale_segments = 0;
      info.stale_ports = 0;
    }
  }
  if (info.stale_segments + info.stale_ports > 0) {
    info.warnings.push_back("shm-stale-files");
  }
  add_capacity_warning(info);
  return info;
}

void add_capacity_warning(ShmInfo & info)
{
  if (info.total_bytes > 0 &&
    (info.free_bytes < kNearlyFullFreeBytes ||
    static_cast<double>(info.used_bytes) >=
    kNearlyFullRatio * static_cast<double>(info.total_bytes)))
  {
    info.warnings.push_back("shm-nearly-full");
  }
}

}  // namespace fastdds_transport_viz
