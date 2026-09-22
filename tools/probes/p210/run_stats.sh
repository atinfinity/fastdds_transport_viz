#!/bin/bash
# #210: talker + listener + nonros210 side b (P206_NO_TYPEOBJECT=1), all with statistics on,
# then transport_viz --json --all --stats. Side b's reader drops some talker samples in take().
set +u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p210"; mkdir -p "$W/out"   # binaries in $W, logs in $W/out
source /opt/ros/lyrical/setup.bash
source $ROOT/build/lyrical/install/setup.bash
export LD_LIBRARY_PATH=/opt/ros/lyrical/lib:/opt/ros/lyrical/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
export FASTRTPS_DEFAULT_PROFILES_FILE=$ROOT/build/lyrical/install/fastdds_transport_viz/share/fastdds_transport_viz/config/statistics.xml
cd "$W"
T=${TAG:-stats}
ros2 run demo_nodes_cpp listener > out/$T.listener.log 2>&1 &
P206_NO_TYPEOBJECT=1 P210_NO_WRITER=1 ./nonros210 ${NSEC:-40} b > out/$T.nonros.log 2>&1 &
N=$!
if [ -n "$EARLY" ]; then
  (sleep ${TDELAY:-2}; ros2 run demo_nodes_cpp talker > out/$T.talker.log 2>&1) &
  env -u FASTDDS_STATISTICS -u FASTRTPS_DEFAULT_PROFILES_FILE ros2 run fastdds_transport_viz transport_viz --json --all --stats --quiet 6 > out/$T.json 2> out/$T.tool.err
else
ros2 run demo_nodes_cpp talker > out/$T.talker.log 2>&1 &
sleep 8
env -u FASTDDS_STATISTICS -u FASTRTPS_DEFAULT_PROFILES_FILE ros2 run fastdds_transport_viz transport_viz --json --all --stats --quiet 6 > out/$T.json 2> out/$T.tool.err
env -u FASTDDS_STATISTICS -u FASTRTPS_DEFAULT_PROFILES_FILE ros2 run fastdds_transport_viz transport_viz --all --stats --quiet 6 > out/$T.table.txt 2>> out/$T.tool.err
fi
wait $N
pkill -f demo_nodes_cpp
