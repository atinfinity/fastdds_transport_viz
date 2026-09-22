#!/bin/bash
# usage: run_probe.sh [seconds]  -- runs the #193 listener probe while the non-ROS app
# and a ROS 2 demo talker/listener share the topic rt/chatter.
set +u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p206"; mkdir -p "$W/out"   # binaries in $W, logs in $W/out
SEC=${1:-14}
source /opt/ros/lyrical/setup.bash
export LD_LIBRARY_PATH=/opt/ros/lyrical/lib:/opt/ros/lyrical/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}
cd "$W"
./nonros_app $((SEC + 8)) ${P206_SIDES:-ab} > out/nonros.log 2>&1 &
N=$!
ros2 run demo_nodes_cpp talker > out/talker.log 2>&1 &
T=$!
sleep 4
"$ROOT/build/probes/p193/probe_lyrical" "$SEC"
echo "=========== nonros_app log ==========="
cat out/nonros.log
kill $N $T 2>/dev/null
wait 2>/dev/null
