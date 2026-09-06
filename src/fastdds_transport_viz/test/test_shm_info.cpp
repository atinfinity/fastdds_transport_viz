// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
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
  auto info = scan_shm("/nonexistent/ftv_shm", ShmScanInput{});
  EXPECT_FALSE(info.available);
  EXPECT_TRUE(info.warnings.empty());
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
  file("unrelated", 4096);

  ShmScanInput in;
  in.node_ports = {7411, 7419};
  in.datasharing_writers = {
    {"fast_datasharing_01.02.03.04.05.06.07.08.00.00.00.00_0.0.14.3", "W1"}};
  auto info = scan_shm(dir, in);
  EXPECT_TRUE(info.available);
  EXPECT_GT(info.total_bytes, 0u);
  EXPECT_EQ(info.fastdds_bytes, 1000u * 4 + 500 * 4 + 32 * 2 + 200 + 300);
  EXPECT_EQ(info.segments, 4u);
  EXPECT_EQ(info.stale_segments, 2u);
  EXPECT_EQ(info.ports, 4u);
  EXPECT_EQ(info.stale_ports, 1u);
  EXPECT_EQ(info.datasharing_histories, 2u);
  EXPECT_EQ(info.datasharing_unmatched, 1u);
  ASSERT_EQ(info.datasharing_by_writer.count("W1"), 1u);
  EXPECT_EQ(info.datasharing_by_writer.at("W1"), 200u);
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

  // the held port is the tool's own (same number in another network namespace)
  in.own_ports = {7411};
  info = scan_shm(dir, in);
  EXPECT_EQ(info.missing_ports.size(), 2u);

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

TEST_F(FakeShmDir, ExplanationsExistForShmWarnings)
{
  // codes must be documented for --explain / reason_code_descriptions
  for (const char * code : {"shm-stale-files", "shm-nearly-full", "shm-not-visible"}) {
    EXPECT_NE(explain(code), "(no description)") << code;
  }
}

TEST(ScanShm, RegularFileIsNotADirectory)
{
  char tmpl[] = "/tmp/ftv_shm_file_XXXXXX";
  int fd = ::mkstemp(tmpl);
  ASSERT_GE(fd, 0);
  ::close(fd);
  auto info = scan_shm(tmpl, ShmScanInput{});   // statvfs works, opendir does not
  EXPECT_TRUE(info.available);
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
