// Copyright 2026 atinfinity
// SPDX-License-Identifier: Apache-2.0

#include "fastdds_transport_viz/rmw_check.hpp"

#include <optional>
#include <string>

namespace fastdds_transport_viz
{

RmwVerdict rmw_verdict(
  const std::optional<std::string> & identifier, const std::string & requested,
  const std::string & load_error)
{
  if (!identifier) {
    return {RmwVerdictKind::Reject,
      "cannot load the RMW named by RMW_IMPLEMENTATION=" +
      (requested.empty() ? std::string("(unset)") : requested) +
      (load_error.empty() ? std::string() : ": " + load_error)};
  }
  if (*identifier == kSupportedRmw || *identifier == kSupportedRmwDynamic) {
    return {RmwVerdictKind::Accept, ""};
  }
  return {RmwVerdictKind::Reject,
    "RMW is " + *identifier + "; this tool observes Fast DDS and needs " + kSupportedRmw +
    " (set RMW_IMPLEMENTATION=" + kSupportedRmw + ", see Limitations in the README)"};
}

}  // namespace fastdds_transport_viz
