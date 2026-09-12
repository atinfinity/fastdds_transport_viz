// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0
//
// Is the RMW this process would run on one the tool makes sense with? Pure function; the
// caller asks rmw_get_implementation_identifier() and prints / exits on the verdict.

#ifndef FASTDDS_TRANSPORT_VIZ__RMW_CHECK_HPP_
#define FASTDDS_TRANSPORT_VIZ__RMW_CHECK_HPP_

#include <optional>
#include <string>

namespace fastdds_transport_viz
{

/// The RMW the tool is written for: its discovery data, name mangling and statistics.
constexpr char kSupportedRmw[] = "rmw_fastrtps_cpp";
/// Also accepted: shares the discovery layer, name mangling and statistics with
/// kSupportedRmw (the type support goes through introspection instead of generated code,
/// which no verdict depends on); the launch test suite passes on it (issue #73).
constexpr char kSupportedRmwDynamic[] = "rmw_fastrtps_dynamic_cpp";

enum class RmwVerdictKind
{
  Accept,   // rmw_fastrtps_cpp or rmw_fastrtps_dynamic_cpp: nothing to say
  Reject,   // another middleware, or no RMW could be loaded: do not start
};

struct RmwVerdict
{
  RmwVerdictKind kind{RmwVerdictKind::Accept};
  /// Empty for Accept. Reject: the reason without a program prefix
  /// ("RMW is rmw_cyclonedds_cpp; ..."), so that `transport_viz` and `ros2 transport` can
  /// each prepend their own name.
  std::string message;
};

/// `identifier` is rmw_get_implementation_identifier() (std::nullopt when it returned
/// nullptr, i.e. the RMW named by RMW_IMPLEMENTATION could not be loaded; `load_error` is
/// then the rmw error text). `requested` is the RMW_IMPLEMENTATION value, "" when unset.
RmwVerdict rmw_verdict(
  const std::optional<std::string> & identifier, const std::string & requested,
  const std::string & load_error);

}  // namespace fastdds_transport_viz

#endif  // FASTDDS_TRANSPORT_VIZ__RMW_CHECK_HPP_
