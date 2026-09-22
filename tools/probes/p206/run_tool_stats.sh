#!/bin/bash
set +u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p206"; mkdir -p "$W/out"   # binaries in $W, logs in $W/out
OUT=${1:-$W/out/p206s.json}
source /opt/ros/lyrical/setup.bash
source $ROOT/build/lyrical/install/setup.bash
export LD_LIBRARY_PATH=/opt/ros/lyrical/lib:/opt/ros/lyrical/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
export FASTRTPS_DEFAULT_PROFILES_FILE=$ROOT/build/lyrical/install/fastdds_transport_viz/share/fastdds_transport_viz/config/statistics.xml
cd "$W"
ros2 run demo_nodes_cpp talker > out/talker.log 2>&1 &
T=$!
ros2 run demo_nodes_cpp listener > out/listener.log 2>&1 &
L=$!
env -u FASTDDS_STATISTICS -u FASTRTPS_DEFAULT_PROFILES_FILE ./nonros_app 40 ${P206_SIDES:-a} > out/nonros.log 2>&1 &
N=$!
sleep 8
ros2 run fastdds_transport_viz transport_viz --json --all --stats --quiet 6 > "$OUT" 2>out/tool.err
echo "--- tool stderr ---"; cat out/tool.err
echo "--- nonros MATCH lines ---"; grep MATCH out/nonros.log
kill $N $T $L 2>/dev/null
wait 2>/dev/null
