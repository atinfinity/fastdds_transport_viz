// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "fastdds_transport_viz/rmw_check.hpp"

using fastdds_transport_viz::rmw_verdict;
using fastdds_transport_viz::RmwVerdictKind;

TEST(RmwCheck, FastrtpsCppIsAcceptedSilently)
{
  const auto v = rmw_verdict(std::string("rmw_fastrtps_cpp"), "rmw_fastrtps_cpp", "");
  EXPECT_EQ(v.kind, RmwVerdictKind::Accept);
  EXPECT_TRUE(v.message.empty());
  // the default RMW (RMW_IMPLEMENTATION unset) is judged by the identifier, not the variable
  EXPECT_EQ(rmw_verdict(std::string("rmw_fastrtps_cpp"), "", "").kind, RmwVerdictKind::Accept);
}

TEST(RmwCheck, FastrtpsDynamicCppIsAcceptedSilently)
{
  const auto v = rmw_verdict(
    std::string("rmw_fastrtps_dynamic_cpp"), "rmw_fastrtps_dynamic_cpp", "");
  EXPECT_EQ(v.kind, RmwVerdictKind::Accept);
  EXPECT_TRUE(v.message.empty()) << v.message;
}

TEST(RmwCheck, OtherMiddlewareIsRejectedNamingItAndTheFix)
{
  for (const char * other : {"rmw_cyclonedds_cpp", "rmw_zenoh_cpp", "rmw_connextdds"}) {
    const auto v = rmw_verdict(std::string(other), other, "");
    EXPECT_EQ(v.kind, RmwVerdictKind::Reject) << other;
    EXPECT_EQ(v.message.rfind(std::string("RMW is ") + other + ";", 0), 0u) << v.message;
    EXPECT_NE(v.message.find("RMW_IMPLEMENTATION=rmw_fastrtps_cpp"), std::string::npos);
    EXPECT_EQ(v.message.find("transport_viz:"), std::string::npos) <<
      "no program prefix: transport_viz and ros2 transport add their own";
  }
}

TEST(RmwCheck, UnloadableRmwIsRejectedWithTheRmwError)
{
  const auto v = rmw_verdict(std::nullopt, "rmw_bogus_cpp", "failed to load shared library");
  EXPECT_EQ(v.kind, RmwVerdictKind::Reject);
  EXPECT_EQ(
    v.message,
    "cannot load the RMW named by RMW_IMPLEMENTATION=rmw_bogus_cpp: failed to load shared "
    "library");
  // unset variable and no error text: still a complete sentence
  EXPECT_EQ(
    rmw_verdict(std::nullopt, "", "").message,
    "cannot load the RMW named by RMW_IMPLEMENTATION=(unset)");
}
