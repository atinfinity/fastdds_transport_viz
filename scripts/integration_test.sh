#!/usr/bin/env bash
# Multi-container integration tests (run on the Docker host, not inside a container).
#
#   scripts/integration_test.sh [multi_container|stats_multi_container|hostnet_shm|all]
#
#   multi_container        talker and listener in two bridged containers (separate
#                          network and IPC namespaces => different Fast DDS host ids).
#                          Expect /chatter = UDPv4, reason "different-host".
#   stats_multi_container  same, nodes started with FASTDDS_STATISTICS and transport_viz
#                          run with --stats. Expect measured UDPv4 and two different
#                          PHYSICAL_DATA host names.
#   hostnet_shm            talker and listener in two containers that share the Docker
#                          host's network and IPC namespaces (same host id, same
#                          /dev/shm). Expect /chatter = SHM, reason "same-host-guid".
#
# transport_viz always runs in a third container on the same scope as the nodes.
# Results are written to ${TMPDIR:-/tmp}/transport_viz_<scenario>.json.
set -euo pipefail
cd "$(dirname "$0")/.."

scenario="${1:-multi_container}"
out_dir="${TMPDIR:-/tmp}"
run_containers=()

cleanup() {
  if ((${#run_containers[@]})); then docker rm -f "${run_containers[@]}" >/dev/null 2>&1 || true; fi
  docker compose down --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT

build() {
  docker compose build dev >/dev/null
  echo "== building workspace"
  docker compose run --rm dev bash -c \
    "colcon build --symlink-install > /dev/null && echo build ok"
}

# run_viz <service> <output file> [transport_viz args...]
run_viz() {
  local service="$1" out="$2"; shift 2
  echo "== running transport_viz ($service) $*"
  docker compose run --rm -T "$service" \
    ros2 run fastdds_transport_viz transport_viz --json --timeout 6 --quiet 0 "$@" > "$out"
  jq '.topics[] | select(.topic=="/chatter") | .pairs[] | {transport, measured: .measured.transports, reasons, warnings, writer_host, reader_host}' "$out"
}

# start_hostnet <name> <ros2 run args...>: detached one-off container on host net/IPC
start_hostnet() {
  local name="$1"; shift
  docker compose run --rm -d --name "$name" hostnet ros2 run "$@" >/dev/null
  run_containers+=("$name")
}

assert() {  # assert <scenario> <json file>
  python3 - "$1" "$2" <<'PY'
import json, sys
scenario, path = sys.argv[1], sys.argv[2]
doc = json.load(open(path))
chatter = next(t for t in doc['topics'] if t['topic'] == '/chatter')
assert len(chatter['pairs']) == 1, chatter
p = chatter['pairs'][0]
if scenario == 'multi_container':
    assert p['transport'] == 'UDPv4', p
    assert 'different-host' in p['reasons'], p
    assert p['writer_host'] != p['reader_host'], p
    print('PASS: /chatter across bridged containers uses UDPv4 (different-host)')
elif scenario == 'stats_multi_container':
    assert doc['stats']['enabled'] and doc['stats']['samples'] > 0, doc['stats']
    assert p['transport'] == 'UDPv4', p
    assert 'different-host' in p['reasons'], p
    assert p['measured']['transports'] == ['UDPv4'], p
    assert 'measured-udpv4-traffic' in p['reasons'], p
    assert 'measured-transport-mismatch' not in p['warnings'], p
    w, r = chatter['writers'][0], chatter['readers'][0]
    assert w['host_name'] and r['host_name'], (w, r)
    assert w['host_name'].split(':')[0] != r['host_name'].split(':')[0], (w, r)
    assert p['writer_host'] == w['host_name'].split(':')[0], (p, w)
    print(f"PASS: --stats measured UDPv4 between hosts {p['writer_host']} and {p['reader_host']}")
elif scenario == 'hostnet_shm':
    assert p['transport'] == 'SHM', p
    assert 'same-host-guid' in p['reasons'] and 'both-shm-locators' in p['reasons'], p
    assert 'host-id-match-but-ip-differs' not in p['warnings'], p
    assert p['writer_host'] == p['reader_host'], p
    print('PASS: /chatter across host-network/IPC containers uses SHM (same-host-guid)')
else:
    sys.exit(f'unknown scenario {scenario}')
PY
}

scenario_multi_container() {
  local out="$out_dir/transport_viz_multi_container.json"
  echo "== starting talker / listener containers"
  docker compose up -d talker listener
  sleep 3
  run_viz dev "$out"
  assert multi_container "$out"
}

scenario_stats_multi_container() {
  local out="$out_dir/transport_viz_stats_multi_container.json"
  echo "== starting talker_stats / listener_stats containers"
  docker compose up -d talker_stats listener_stats
  sleep 3
  local attempt
  for attempt in 1 2 3; do   # counters need a moment to accumulate
    run_viz dev "$out" --stats
    if assert stats_multi_container "$out"; then return 0; fi
    echo "-- attempt $attempt: statistics incomplete, retrying"
  done
  return 1
}

scenario_hostnet_shm() {
  local out="$out_dir/transport_viz_hostnet_shm.json"
  echo "== starting talker / listener on the host network and IPC namespace"
  start_hostnet tv_hostnet_talker demo_nodes_cpp talker
  start_hostnet tv_hostnet_listener demo_nodes_cpp listener
  sleep 3
  run_viz hostnet "$out"
  assert hostnet_shm "$out"
}

build
case "$scenario" in
  multi_container|stats_multi_container|hostnet_shm)
    "scenario_$scenario" ;;
  all)
    for s in multi_container stats_multi_container hostnet_shm; do
      echo; echo "#### $s"
      "scenario_$s"
      cleanup
    done ;;
  *)
    echo "usage: $0 [multi_container|stats_multi_container|hostnet_shm|all]" >&2; exit 2 ;;
esac
