#!/bin/bash
# usage: run_case.sh <label> [seconds]   env passed through: P206_*, P210_*, ROS_DOMAIN_ID
# Lyrical: nonros210 (sides ${SIDES:-ab}) + demo talker + listener + p210 probe; logs to out/<label>.*
set +u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p210"; mkdir -p "$W/out"   # binaries in $W, logs in $W/out
L=$1; SEC=${2:-12}
source /opt/ros/lyrical/setup.bash
export LD_LIBRARY_PATH=/opt/ros/lyrical/lib:/opt/ros/lyrical/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}
cd "$W"
./nonros210 $((SEC + 6)) ${SIDES:-ab} > out/$L.nonros.log 2>&1 &
N=$!
[ -z "$NO_ROS" ] && { ros2 run demo_nodes_cpp talker > out/$L.talker.log 2>&1 & }
[ -z "$NO_ROS" ] && { ros2 run demo_nodes_cpp listener > out/$L.listener.log 2>&1 & }
sleep 3
env -u P206_NO_TYPEOBJECT -u P206_EXT ./probe $SEC > out/$L.probe.log 2>&1
wait $N
pkill -INT -f demo_nodes_cpp; sleep 1; pkill -f demo_nodes_cpp
wait 2>/dev/null
echo "== $L done"
