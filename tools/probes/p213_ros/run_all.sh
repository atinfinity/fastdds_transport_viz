#!/bin/bash
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p213_ros"; mkdir -p "$W/out"   # ws_*/{build,install,log} and logs (out/) in $W
d=${D0:-73}
run() { ROS_DOMAIN_ID=$d "$HERE/run_case.sh" "$@"; d=$((d+1)); [ $d -gt 79 ] && d=73; }
for v in ${VARIANTS:-base a_type a_add b_default c_const d_rename e_comment e_bound e_cname e_nodef}; do
  run ${PREFIX}$v base $v cpp cpp
done
