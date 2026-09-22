#!/bin/bash
# usage: run_case.sh <label> <pub_ws> <sub_ws> [pub cpp|py] [sub cpp|py]
# env: ROS_DOMAIN_ID; PUB_RMW / SUB_RMW (default rmw_fastrtps_cpp) for the two nodes; probe+tool use default rmw
# pub node sources ws_<pub_ws>, sub node sources ws_<sub_ws>; probe + transport_viz --json/table. Logs to out/<label>.*
set +u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p213_ros"; mkdir -p "$W/out"   # ws_*/{build,install,log} and logs (out/) in $W
L=$1; PW=$2; SW=$3; PK=${4:-cpp}; SK=${5:-cpp}
cd "$W"
source /opt/ros/lyrical/setup.bash
export LD_LIBRARY_PATH=/opt/ros/lyrical/lib:/opt/ros/lyrical/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}
side() { # ws kind role
  ( source "$W/ws_$1/install/setup.bash"
    if [ $2 = cpp ]; then exec timeout -s INT 16 ros2 run p213_msgs foo_$3
    elif [ $3 = pub ]; then exec timeout -s INT 16 ros2 topic pub -r 2 /p213 p213_msgs/msg/Foo "{s: py_pub}"
    else exec timeout -s INT 16 ros2 topic echo /p213 p213_msgs/msg/Foo; fi )
}
RMW_IMPLEMENTATION=${PUB_RMW:-${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}} side $PW $PK pub > out/$L.pub.log 2>&1 &
RMW_IMPLEMENTATION=${SUB_RMW:-${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}} side $SW $SK sub > out/$L.sub.log 2>&1 &
sleep 3
"$ROOT/build/probes/p213/probe" 6 > out/$L.probe.log 2>&1
( source "$ROOT/build/lyrical/install/setup.bash"
  ros2 run fastdds_transport_viz transport_viz --json --all > out/$L.json 2> out/$L.tool.err
  ros2 run fastdds_transport_viz transport_viz --all > out/$L.table.txt 2>> out/$L.tool.err )
wait
echo "== $L done"
