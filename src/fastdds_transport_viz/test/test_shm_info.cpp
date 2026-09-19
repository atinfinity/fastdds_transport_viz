// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "fastdds_transport_viz/decision.hpp"
#include "fastdds_transport_viz/shm_info.hpp"

using namespace fastdds_transport_viz;  // NOLINT

namespace
{

bool has(const std::vector<std::string> & v, const std::string & s)
{
  return std::find(v.begin(), v.end(), s) != v.end();
}

/// A fake /dev/shm with the file layout Fast DDS produces.
class FakeShmDir : public ::testing::Test
{
protected:
  void SetUp() override
  {
    char tmpl[] = "/tmp/ftv_shm_XXXXXX";
    ASSERT_NE(::mkdtemp(tmpl), nullptr);
    dir = tmpl;
  }
  void TearDown() override
  {
    for (int fd : held) {::close(fd);}
    for (const auto & f : files) {::unlink((dir + "/" + f).c_str());}
    ::rmdir(dir.c_str());
  }
  void file(const std::string & name, size_t bytes)
  {
    std::ofstream(dir + "/" + name) << std::string(bytes, 'x');
    files.push_back(name);
  }
  /// Create `<name>_el` and keep it flock()ed like a living Fast DDS process.
  void locked(const std::string & name)
  {
    file(name + "_el", 0);
    int fd = ::open((dir + "/" + name + "_el").c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(::flock(fd, LOCK_EX), 0);
    held.push_back(fd);
  }
  std::string dir;
  std::vector<std::string> files;
  std::vector<int> held;
};

}  // namespace

TEST(DataSharingName, MatchesFastDdsFormat)
{
  // fast_datasharing_01.0f.40.ec.26.00.af.24.00.00.00.00_0.0.14.3 for GUID
  // 01.0f.40.ec.26.00.af.24.00.00.00.00|00.00.14.03 (entity id bytes in hex, unpadded)
  std::array<uint8_t, 16> g{
    0x01, 0x0f, 0x40, 0xec, 0x26, 0x00, 0xaf, 0x24, 0, 0, 0, 0, 0x00, 0x00, 0x14, 0x03};
  EXPECT_EQ(
    datasharing_segment_name(g), "fast_datasharing_01.0f.40.ec.26.00.af.24.00.00.00.00_0.0.14.3");
  g[13] = 0x0a;
  EXPECT_EQ(
    datasharing_segment_name(g), "fast_datasharing_01.0f.40.ec.26.00.af.24.00.00.00.00_0.a.14.3");
}

TEST(ScanShm, MissingDirectoryIsNotAvailable)
{
  ShmScanInput in;
  in.node_ports = {7411};
  auto info = scan_shm("/nonexistent/ftv_shm", in);
  EXPECT_FALSE(info.available);
  EXPECT_FALSE(info.listed);
  EXPECT_TRUE(info.warnings.empty());
  EXPECT_TRUE(info.port_locks.empty());   // nothing probed: unprobed in the JSON (#125)
}

TEST_F(FakeShmDir, CountsSizesAndStaleFilesByLock)
{
  file("fastrtps_aaaa", 1000);
  locked("fastrtps_aaaa");                 // living participant
  file("fastrtps_bbbb", 1000);
  file("fastrtps_bbbb_el", 0);             // owner died: lock free
  // no lock file: not a zombie for 'fastdds shm clean' either
  file("fastrtps_cccc", 1000);
  file("fastrtps_port7411", 500);
  locked("fastrtps_port7411");             // port in use
  file("fastrtps_port7000", 500);
  file("fastrtps_port7000_el", 0);         // users died: lock free
  file("fastrtps_port7002", 500);          // released cleanly: no lock file
  file("fastdds_dddd", 1000);              // Fast DDS 3.x naming
  file("fastdds_dddd_el", 0);
  file("fastdds_port7419", 500);
  locked("fastdds_port7419");
  file("sem.fastdds_port7419_mutex", 32);
  file("sem.fastrtps_port7411_mutex", 32);
  file("fast_datasharing_01.02.03.04.05.06.07.08.00.00.00.00_0.0.14.3", 200);
  file("fast_datasharing_01.02.03.04.05.06.07.08.00.00.00.00_0.0.15.3", 300);
  // a discovered reader's notification segment
  file("fast_datasharing_01.02.03.04.05.06.07.08.00.00.00.00_0.0.16.4", 100);
  file("unrelated", 4096);

  ShmScanInput in;
  in.node_ports = {7411, 7419};
  in.datasharing_writers = {
    {"fast_datasharing_01.02.03.04.05.06.07.08.00.00.00.00_0.0.14.3", "W1"}};
  in.datasharing_readers = {
    {"fast_datasharing_01.02.03.04.05.06.07.08.00.00.00.00_0.0.16.4", "R1"}};
  auto info = scan_shm(dir, in);
  EXPECT_TRUE(info.available);
  EXPECT_TRUE(info.listed);
  EXPECT_GT(info.total_bytes, 0u);
  EXPECT_EQ(info.fastdds_bytes, 1000u * 4 + 500 * 4 + 32 * 2 + 200 + 300 + 100);
  EXPECT_EQ(info.segments, 4u);
  EXPECT_EQ(info.stale_segments, 2u);
  EXPECT_EQ(info.ports, 4u);
  EXPECT_EQ(info.stale_ports, 1u);
  EXPECT_EQ(info.datasharing_histories, 2u);
  EXPECT_EQ(info.datasharing_unmatched, 1u);
  ASSERT_EQ(info.datasharing_by_writer.count("W1"), 1u);
  EXPECT_EQ(info.datasharing_by_writer.at("W1"), 200u);
  // the notification segment is neither a history nor unmatched
  EXPECT_EQ(info.datasharing_notifications, 1u);
  EXPECT_EQ(info.datasharing_notification_readers, (std::set<std::string>{"R1"}));
  EXPECT_EQ(info.checked_ports, (std::vector<uint32_t>{7411, 7419}));
  EXPECT_TRUE(info.missing_ports.empty());
  EXPECT_TRUE(info.nodes_visible);
  EXPECT_TRUE(has(info.warnings, "shm-stale-files"));
  EXPECT_FALSE(has(info.warnings, "shm-not-visible"));
}

TEST_F(FakeShmDir, CleanDirectoryHasNoWarnings)
{
  file("fastrtps_aaaa", 10);
  locked("fastrtps_aaaa");
  auto info = scan_shm(dir, ShmScanInput{});
  EXPECT_EQ(info.segments, 1u);
  EXPECT_EQ(info.stale_segments, 0u);
  EXPECT_TRUE(info.warnings.empty()) << info.warnings.size();
}

TEST_F(FakeShmDir, NodesInAnotherIpcNamespace)
{
  file("fastrtps_bbbb", 10);               // stale leftover of this namespace
  file("fastrtps_bbbb_el", 0);
  file("fastrtps_port7413", 10);           // released port: nobody holds its lock
  ShmScanInput in;
  in.node_ports = {7411, 7413};
  auto info = scan_shm(dir, in);
  EXPECT_EQ(info.missing_ports, (std::vector<uint32_t>{7411, 7413}));
  EXPECT_FALSE(info.nodes_visible);
  EXPECT_TRUE(has(info.warnings, "shm-not-visible"));
  // nothing here belongs to the nodes: leftovers are not reported as theirs
  EXPECT_EQ(info.stale_segments, 0u);
  EXPECT_EQ(info.stale_ports, 0u);
  EXPECT_FALSE(has(info.warnings, "shm-stale-files"));

  // partially visible: still classify what is here
  file("fastrtps_port7411", 10);
  locked("fastrtps_port7411");
  info = scan_shm(dir, in);
  EXPECT_EQ(info.checked_ports, (std::vector<uint32_t>{7411, 7413}));
  EXPECT_EQ(info.missing_ports, (std::vector<uint32_t>{7413}));
  EXPECT_TRUE(has(info.warnings, "shm-not-visible"));
  EXPECT_TRUE(has(info.warnings, "shm-stale-files"));
  // the same probe per port (#125): held here, released (its lock file is gone)
  EXPECT_EQ(info.port_locks.at(7411), PortLock::Held);
  EXPECT_EQ(info.port_locks.at(7413), PortLock::Absent);

  // the held port is the tool's own (same number in another network namespace)
  in.own_ports = {7411};
  info = scan_shm(dir, in);
  EXPECT_EQ(info.missing_ports.size(), 2u);
  EXPECT_EQ(info.port_locks.at(7411), PortLock::Own);

  // participants with another host id are never visible (they announce no SHM locator)
  in = ShmScanInput{};
  in.other_host_participants = 2;
  info = scan_shm(dir, in);
  EXPECT_TRUE(info.checked_ports.empty());
  EXPECT_EQ(info.other_host_participants, 2u);
  EXPECT_FALSE(info.nodes_visible);
  EXPECT_TRUE(has(info.warnings, "shm-not-visible"));
  EXPECT_FALSE(has(info.warnings, "shm-stale-files"));   // nothing here is theirs
}

TEST_F(FakeShmDir, UnreadableLockIsUnknownNotMissing)
{
  file("fastdds_port7415", 10);
  ASSERT_EQ(::symlink("fastdds_port7415_el", (dir + "/fastdds_port7415_el").c_str()), 0);
  files.push_back("fastdds_port7415_el");   // open() fails with ELOOP
  ShmScanInput in;
  in.node_ports = {7411, 7415};
  auto info = scan_shm(dir, in);
  EXPECT_EQ(info.missing_ports, (std::vector<uint32_t>{7411, 7415}));
  EXPECT_EQ(info.unknown_ports, (std::vector<uint32_t>{7415}));
  EXPECT_EQ(info.port_locks.at(7411), PortLock::Absent);   // no such port file
  EXPECT_EQ(info.port_locks.at(7415), PortLock::Unknown);

  // the tool's own port is another namespace's whatever its lock says
  in.own_ports = {7415};
  info = scan_shm(dir, in);
  EXPECT_EQ(info.missing_ports, (std::vector<uint32_t>{7411, 7415}));
  EXPECT_TRUE(info.unknown_ports.empty());
  EXPECT_EQ(info.port_locks.at(7415), PortLock::Own);
}

TEST_F(FakeShmDir, FreeLockIsUndecided)
{
  // a node that just died leaves a free lock and may still be discovered: not proof
  // that it listens in another IPC namespace
  file("fastdds_port7417", 10);
  file("fastdds_port7417_el", 0);
  ShmScanInput in;
  in.node_ports = {7417};
  auto info = scan_shm(dir, in);
  EXPECT_EQ(info.missing_ports, (std::vector<uint32_t>{7417}));
  EXPECT_EQ(info.unknown_ports, (std::vector<uint32_t>{7417}));
  EXPECT_EQ(info.port_locks.at(7417), PortLock::Stale);
  EXPECT_FALSE(info.nodes_visible);
  EXPECT_EQ(participant_shm_visibility({7417}, {7417}, info, {}), ShmVisibility::Unprobed);
}

TEST(ParticipantShmVisibility, FromTheScanResult)
{
  ShmInfo info;
  info.available = true;
  info.checked_ports = {7000, 7001, 16161, 16163, 16165};
  info.missing_ports = {7001, 16163, 16165};   // 16165 could not be probed
  info.unknown_ports = {16165};
  const std::map<uint32_t, size_t> single{};
  const std::map<uint32_t, size_t> shared{{7000, 2}};
  const std::map<uint32_t, size_t> both_shared{{7000, 2}, {16161, 2}};
  // 7000 and 7001 as the ros_discovery_info reader's ports, the rest from the other endpoints
  const std::set<uint32_t> proof{16161, 16163, 16165, 17000};
  using SV = ShmVisibility;

  EXPECT_EQ(participant_shm_visibility({16161}, proof, info, single), SV::Visible);
  EXPECT_EQ(participant_shm_visibility({7000, 16161}, proof, info, single), SV::Visible);
  // a held number announced by two participants: whose lock is it? It leaves the decision
  // to the participant's other ports (#118)
  EXPECT_EQ(participant_shm_visibility({7000, 16161}, proof, info, shared), SV::Visible);
  EXPECT_EQ(participant_shm_visibility({7000, 16161}, proof, info, both_shared), SV::Unprobed);
  // the reader's 7000+ number proves nothing: any participant of the namespace may hold it
  EXPECT_EQ(participant_shm_visibility({7000}, proof, info, single), SV::Unprobed);
  EXPECT_EQ(participant_shm_visibility({7000}, {}, info, single), SV::Unprobed);
  // one missing port is enough, even next to a held one of its own
  EXPECT_EQ(participant_shm_visibility({7000, 16163}, proof, info, shared), SV::NotVisible);
  EXPECT_EQ(participant_shm_visibility({16161, 16163}, proof, info, single), SV::NotVisible);
  EXPECT_EQ(participant_shm_visibility({16161, 7001}, proof, info, single), SV::NotVisible);
  // an unreadable lock or a port that was not probed decides nothing, even next to a held one
  EXPECT_EQ(participant_shm_visibility({16165}, proof, info, single), SV::Unprobed);
  EXPECT_EQ(participant_shm_visibility({16161, 16165}, proof, info, single), SV::Unprobed);
  EXPECT_EQ(participant_shm_visibility({16161, 17000}, proof, info, single), SV::Unprobed);
  EXPECT_EQ(participant_shm_visibility({}, proof, info, single), SV::Unprobed);
  EXPECT_EQ(participant_shm_visibility({16161}, proof, ShmInfo{}, single), SV::Unprobed);
}

TEST_F(FakeShmDir, ExplanationsExistForShmWarnings)
{
  // codes must be documented for --explain / reason_code_descriptions
  for (const char * code : {"shm-stale-files", "shm-nearly-full", "shm-not-visible"}) {
    EXPECT_NE(explain(code), "(no description)") << code;
  }
}

TEST_F(FakeShmDir, HeldPortLocksOfThisProcess)
{
  locked("fastdds_port7421");
  const auto held = held_port_locks();
  ASSERT_TRUE(held.has_value());
  EXPECT_EQ(held->count(7421), 1u);
}

TEST(HeldPortLocks, OnlyPortLockFiles)
{
  char tmpl[] = "/tmp/ftv_fd_XXXXXX";
  ASSERT_NE(::mkdtemp(tmpl), nullptr);
  const std::string dir = tmpl;
  const std::vector<std::pair<std::string, std::string>> links = {
    {"3", "/dev/shm/fastrtps_port7411_el"},
    {"4", "/dev/shm/fastdds_port17915_el"},
    {"5", "/dev/shm/fastrtps_port7413"},              // the port itself, not its lock
    {"6", "/dev/shm/fastrtps_port7400_sl"},           // multicast shared lock
    {"7", "/dev/shm/fastrtps_aaaa_el"},               // segment lock
    {"8", "/dev/shm/fastrtps_port7417_el (deleted)"},
    {"9", "socket:[12345]"},
  };
  for (const auto & [fd, target] : links) {
    ASSERT_EQ(::symlink(target.c_str(), (dir + "/" + fd).c_str()), 0);
  }
  const auto held = held_port_locks(dir);
  for (const auto & l : links) {
    ::unlink((dir + "/" + l.first).c_str());
  }
  ::rmdir(dir.c_str());
  ASSERT_TRUE(held.has_value());
  EXPECT_EQ(*held, (std::set<uint32_t>{7411, 17915}));
  EXPECT_FALSE(held_port_locks(dir).has_value());   // no such directory
}

TEST(ScanShm, RegularFileIsNotADirectory)
{
  char tmpl[] = "/tmp/ftv_shm_file_XXXXXX";
  int fd = ::mkstemp(tmpl);
  ASSERT_GE(fd, 0);
  ::close(fd);
  auto info = scan_shm(tmpl, ShmScanInput{});   // statvfs works, opendir does not
  EXPECT_TRUE(info.available);
  EXPECT_FALSE(info.listed);   // no segment visibility without a listing
  EXPECT_EQ(info.segments, 0u);
  ::unlink(tmpl);
}

TEST(ScanShm, CapacityWarning)
{
  ShmInfo info;
  info.total_bytes = 1000; info.used_bytes = 100; info.free_bytes = 900;
  add_capacity_warning(info);
  EXPECT_TRUE(has(info.warnings, "shm-nearly-full"));   // 900 bytes free < 16 MiB
  info.warnings.clear();
  info.total_bytes = 64ull << 20; info.used_bytes = 10ull << 20; info.free_bytes = 54ull << 20;
  add_capacity_warning(info);
  EXPECT_TRUE(info.warnings.empty());
  info.used_bytes = 60ull << 20; info.free_bytes = 4ull << 20;   // < 16 MiB free
  add_capacity_warning(info);
  EXPECT_TRUE(has(info.warnings, "shm-nearly-full"));
  info.warnings.clear();
  // 93 % used
  info.total_bytes = 16ull << 30;
  info.used_bytes = 15ull << 30;
  info.free_bytes = 1ull << 30;
  add_capacity_warning(info);
  EXPECT_TRUE(has(info.warnings, "shm-nearly-full"));
  info.warnings.clear();
  info.total_bytes = 0;
  add_capacity_warning(info);
  EXPECT_TRUE(info.warnings.empty());
}
