#!/usr/bin/env bash
# Scale verification (#74; run on the Docker host, not inside a container, and not in CI).
#
#   [ROS_DISTRO=jazzy] scripts/scale_test.sh <small|medium|large|large_multi|limit|nav2> [--no-build]
#
#   small        10 scale_load processes (one node each), 100 topics   (~500 pairs)
#   medium       20 processes, 500 topics                               (~2k pairs)
#   large        40 processes, 1000 topics                              (~5k pairs)
#   large_multi  large split across two bridged containers: processes 0-19 in scale_load_a
#                (the tool's container, SHM among them), 20-39 in scale_load_b (UDPv4)
#   limit        records only: 60, 90, 135, ... processes with 25 topics each, until the load
#                or the tool falls over or memory runs out (limit_p<N>.json per step)
#   nav2         Nav2 + TurtleBot3 simulation, Gazebo headless, initial pose only (Jazzy, Kilted)
#
# Every topic of scale_load has one writer and 4 readers in other processes; the nodes also
# keep their parameter services, /rosout and /parameter_events (a nodes x nodes topic), and run
# with FASTDDS_STATISTICS. transport_viz runs inside the load container (`docker compose exec`)
# with FTV_PROFILE=1; scripts/scale_measure.py drives it, checks the budgets and writes
# build/<distro>/scale/<label>.json (plus <label>.viz.json for the web viewer and
# <label>.table-v.txt), then prints the Markdown row for docs/development.md "Scale results".
set -euo pipefail
cd "$(dirname "$0")/.."

scenario="${1:?usage: scripts/scale_test.sh <small|medium|large|large_multi|limit|nav2> [--no-build]}"
no_build="${2:-}"
export ROS_DISTRO="${ROS_DISTRO:-jazzy}"
out="build/${ROS_DISTRO}/scale"
mkdir -p "$out"

cleanup() {
  docker compose --profile scale down --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT

build() {
  [[ "$no_build" == "--no-build" ]] && return
  docker compose build dev >/dev/null
  echo "== building workspace ($ROS_DISTRO)"
  docker compose run --rm hostnet bash -c "colcon build --symlink-install > /dev/null && echo build ok"
}

# measure <label> <service> <expected pairs> <load processes in the service> [scale_measure args]
measure() {
  local label="$1" service="$2" pairs="$3" procs="$4"
  shift 4
  docker compose exec -T "$service" /entrypoint.sh python3 /ws/scripts/scale_measure.py \
    --label "$label" --scenario "$scenario" --out "/ws/$out" --expected-pairs "$pairs" \
    --expected-load-processes "$procs" --date "$(date +%F)" "$@" | tee -a "$out/rows.md"
}

# run_synthetic <label> <processes> <topics> <split|""> [scale_measure args]
run_synthetic() {
  local label="$1" processes="$2" topics="$3" split="$4"
  shift 4
  local readers=$((processes - 1 < 4 ? processes - 1 : 4))
  export SCALE_PROCESSES="$processes" SCALE_TOPICS="$topics"
  local in_a="$processes"
  if [[ -n "$split" ]]; then
    in_a=$((processes / 2))
    export SCALE_A_FIRST=0 SCALE_A_LAST=$((in_a - 1)) SCALE_B_FIRST="$in_a" SCALE_B_LAST=$((processes - 1))
    docker compose --profile scale up -d scale_load_a scale_load_b >/dev/null
  else
    export SCALE_A_FIRST=0 SCALE_A_LAST=$((processes - 1))
    docker compose --profile scale up -d scale_load_a >/dev/null
  fi
  echo "== $label: $processes processes, $topics topics, $((topics * readers)) /scale pairs"
  local rc=0
  measure "$label" scale_load_a "$((topics * readers))" "$in_a" \
    --load "$processes scale_load processes x 1 node, $topics topics, $readers readers each${split:+, split over two containers}" \
    "$@" || rc=$?
  docker compose --profile scale down --remove-orphans >/dev/null 2>&1 || true
  return $rc
}

# The budgets are judged at medium (#167): scale_measure.py --judge exits 1 there when a
# budget with a value fails (Humble's stats_* budgets have none) or the load died. The other
# rungs are recorded only: large takes most of the host and limit is meant to exceed them.
build
case "$scenario" in
  small) run_synthetic small 10 100 "" ;;
  medium) run_synthetic medium 20 500 "" --judge ;;
  large) run_synthetic large 40 1000 "" ;;
  large_multi) run_synthetic large_multi 40 1000 split ;;
  limit)
    processes=60
    while :; do
      label="limit_p${processes}"
      if ! run_synthetic "$label" "$processes" $((processes * 25)) ""; then
        echo "== $label: measurement failed, stopping"
        break
      fi
      if ! python3 - "$out/$label.json" <<'EOF'
import json, sys
r = json.load(open(sys.argv[1]))
mem = r['load_after']['mem_available_mb'] or 0
ok = r['load_healthy'] and mem > 0.1 * r['host']['mem_total_mb']
print(f"== {r['label']}: load healthy {r['load_healthy']}, {mem} MB available")
sys.exit(0 if ok else 1)
EOF
      then
        break
      fi
      processes=$((processes * 3 / 2))
    done
    ;;
  nav2)
    docker compose --profile scale build nav2_tb3 >/dev/null
    docker compose --profile scale up -d nav2_tb3 >/dev/null
    echo "== nav2: Nav2 + TurtleBot3 (Gazebo headless)"
    measure nav2 nav2_tb3 0 0 --warmup 90 --load "nav2_bringup tb3_simulation_launch.py headless"
    ;;
  *)
    echo "unknown scenario: $scenario" >&2
    exit 2
    ;;
esac
