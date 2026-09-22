#!/bin/bash
# usage: run_case.sh <label> [probe-seconds]   env passed through: P206_*, P213_*, SIDES (ab), ROS_DOMAIN_ID
# Lyrical: nonros213 + ROS side (demo talker+listener, or with P213_MODE=header `ros2 topic pub/echo
# std_msgs/msg/Header` on /chatter) + p210 probe + transport_viz --json --all + table --all.
# Logs to out/<label>.*
set +u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p213"; mkdir -p "$W/out"   # binaries in $W, logs in $W/out
L=$1; SEC=${2:-8}
source /opt/ros/lyrical/setup.bash
source $ROOT/build/lyrical/install/setup.bash
export LD_LIBRARY_PATH=/opt/ros/lyrical/lib:/opt/ros/lyrical/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}
cd "$W"
./nonros213 $((SEC + 26)) ${SIDES:-ab} > out/$L.nonros.log 2>&1 &
N=$!
if [ "$P213_MODE" = header ]; then
  ros2 topic pub -r 2 /chatter std_msgs/msg/Header "{frame_id: from_ros_pub}" > out/$L.talker.log 2>&1 &
  ros2 topic echo /chatter std_msgs/msg/Header > out/$L.listener.log 2>&1 &
else
  ros2 run demo_nodes_cpp talker > out/$L.talker.log 2>&1 &
  ros2 run demo_nodes_cpp listener > out/$L.listener.log 2>&1 &
fi
sleep 3
env -u P213_TP ./probe $SEC > out/$L.probe.log 2>&1
env -u P213_TP ros2 run fastdds_transport_viz transport_viz --json --all > out/$L.json 2> out/$L.tool.err
env -u P213_TP ros2 run fastdds_transport_viz transport_viz --all > out/$L.table.txt 2>> out/$L.tool.err
wait $N
pkill -INT -f "demo_nodes_cpp|ros2 topic"; sleep 1; pkill -f "demo_nodes_cpp|ros2 topic"
wait 2>/dev/null
echo "== $L done"
