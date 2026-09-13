#!/usr/bin/env bash
# Multi-container integration tests (run on the Docker host, not inside a container).
#
#   scripts/integration_test.sh [multi_container|stats_multi_container|hostnet_shm|large_data_tcp|udpv6_multi_container|easy_mode_shm|easy_mode_tcp|all]
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
#   large_data_tcp         bridged containers with FASTDDS_BUILTIN_TRANSPORTS=LARGE_DATA
#                          (UDPv4 discovery, TCPv4 + SHM user data) and statistics.
#                          Expect /chatter = TCPv4, "common-tcpv4-locator", measured TCPv4.
#   udpv6_multi_container  bridged containers (the project network has IPv6) with
#                          FASTDDS_BUILTIN_TRANSPORTS=DEFAULTv6. Expect /chatter = UDPv6.
#   easy_mode_shm          hostnet containers with ROS2_EASY_MODE=127.0.0.1 (Fast DDS 3.2+:
#                          ROS_DISTRO=kilted|lyrical|rolling; skipped otherwise). One Discovery
#                          Server per host, P2P transport. Expect /chatter = SHM, no multicast.
#   easy_mode_tcp          bridged containers with fixed addresses, ROS2_EASY_MODE pointing at
#                          the talker's (Fast DDS 3.2+, skipped otherwise); transport_viz runs
#                          on the talker's host with --stats. Expect /chatter = TCPv4,
#                          "common-tcpv4-locator", measured TCPv4, no multicast.
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

# run_viz <service> <output file> [transport_viz args...]; VIZ_ENV holds extra -e flags
VIZ_ENV=()
run_viz() {
  local service="$1" out="$2"; shift 2
  echo "== running transport_viz ($service) $*"
  docker compose run --rm -T ${VIZ_ENV[@]+"${VIZ_ENV[@]}"} "$service" \
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
    # the nodes' /dev/shm is not the tool's (separate IPC namespaces)
    shm = doc['shm']
    assert shm['available'] and not shm['nodes_visible'], shm
    assert shm['other_host_participants'] >= 2 and shm['checked_ports'] == [], shm
    assert 'shm-not-visible' in shm['warnings'], shm
    print('PASS: /chatter across bridged containers uses UDPv4 (different-host); shm-not-visible reported')
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
elif scenario == 'large_data_tcp':
    assert doc['stats']['enabled'] and doc['stats']['samples'] > 0, doc['stats']
    assert p['transport'] == 'TCPv4', p
    assert 'different-host' in p['reasons'] and 'common-tcpv4-locator' in p['reasons'], p
    assert p['measured']['transports'] == ['TCPv4'], p
    assert 'measured-tcpv4-traffic' in p['reasons'], p
    assert 'measured-transport-mismatch' not in p['warnings'], p
    print(f"PASS: LARGE_DATA across bridged containers uses TCPv4, measured TCPv4 "
          f"({p['measured']['packets']} packets)")
elif scenario == 'udpv6_multi_container':
    assert p['transport'] == 'UDPv6', p
    assert 'different-host' in p['reasons'] and 'common-udpv6-locator' in p['reasons'], p
    print('PASS: DEFAULTv6 across bridged containers uses UDPv6 (different-host)')
elif scenario == 'easy_mode_shm':
    assert p['transport'] == 'SHM', p
    assert 'same-host-guid' in p['reasons'] and 'both-shm-locators' in p['reasons'], p
    for e in chatter['writers'] + chatter['readers']:   # P2P: SHM + TCPv4, no multicast
        assert e['multicast_locators'] == [], e
        assert {l['kind'] for l in e['unicast_locators']} == {'SHM', 'TCPv4'}, e
    print('PASS: Easy Mode on one host uses SHM (same-host-guid); P2P announces no multicast')
elif scenario == 'easy_mode_tcp':
    assert doc['stats']['enabled'] and doc['stats']['samples'] > 0, doc['stats']
    assert p['transport'] == 'TCPv4', p
    assert 'different-host' in p['reasons'] and 'common-tcpv4-locator' in p['reasons'], p
    assert p['measured']['transports'] == ['TCPv4'], p
    assert 'measured-tcpv4-traffic' in p['reasons'], p
    assert 'measured-transport-mismatch' not in p['warnings'], p
    for e in chatter['writers'] + chatter['readers']:
        # P2P: TCPv4 (+ SHM on the tool's own host), no multicast
        kinds = {l['kind'] for l in e['unicast_locators']}
        assert e['multicast_locators'] == [] and 'TCPv4' in kinds <= {'SHM', 'TCPv4'}, e
    print(f"PASS: Easy Mode across bridged containers uses TCPv4, measured TCPv4 "
          f"({p['measured']['packets']} packets); P2P announces no multicast")
elif scenario == 'hostnet_shm':
    assert p['transport'] == 'SHM', p
    assert 'same-host-guid' in p['reasons'] and 'both-shm-locators' in p['reasons'], p
    assert 'host-id-match-but-ip-differs' not in p['warnings'], p
    assert p['writer_host'] == p['reader_host'], p
    shm = doc['shm']
    assert shm['available'] and shm['nodes_visible'], shm
    assert 'shm-not-visible' not in shm['warnings'], shm
    assert shm['segments'] - shm['stale_segments'] >= 2, shm   # talker and listener alive
    print('PASS: /chatter across host-network/IPC containers uses SHM (same-host-guid); their segments are visible')
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

scenario_large_data_tcp() {
  local out="$out_dir/transport_viz_large_data_tcp.json"
  echo "== starting talker_large_data / listener_large_data containers"
  docker compose up -d talker_large_data listener_large_data
  sleep 3
  VIZ_ENV=(-e FASTDDS_BUILTIN_TRANSPORTS=LARGE_DATA)   # the tool needs TCP to receive statistics
  local attempt
  for attempt in 1 2 3; do
    run_viz dev "$out" --stats
    if assert large_data_tcp "$out"; then VIZ_ENV=(); return 0; fi
    echo "-- attempt $attempt: statistics incomplete, retrying"
  done
  VIZ_ENV=()
  return 1
}

scenario_udpv6_multi_container() {
  local out="$out_dir/transport_viz_udpv6_multi_container.json"
  echo "== starting talker_udpv6 / listener_udpv6 containers"
  docker compose up -d talker_udpv6 listener_udpv6
  sleep 3
  VIZ_ENV=(-e FASTDDS_BUILTIN_TRANSPORTS=DEFAULTv6)
  run_viz dev "$out"
  VIZ_ENV=()
  assert udpv6_multi_container "$out"
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

# Easy Mode needs Fast DDS 3.2+ (ROS 2 Kilted or later); the default jazzy image ignores it.
has_easy_mode() {
  case "${ROS_DISTRO:-jazzy}" in kilted|lyrical|rolling) return 0 ;; *) return 1 ;; esac
}

