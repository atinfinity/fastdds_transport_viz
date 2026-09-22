#!/bin/bash
# Lyrical, UDP only: sniff SEDP for talker/listener + nonros210 ab under 4 configs.
set +u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p210"; mkdir -p "$W/out"   # binaries in $W, logs in $W/out
source /opt/ros/lyrical/setup.bash
export LD_LIBRARY_PATH=/opt/ros/lyrical/lib:/opt/ros/lyrical/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}
export FASTDDS_BUILTIN_TRANSPORTS=UDPv4
cd "$W"
dom=44
for cfg in "default" "noto:P206_NO_TYPEOBJECT=1" "final:P206_EXT=final" "noto_final:P206_NO_TYPEOBJECT=1 P206_EXT=final"; do
  name=${cfg%%:*}; envs=""; [[ "$cfg" == *:* ]] && envs=${cfg#*:}
  export ROS_DOMAIN_ID=$dom
  python3 "$HERE/sniff_sedp.py" 9 > out/sniff_$name.log 2>&1 &
  S=$!
  sleep 0.5
  env $envs ./nonros210 8 ab > out/sniff_$name.nonros.log 2>&1 &
  ros2 run demo_nodes_cpp talker > /dev/null 2>&1 &
  ros2 run demo_nodes_cpp listener > /dev/null 2>&1 &
  wait $S
  pkill -f demo_nodes_cpp; pkill nonros210; sleep 1
  echo "== $name (domain $dom) env=[$envs]"; grep "rt/chatter" out/sniff_$name.log
  dom=$((dom+1))
done
