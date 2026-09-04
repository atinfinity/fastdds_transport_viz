#!/usr/bin/env bash
# Multi-container integration test (run on the Docker host, not inside a container).
#
# Starts demo talker and listener in two separate containers (separate network
# and IPC namespaces => Fast DDS assigns different host ids), runs transport_viz
# from a third container on the same Docker network, and asserts that /chatter
# is reported as UDPv4 with reason "different-host".
set -euo pipefail
cd "$(dirname "$0")/.."

cleanup() { docker compose down --remove-orphans >/dev/null 2>&1 || true; }
trap cleanup EXIT

docker compose build dev >/dev/null
echo "== building workspace"
docker compose run --rm dev bash -c \
  "colcon build --symlink-install > /dev/null && echo build ok"

echo "== starting talker / listener containers"
docker compose up -d talker listener
sleep 3

echo "== running transport_viz"
docker compose run --rm -T dev \
  ros2 run fastdds_transport_viz transport_viz --json --timeout 6 --quiet 0 \
  > /tmp/transport_viz_multi.json
jq '.topics[] | select(.topic=="/chatter") | {transport: .pairs[0].transport, reasons: .pairs[0].reasons, writer_host: .pairs[0].writer_host, reader_host: .pairs[0].reader_host}' \
  /tmp/transport_viz_multi.json

python3 - <<'PY'
import json
doc = json.load(open('/tmp/transport_viz_multi.json'))
chatter = next(t for t in doc['topics'] if t['topic'] == '/chatter')
assert len(chatter['pairs']) == 1, chatter
p = chatter['pairs'][0]
assert p['transport'] == 'UDPv4', p
assert 'different-host' in p['reasons'], p
assert p['writer_host'] != p['reader_host'], p
print('PASS: /chatter across containers uses UDPv4 (different-host)')
PY
