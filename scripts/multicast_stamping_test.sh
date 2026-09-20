#!/usr/bin/env bash
# Does a multi-homed sender inflate the receiver's RTPS_LOST? (#130)
#
#   scripts/multicast_stamping_test.sh [rung...]     # default: every rung
#   ROS_DISTRO=lyrical scripts/multicast_stamping_test.sh mcast2
#
# Fast DDS stamps the statistics sequence number inside the transport's per-socket send()
# (UDPTransportInterface.cpp, one line before send_to), from a counter keyed by destination
# locator alone. A multicast send goes out on the any-address socket (pinned to localhost)
# plus one socket per interface, so a sender with N interfaces burns N+1 sequence numbers
# per logical message; a receiver in another network namespace gets one copy and reports
# the rest as lost. RTPS_SENT, emitted once per message after the socket loop, is the
# denominator: lost / sent == N is the whole verdict, and it needs no timing accuracy.
#
#   mcast1 mcast2 mcast3   the sender on 1, 2 and 3 interfaces => ratio 1, 2 and 3
#   control                sender and receiver in one network namespace => ratio 0
#   whitelist              an interface whitelist on unicast traffic => no false loss,
#                          duplicate datagrams instead (the third check of the issue)
#
# Deliberately not a scripts/integration_test.sh scenario and not in CI: this measures a
# Fast DDS behaviour, not ours, and it must not start failing the day eProsima fixes it.
# The `multicast` compose profile keeps its services out of every other run.
set -euo pipefail
cd "$(dirname "$0")/.."

rungs=("$@")
if ((${#rungs[@]} == 0)); then rungs=(mcast1 mcast2 mcast3 control whitelist); fi
out_dir="${TMPDIR:-/tmp}"
runs=3
# 45 s, not the 15 s of the other scenarios: RTPS_LOST and RTPS_SENT are separate
# statistics samples, so each counter's window starts and ends a moment apart. That skew is
# a fixed couple of seconds, and only a long window makes it small next to the count.
timeout="${FTV_MCAST_TIMEOUT:-45}"
distro="${ROS_DISTRO:-jazzy}"
compose=(docker compose --profile multicast)

cleanup() { "${compose[@]}" down --remove-orphans >/dev/null 2>&1 || true; }
trap cleanup EXIT

# Every node must still be alive when the tool runs: one that died would only show as an
# empty document (#171).
require_running() {
  local name id ok=0
  for name in "$@"; do
    id="$("${compose[@]}" ps -q "$name" 2>/dev/null || true)"
    if [ -z "$id" ] || [[ "$(docker inspect -f '{{.State.Running}}' "$id" 2>/dev/null)" != true ]]; then
      echo "ERROR: $name is not running; its output:" >&2
      "${compose[@]}" logs "$name" 2>&1 | tail -20 >&2 || true
      ok=1
    fi
  done
  return $ok
}

build() {
  "${compose[@]}" build dev >/dev/null
  echo "== building workspace ($distro)"
  # In `hostnet`, like integration_test.sh: a host-network node started right after a
  # container left the project bridge would otherwise get another Fast DDS host id.
  "${compose[@]}" run --rm hostnet bash -c "colcon build --symlink-install > /dev/null && echo build ok"
}

# rung <label> <tool service> <report args...> -- <compose service...>
rung() {
  local label="$1" viz_service="$2"; shift 2
  local report=() services=() docs=()
  while [ "$1" != "--" ]; do report+=("$1"); shift; done
  shift
  services=("$@")

  echo
  echo "===== $label: ${services[*]}"
  "${compose[@]}" up -d "${services[@]}"
  # Let discovery and the statistics writers settle before the first run: a counter the tool
  # starts reading late loses a slice of the window, and the ratio with it.
  sleep 15
  require_running "${services[@]}" || return 1
  "${compose[@]}" logs talker_mcast talker_mcast_hostnet talker_whitelist 2>/dev/null \
    | grep -E "^[a-z_]+-1 *\| (==|[a-z0-9@]+ +(UP|DOWN))" || true

  local index doc
  for index in $(seq 1 $runs); do
    doc="$out_dir/multicast_stamping_${label}_$index.json"
    echo "== run $index/$runs (transport_viz --stats --timeout $timeout, $viz_service)"
    "${compose[@]}" run --rm -T "$viz_service" \
      ros2 run fastdds_transport_viz transport_viz --json --stats --timeout "$timeout" --quiet 0 \
      > "$doc"
    docs+=("$doc")
  done
  local status=0
  require_running "${services[@]}" || status=1
  python3 scripts/multicast_stamping_report.py --label "$label" "${report[@]}" "${docs[@]}" || status=1
  "${compose[@]}" rm -sf "${services[@]}" >/dev/null 2>&1 || true
  return $status
}

build
failed=()
for name in "${rungs[@]}"; do
  case "$name" in
    mcast[123])
      export FTV_MCAST_IFACES="${name#mcast}"
      rung "$name" dev --multicast-expect "${name#mcast}" -- talker_mcast listener_mcast listener_mcast2 \
        || failed+=("$name")
      ;;
    control)
      rung control hostnet --multicast-expect 0 -- talker_mcast_hostnet listener_mcast_hostnet listener_mcast_hostnet2 \
        || failed+=("$name")
      ;;
    whitelist)
      rung whitelist dev --unicast-expect 0 -- talker_whitelist listener_whitelist \
        || failed+=("$name")
      ;;
    *)
      echo "usage: $0 [mcast1|mcast2|mcast3|control|whitelist ...]" >&2
      exit 2
      ;;
  esac
done

echo
if ((${#failed[@]})); then
  echo "FAIL: ${failed[*]} did not meet the expected ratio (see the tables above)"
  exit 1
fi
echo "PASS: every rung met its expected ratio"