scenario_easy_mode_shm() {
  local out="$out_dir/transport_viz_easy_mode_shm.json"
  if ! has_easy_mode; then echo "SKIP: easy_mode_shm needs ROS_DISTRO=kilted|lyrical|rolling"; return 0; fi
  echo "== starting talker / listener on the host network in Easy Mode"
  docker compose run --rm -d --name tv_easy_talker -e ROS2_EASY_MODE=127.0.0.1 hostnet \
    ros2 run demo_nodes_cpp talker >/dev/null
  docker compose run --rm -d --name tv_easy_listener -e ROS2_EASY_MODE=127.0.0.1 hostnet \
    ros2 run demo_nodes_cpp listener >/dev/null
  run_containers+=(tv_easy_talker tv_easy_listener)
  sleep 3
  VIZ_ENV=(-e ROS2_EASY_MODE=127.0.0.1)   # the tool must join Easy Mode to see the nodes
  run_viz hostnet "$out" --locators
  VIZ_ENV=()
  assert easy_mode_shm "$out"
}

scenario_easy_mode_tcp() {
  local out="$out_dir/transport_viz_easy_mode_tcp.json"
  if ! has_easy_mode; then echo "SKIP: easy_mode_tcp needs ROS_DISTRO=kilted|lyrical|rolling"; return 0; fi
  echo "== starting talker_easy_mode / listener_easy_mode containers"
  docker compose up -d talker_easy_mode listener_easy_mode
  sleep 3
  # The tool runs on the talker's host (docs/how-it-works.md, Easy Mode): a host's Discovery
  # Server relays an endpoint only once it has resolved its type, which a host without a
  # node of that type never manages.
  local attempt
  for attempt in 1 2 3; do
    echo "== running transport_viz (talker_easy_mode) --stats"
    docker compose exec -T talker_easy_mode /entrypoint.sh \
      ros2 run fastdds_transport_viz transport_viz --json --timeout 6 --quiet 0 --stats --locators > "$out"
    jq '.topics[] | select(.topic=="/chatter") | .pairs[] | {transport, measured: .measured.transports, reasons, warnings, writer_host, reader_host}' "$out"
    if assert easy_mode_tcp "$out"; then return 0; fi
    echo "-- attempt $attempt: statistics incomplete, retrying"
  done
  return 1
}

build
case "$scenario" in
  multi_container|stats_multi_container|hostnet_shm|large_data_tcp|udpv6_multi_container|easy_mode_shm|easy_mode_tcp)
    "scenario_$scenario" ;;
  all)
    for s in multi_container stats_multi_container hostnet_shm large_data_tcp udpv6_multi_container easy_mode_shm easy_mode_tcp; do
      echo; echo "#### $s"
      "scenario_$s"
      cleanup
    done ;;
  *)
    echo "usage: $0 [multi_container|stats_multi_container|hostnet_shm|large_data_tcp|udpv6_multi_container|easy_mode_shm|easy_mode_tcp|all]" >&2; exit 2 ;;
esac
