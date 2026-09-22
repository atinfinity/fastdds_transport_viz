#!/bin/bash
# usage: run.sh [seconds]  -- runs the #193 type-identity probe next to a demo talker and
# listener in the current distribution's image (build it first with build.sh).
set +u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p193"; mkdir -p "$W/out"   # binaries in $W, logs in $W/out
SEC=${1:-12}
source /opt/ros/$ROS_DISTRO/setup.bash
export RMW_IMPLEMENTATION=${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}
export ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}
export LD_LIBRARY_PATH=/opt/ros/$ROS_DISTRO/lib:/opt/ros/$ROS_DISTRO/lib/$(uname -m)-linux-gnu:${LD_LIBRARY_PATH:-}
ros2 run demo_nodes_cpp talker > "$W/out/talker.log" 2>&1 &
T=$!
ros2 run demo_nodes_cpp listener > "$W/out/listener.log" 2>&1 &
L=$!
sleep 4
"$W/probe_$ROS_DISTRO" "$SEC"
kill $T $L 2>/dev/null
wait 2>/dev/null
