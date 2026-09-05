#!/usr/bin/env bash
# Regenerates the colored output examples used in README and docs (run on the Docker host):
#   docs/images/example-table.ansi / .svg   ros2 transport list -v --stats --color always
#   docs/images/example-watch.ansi / .svg   one frame of --watch with +, ~ and - marks
#
# Starts demo and verification nodes in the dev container, captures real transport_viz
# output with ANSI colors, and converts it with scripts/ansi2svg.py. Not run in CI.
set -euo pipefail
cd "$(dirname "$0")/.."
out=docs/images
mkdir -p "$out"

docker compose build dev >/dev/null
# The container runs as root; hand the captures back through a tar stream so that the
# files in docs/images stay owned by the user running this script.
docker compose run --rm -T dev bash -c '
set -e
colcon build --symlink-install > /dev/null && source install/setup.bash
CFG=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config
STATS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC"
export FASTDDS_STATISTICS="$STATS"
FASTRTPS_DEFAULT_PROFILES_FILE=$CFG/statistics.xml ros2 run demo_nodes_cpp talker > /dev/null 2>&1 &
FASTRTPS_DEFAULT_PROFILES_FILE=$CFG/statistics.xml ros2 run demo_nodes_cpp listener > /dev/null 2>&1 &
FASTRTPS_DEFAULT_PROFILES_FILE=$CFG/statistics.xml FASTDDS_BUILTIN_TRANSPORTS=UDPv4 ros2 run demo_nodes_cpp listener --ros-args -r __node:=listener_udp > /dev/null 2>&1 &
FASTRTPS_DEFAULT_PROFILES_FILE=$CFG/datasharing_auto_stats.xml RMW_FASTRTPS_USE_QOS_FROM_XML=1 ros2 run fastdds_transport_viz bounded_pub > /dev/null 2>&1 &
FASTRTPS_DEFAULT_PROFILES_FILE=$CFG/datasharing_auto_stats.xml RMW_FASTRTPS_USE_QOS_FROM_XML=1 ros2 run fastdds_transport_viz bounded_sub > /dev/null 2>&1 &
sleep 5
unset FASTDDS_STATISTICS
echo "== one-shot table" >&2
ros2 run fastdds_transport_viz transport_viz -v --stats --color always --topic "^/(chatter|bounded)$" --timeout 6 --quiet 0 > /tmp/example-table.ansi
echo "== watch frames (-v on /chatter: a listener appears and the UDP listener goes away in one refresh)" >&2
ros2 run fastdds_transport_viz transport_viz --watch --stats -v --color always --topic "^/chatter$" --interval 3 --timeout 3 > /tmp/watch.ansi 2>/dev/null &
W=$!
sleep 10
# removal is noticed at once, discovery of the new node takes a moment: remove first
pkill -f "__node:=listener_udp" || true
FASTDDS_STATISTICS="$STATS" FASTRTPS_DEFAULT_PROFILES_FILE=$CFG/statistics.xml ros2 run demo_nodes_cpp listener --ros-args -r __node:=listener_new > /dev/null 2>&1 &
sleep 13
kill $W; wait $W 2>/dev/null || true
pkill -f demo_nodes_cpp || true; pkill -f bounded_ || true
python3 - <<PY
import re, sys
text = open("/tmp/watch.ansi", encoding="utf-8", errors="replace").read()
frames = [f for f in re.split(r"(?=transport_viz  domain )", text) if f.startswith("transport_viz")]
def score(f):
    plain = re.sub(r"\x1b\[[0-9;]*m", "", f)
    kinds = sum(("\n" + m + " ") in plain for m in "+~-")
    summary = 5 if ("changes:" in plain and "changes: none" not in plain) else 0
    return kinds * 10 + summary + frames.index(f)   # later frames win ties
best = max(frames, key=score) if frames else text
open("/tmp/example-watch.ansi", "w").write(best)
print("frames:", len(frames), "chosen score:", score(best), file=sys.stderr)
PY
tar -C /tmp -c example-table.ansi example-watch.ansi
' | tar -x -C "$out"

python3 scripts/ansi2svg.py "$out/example-table.ansi" "$out/example-table.svg" --title '$ ros2 transport list -v --stats --topic "^/(chatter|bounded)$"' --max-cols 132
python3 scripts/ansi2svg.py "$out/example-watch.ansi" "$out/example-watch.svg" --title '$ ros2 transport list --watch --stats -v --interval 3 --topic "^/chatter$"   (one frame)' --max-cols 132
ls -l "$out"/example-*
