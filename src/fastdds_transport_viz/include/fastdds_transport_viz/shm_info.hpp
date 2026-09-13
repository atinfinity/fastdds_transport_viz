// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Shared-memory usage of the environment the tool runs in: capacity of /dev/shm
// and the files Fast DDS keeps there (segments, port ring buffers, data-sharing
// histories). Pure POSIX; no DDS runtime needed, so it is unit-tested on a temp dir.

#ifndef FASTDDS_TRANSPORT_VIZ__SHM_INFO_HPP_
#define FASTDDS_TRANSPORT_VIZ__SHM_INFO_HPP_

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "fastdds_transport_viz/model.hpp"

namespace fastdds_transport_viz
{

/// Directory Fast DDS uses for shared memory on Linux.
inline constexpr const char * kDefaultShmDir = "/dev/shm";

/// Name of the data-sharing history file Fast DDS creates for a writer
/// ("fast_datasharing_<guid prefix>_<entity id>").
std::string datasharing_segment_name(const std::array<uint8_t, 16> & writer_guid);

/// What discovery knows that the scan needs.
struct ShmScanInput
{
  /// SHM locator ports announced by observed endpoints with the tool's host id. Such a
  /// port is "visible" when its `fastrtps_port<N>_el` lock is held by a living process
  /// (the port's listener holds it; senders do not) and it is not one of the tool's own
  /// ports (then the lock is the tool's: the same port number in another IPC namespace).
  std::set<uint32_t> node_ports;
  /// SHM ports of the tool's own participants.
  std::set<uint32_t> own_ports;
  /// Participants of observed endpoints with another host id: never in this /dev/shm
  /// (they do not even announce SHM locators to us).
  size_t other_host_participants{0};
  /// data-sharing file name -> writer GUID, to attribute history files to discovered
  /// writers (fills ShmInfo::datasharing_by_writer).
  std::map<std::string, std::string> datasharing_writers;
};

/// Scan `path` (statvfs + directory listing + lock probes).
ShmInfo scan_shm(const std::string & path, const ShmScanInput & in);

/// Where a participant with the tool's host id listens for SHM, from the scan result:
/// NotVisible when one of its `ports` is missing here (nobody holds the lock, or the
/// number is one of the tool's own), Visible when every port is held here and
/// `participants_per_port` counts no other participant announcing it (a number shared by
/// participants in several IPC namespaces does not say whose lock is held), otherwise
/// Unprobed (no scan, a lock that could not be read, a shared number).
ShmVisibility participant_shm_visibility(
  const std::set<uint32_t> & ports, const ShmInfo & info,
  const std::map<uint32_t, size_t> & participants_per_port);

/// SHM ports whose `fastrtps_port<N>_el` / `fastdds_port<N>_el` lock file this process
/// holds open, from the fd links in `fd_dir`: the ports the tool's own participants
/// listen on, including those no discovered endpoint announces. nullopt without procfs.
std::optional<std::set<uint32_t>> held_port_locks(const std::string & fd_dir = "/proc/self/fd");

/// Adds `shm-nearly-full` from total/used/free (90 % used or less than 16 MiB free).
void add_capacity_warning(ShmInfo & info);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__SHM_INFO_HPP_
