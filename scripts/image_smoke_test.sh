#!/usr/bin/env bash
# Smoke test of the published image (the `release` stage of docker/Dockerfile, #78), run on
# the Docker host by .github/workflows/release.yml before anything is pushed, and by hand:
#
#   docker build --target release --build-arg ROS_DISTRO=jazzy -t ftv:jazzy -f docker/Dockerfile .
#   scripts/image_smoke_test.sh ftv:jazzy
#
# Everything runs the way getting-started.md tells users to: containers of the image on the
# host network and IPC namespace. A talker and a listener run as a non-root user (uid 1000,
# like nodes started on the host), the tool as root, the image's default user. Checks:
#   - `ros2 transport list --help` and the default command (`ros2 transport list`) exit 0
#   - /chatter is predicted SHM (reason same-host-guid)
#   - with statistics on the nodes (not on Humble, whose Fast DDS has no statistics module)
#     `--stats` measures SHM on /chatter
#   - transport_viz_web serves the viewer and /latest.json on 127.0.0.1:8765
# Every container, even the ones that need neither, is on the host network: a container on a
# Docker bridge brings docker0 up and down, which changes the Fast DDS host id of the nodes
# started around it, so they no longer take each other's SHM locators (same effect as the
# hostnet note in scripts/integration_test.sh). Needs docker and jq; leaves no container
# behind.
set -euo pipefail

image="${1:?usage: $0 <image>}"
host=(--net host --ipc host)
user=(--user 1000:1000 -e HOME=/tmp -e ROS_HOME=/tmp/ros)
distro="$(docker run --rm "${host[@]}" "$image" printenv ROS_DISTRO)"
profile=/opt/fastdds_transport_viz/share/fastdds_transport_viz/config/statistics.xml
stats_env=()
if [[ "$distro" != humble ]]; then
  stats_env=(-e "FASTRTPS_DEFAULT_PROFILES_FILE=$profile" -e "FASTDDS_DEFAULT_PROFILES_FILE=$profile"
             -e "FASTDDS_STATISTICS=RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC")
fi
containers=(ftv_smoke_talker ftv_smoke_listener ftv_smoke_web)
cleanup() { docker rm -f "${containers[@]}" >/dev/null 2>&1 || true; }
trap cleanup EXIT
cleanup

fail() {
  echo "FAIL: $*" >&2
  for c in "${containers[@]}"; do
    if docker inspect "$c" >/dev/null 2>&1; then
      echo "--- $c" >&2
      docker logs "$c" 2>&1 | tail -20 >&2
    fi
  done
  exit 1
}

echo "== $image (ROS 2 $distro)"
docker run --rm "${host[@]}" "$image" ros2 transport list --help >/dev/null || fail "ros2 transport list --help"

# Not --rm: a node that dies must leave its output for fail().
docker run -d --name ftv_smoke_talker "${host[@]}" "${user[@]}" ${stats_env[@]+"${stats_env[@]}"} \
  "$image" ros2 run demo_nodes_cpp talker >/dev/null
docker run -d --name ftv_smoke_listener "${host[@]}" "${user[@]}" ${stats_env[@]+"${stats_env[@]}"} \
  "$image" ros2 run demo_nodes_cpp listener >/dev/null
sleep 3
for c in ftv_smoke_talker ftv_smoke_listener; do
  [[ "$(docker inspect -f '{{.State.Running}}' "$c")" == true ]] || fail "$c is not running"
done

out="$(docker run --rm "${host[@]}" "$image")" || fail "the default command"
grep -q '/chatter' <<<"$out" || fail "the default command shows no /chatter: $out"
echo "PASS: --help and the default command"

doc="$(docker run --rm "${host[@]}" "$image" ros2 transport list --json --timeout 6)"
jq -e '[.topics[] | select(.topic == "/chatter") | .pairs[]
        | select(.transport == "SHM" and (.reasons | index("same-host-guid")))] | length == 1' \
  <<<"$doc" >/dev/null || fail "/chatter is not one SHM pair: $(jq -c '.topics' <<<"$doc")"
echo "PASS: /chatter predicted SHM"

if [[ "$distro" != humble ]]; then
  doc="$(docker run --rm "${host[@]}" "$image" ros2 transport list --json --stats)"
  jq -e '[.topics[] | select(.topic == "/chatter") | .pairs[]
          | select(.measured.transports == ["SHM"])] | length == 1' <<<"$doc" >/dev/null \
    || fail "--stats did not measure SHM on /chatter: $(jq -c '.topics[] | select(.topic == "/chatter")' <<<"$doc")"
  echo "PASS: --stats measured SHM"
fi

docker run -d --name ftv_smoke_web "${host[@]}" "$image" \
  ros2 run fastdds_transport_viz transport_viz_web --interval 1 >/dev/null
docker run --rm -i "${host[@]}" "$image" python3 - <<'PY' || fail "transport_viz_web"
import json, time, urllib.request
base = 'http://127.0.0.1:8765'
for _ in range(30):
    try:
        with urllib.request.urlopen(base + '/latest.json', timeout=2) as r:
            doc = json.load(r)
        if any(t['topic'] == '/chatter' for t in doc.get('topics', [])):
            break
    except Exception:
        pass
    time.sleep(1)
else:
    raise SystemExit('no document with /chatter from /latest.json')
with urllib.request.urlopen(base + '/index.html', timeout=2) as r:
    assert b'<html' in r.read().lower()
PY
echo "PASS: transport_viz_web"
