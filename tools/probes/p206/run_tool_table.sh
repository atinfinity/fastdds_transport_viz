#!/bin/bash
# usage: run_tool.sh <out.json>  -- runs transport_viz --all while the non-ROS app
# and a ROS 2 demo talker/listener share the topic rt/chatter.
set +u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p206"; mkdir -p "$W/out"   # binaries in $W, logs in $W/out
OUT=${1:-$W/out/p206.json}
source /opt/ros/lyrical/setup.bash
source $ROOT/build/lyrical/install/setup.bash
export LD_LIBRARY_PATH=/opt/ros/lyrical/lib:/opt/ros/lyrical/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}
cd "$W"
./nonros_app 30 ${P206_SIDES:-a} > out/nonros.log 2>&1 &
N=$!
ros2 run demo_nodes_cpp talker > out/talker.log 2>&1 &
T=$!
ros2 run demo_nodes_cpp listener > out/listener.log 2>&1 &
L=$!
sleep 6
ros2 run fastdds_transport_viz transport_viz --all > "$OUT" 2>out/tool.err
echo "--- tool stderr ---"; cat out/tool.err
kill $N $T $L 2>/dev/null
wait 2>/dev/null
