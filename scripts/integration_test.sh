#!/usr/bin/env bash
# Multi-container integration tests (run on the Docker host, not inside a container).
#
#   scripts/integration_test.sh [multi_container|stats_multi_container|stats_loss_multi_container|rate_stats|hostnet_shm|hostnet_noipc_shm|hostnet_split_shm|hostnet_split_shm_visible|hostnet_split_shm_shared_port|hostnet_split_stats|hostnet_split_datasharing|hostnet_split_datasharing_udp|large_data_tcp|udpv6_multi_container|easy_mode_shm|easy_mode_tcp|all]
#
#   multi_container        talker and listener in two bridged containers (separate
#                          network and IPC namespaces => different Fast DDS host ids).
#                          Expect /chatter = UDPv4, reason "different-host".
#   stats_multi_container  same, nodes started with FASTDDS_STATISTICS and transport_viz
#                          run with --stats. Expect measured UDPv4 and two different
#                          PHYSICAL_DATA host names.
#   rate_stats             one container with rate_load pairs of three kinds (SHM between two
#                          processes, Fast DDS intraprocess in one process, shown as SHM
#                          without traffic, and data-sharing) at 10, 100 and
#                          1000 Hz, transport_viz --stats alongside (skipped on Humble). Expect
#                          the HZ column (delivered_per_s) within 3 % of the nominal rate for
#                          every pair and no lower bound (#143).
#   stats_loss_multi_container  same with NET_ADMIN and a 20 Hz talker; tc netem drops 30% of
#                          one node's packets to the other at a time (skipped on Humble).
#                          Expect no lost packets on /chatter while the listener drops, lost
#                          packets and rtps-packets-lost while the talker does (RTPS_LOST,
#                          #122).
#   hostnet_shm            talker and listener in two containers that share the Docker
#                          host's network and IPC namespaces (same host id, same
#                          /dev/shm). Expect /chatter = SHM, reason "same-host-guid".
#   hostnet_noipc_shm      talker and listener in one container on the host network without
#                          the host IPC namespace (the tool's host id, another /dev/shm);
#                          transport_viz in `hostnet`. Expect /chatter = SHM and
#                          shm-not-visible with every SHM port of the nodes missing.
#   hostnet_split_shm      talker and listener in two containers on the host network, an
#                          IPC namespace each (same host id, two /dev/shm); transport_viz in
#                          `hostnet`. Expect /chatter = NONE, shm-ipc-namespace-split and
#                          shm-port-collision (both take the same SHM port number).
#   hostnet_split_shm_visible  the same with a node started before the talker in its
#                          container and transport_viz in the talker's IPC namespace (Jazzy
#                          or newer; skipped on Humble). Expect /chatter = NONE,
#                          shm-reader-port-not-visible and no shm-port-collision.
#   hostnet_split_shm_shared_port  the same view with a node started first in each container
#                          and a second one in the listener's, which takes the talker's 7000+
#                          number (Jazzy or newer; skipped on Humble). Expect /chatter = NONE,
#                          shm-reader-port-not-visible and no shm-port-collision (#118).
#   hostnet_split_stats    hostnet_split_shm with FASTDDS_STATISTICS and transport_viz --stats
#                          (skipped on Humble, which has no statistics module). Expect the
#                          writer's statistics (they no longer go over SHM into its own
#                          /dev/shm), /chatter = NONE, measured SHM traffic, not delivered.
#   hostnet_split_datasharing  bounded_pub and bounded_sub with data-sharing in two containers
#                          on the host network, an IPC namespace each; transport_viz in
#                          `hostnet`. Expect /bounded = NONE, shm-ipc-namespace-split,
#                          datasharing-qos-enabled-both and shm-port-collision.
#   hostnet_split_datasharing_udp  the same with FASTDDS_BUILTIN_TRANSPORTS=UDPv4 and
#                          transport_viz in the publisher's IPC namespace (skipped on Humble).
#                          Expect /bounded = NONE, shm-ipc-namespace-split and
#                          datasharing-reader-segment-not-visible, no SHM reason.
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
  # In `hostnet`, not `dev`: host-network nodes started right after a container left the
  # project bridge announced the bridge address (172.28.0.1) and got another Fast DDS host
  # id (a hash of the interface addresses) than the transport_viz started a few seconds
  # later, so the hostnet scenarios saw two hosts over UDPv4 instead of SHM. The image's
  # colcon defaults build into build/<distro>/, which every container's entrypoint sources.
  docker compose run --rm hostnet bash -c \
    "colcon build --symlink-install > /dev/null && echo build ok"
}

# run_viz <service> <output file> [transport_viz args...]; VIZ_ENV holds extra -e flags
VIZ_ENV=()
run_viz() {
  local service="$1" out="$2"; shift 2
  echo "== running transport_viz ($service) $*"
  docker compose run --rm -T ${VIZ_ENV[@]+"${VIZ_ENV[@]}"} "$service" \
    ros2 run fastdds_transport_viz transport_viz --json --timeout 6 --quiet 0 "$@" > "$out"
  jq '.topics[] | select(.topic=="/chatter" or .topic=="/bounded") | .pairs[] | {transport, measured: .measured.transports, reasons, warnings, writer_host, reader_host}' "$out"
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
topic = '/bounded' if scenario.startswith('hostnet_split_datasharing') else '/chatter'
if scenario.startswith('rate_stats_'):
    chatter, p = None, None   # its own topics, checked below
else:
    chatter = next(t for t in doc['topics'] if t['topic'] == topic)
    assert len(chatter['pairs']) == 1, chatter
    p = chatter['pairs'][0]
if scenario == 'hostnet_noipc_shm' or scenario.startswith('hostnet_split_'):
    # nodes whose ros_discovery_info goes into another /dev/shm than the tool's: their names
    # come from the tool's own UDP reader (#112)
    names = ('/bounded_pub', '/bounded_sub') if topic == '/bounded' else ('/talker', '/listener')
    assert (p['writer_node'], p['reader_node']) == names, p
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
    # the statistics sources are the two nodes: not the tool, not a peer only named in a sample (#113)
    assert set(doc['stats']['participants_with_stats']) == {
        w['participant_guid_prefix'], r['participant_guid_prefix']}, (w, r, doc['stats'])
    print(f"PASS: --stats measured UDPv4 between hosts {p['writer_host']} and {p['reader_host']}")
elif scenario.startswith('rate_stats_'):
    hz = float(scenario.split('_')[-1])
    # the same-process pair has no transport of its own: Fast DDS delivers it inside the
    # participant, the tool shows the SHM locators both announce and no traffic on them
    expect = {'/rate_shm': ('SHM', ['SHM']), '/rate_intra': ('SHM', []),
              '/rate_ds': ('DATA_SHARING', None)}
    for name, (transport, measured) in expect.items():
        t = next(t for t in doc['topics'] if t['topic'] == name)
        assert len(t['pairs']) == 1, t
        q = t['pairs'][0]
        assert q['transport'] == transport, q
        m = q['measured']
        if measured is not None:
            assert m['transports'] == measured, (name, m['transports'])
        assert m['delivered'], q
        assert isinstance(m['delivered_per_s'], (int, float)), m
        assert not m['delivered_per_s_lower_bound'], m
        assert abs(m['delivered_per_s'] - hz) <= 0.03 * hz, (name, m['delivered_per_s'], hz)
        assert m['delivered_per_s_window_s'] == doc['observation_seconds'], m
        print(f"PASS: {name} {transport} delivered {m['delivered_per_s']:.1f}/s at {hz:g} Hz "
              f"over {m['delivered_per_s_window_s']:.1f} s")
elif scenario in ('stats_loss_listener_drops', 'stats_loss_talker_drops'):
    # RTPS_LOST is published by the receiving participant: only the talker's drops are the
    # pair's loss, the listener's (its ACKNACKs) are the talker's to report (#122)
    assert doc['stats']['enabled'] and doc['stats']['samples'] > 0, doc['stats']
    assert p['transport'] == 'UDPv4', p
    assert (p['writer_node'], p['reader_node']) == ('/talker', '/listener'), p
    rel = p['measured']['reliability']
    assert rel is not None and rel['lost_packets'] is not None, p   # the listener publishes RTPS_LOST
    w, r = chatter['writers'][0], chatter['readers'][0]
    reports = [l for l in doc['stats']['lost']
               if l['reporter_participant_guid_prefix'] == r['participant_guid_prefix']
               and l['src_participant_guid_prefix'] == w['participant_guid_prefix']]
    if scenario == 'stats_loss_listener_drops':
        assert rel['lost_packets'] == 0, (p, reports)
        assert 'rtps-packets-lost' not in p['warnings'], p
        assert chatter['lost_packets'] == 0, chatter
        print('PASS: the listener dropping its packets to the talker is no loss of /chatter')
    else:
        assert rel['lost_packets'] > 0, (p, reports)
        assert 'rtps-packets-lost' in p['warnings'], p
        assert chatter['lost_packets'] == rel['lost_packets'], chatter
        print(f"PASS: the talker dropping its packets to the listener: /chatter lost {rel['lost_packets']} packets")
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
    assert 'shm-ipc-namespace-split' not in p['warnings'], p   # one /dev/shm (#101)
    assert not {'datasharing-reader-segment-not-visible',
                'datasharing-writer-segment-not-visible'} & set(p['reasons']), p   # (#110)
    print('PASS: /chatter across host-network/IPC containers uses SHM (same-host-guid); their segments are visible')
elif scenario == 'hostnet_noipc_shm':
    assert p['transport'] == 'SHM', p
    assert 'same-host-guid' in p['reasons'], p
    shm = doc['shm']
    # same host id, another /dev/shm: none of the nodes' port locks is held here, and
    # the tool's own ports (7000, 7001 on Jazzy+) are not taken for theirs (#51)
    assert shm['available'] and not shm['nodes_visible'], shm
    assert shm['checked_ports'] and shm['missing_ports'] == shm['checked_ports'], shm
    assert shm['other_host_participants'] == 0, shm
    assert 'shm-not-visible' in shm['warnings'], shm
    # talker and listener share that other IPC namespace: no split between them (#101)
    assert 'shm-ipc-namespace-split' not in p['warnings'], p
    print('PASS: host-network nodes in their own IPC namespace: /chatter SHM (same-host-guid), '
          'every SHM port of theirs reported missing')
elif scenario == 'hostnet_split_shm':
    # same host id, an IPC namespace each: Fast DDS selects SHM and loses every sample;
    # both take the same SHM port number (7000 on Jazzy+, the pid-based one on Humble)
    assert p['transport'] == 'NONE' and p['confidence'] == 'certain', p
    assert {'same-host-guid', 'both-shm-locators', 'shm-port-collision'} <= set(p['reasons']), p
    assert p['warnings'] == ['shm-ipc-namespace-split'], p
    assert 'shm-not-visible' in doc['shm']['warnings'], doc['shm']
    print('PASS: host-network talker and listener in separate IPC namespaces: /chatter NONE '
          '(shm-ipc-namespace-split, shm-port-collision)')
elif scenario == 'hostnet_split_shm_visible':
    # from the talker's IPC namespace: its ports are held, the listener's are not
    assert p['transport'] == 'NONE' and p['confidence'] == 'certain', p
    assert {'same-host-guid', 'shm-reader-port-not-visible'} <= set(p['reasons']), p
    assert 'shm-port-collision' not in p['reasons'], p
    assert p['warnings'] == ['shm-ipc-namespace-split'], p
    print('PASS: split seen from the talker\'s IPC namespace: /chatter NONE '
          '(shm-ipc-namespace-split, shm-reader-port-not-visible)')
elif scenario == 'hostnet_split_shm_shared_port':
    # the talker's 7001 is announced by a node in the listener's namespace too; its own port
    # still shows that it listens in the tool's namespace (#118)
    assert p['transport'] == 'NONE' and p['confidence'] == 'certain', p
    assert {'same-host-guid', 'shm-reader-port-not-visible'} <= set(p['reasons']), p
    assert 'shm-port-collision' not in p['reasons'], p
    assert p['warnings'] == ['shm-ipc-namespace-split'], p
    shm = doc['shm']
    assert {7000, 7001} <= set(shm['checked_ports']), shm
    assert 7001 not in shm['missing_ports'], shm   # held here by the talker
    # what the report shows of that (#125): the writer's participant with 7001 held but
    # announced twice and its own port as the proof, the reader's with a port absent here
    by_prefix = {q['guid_prefix']: q for q in doc['participants']}
    by_guid = {e['guid']: e for e in chatter['writers'] + chatter['readers']}
    writer = by_prefix[by_guid[p['writer_guid']]['participant_guid_prefix']]
    reader = by_prefix[by_guid[p['reader_guid']]['participant_guid_prefix']]
    assert writer['shm_visibility'] == 'visible' and not writer['own'], writer
    ports = {sp['port']: sp for sp in writer['shm_ports']}
    assert ports[7001]['lock'] == 'held' and ports[7001]['announced_by'] == 2, writer
    assert not ports[7001]['proof'], writer
    assert any(sp['lock'] == 'held' and sp['proof'] and sp['announced_by'] == 1
               for sp in writer['shm_ports']), writer
    assert reader['shm_visibility'] == 'not-visible', reader
    assert any(sp['lock'] == 'absent' for sp in reader['shm_ports']), reader
    assert any(q['own'] for q in doc['participants']), 'the tool\'s own participants'
    print('PASS: split seen from the talker\'s IPC namespace with its 7000+ number announced '
          'twice: /chatter NONE (shm-ipc-namespace-split, shm-reader-port-not-visible), '
          'participants show 7001 held by 2 and the listener\'s port absent')
elif scenario == 'hostnet_split_stats':
    # the tool (a third IPC namespace) gets the nodes' statistics over UDPv4: its statistics
    # readers announce no SHM locator (#106)
    stats = doc['stats']
    assert stats['enabled'] and stats['samples'] > 0, stats
    w, r = chatter['writers'][0], chatter['readers'][0]
    # exactly the two nodes: not the tool, whose multicast discovery the nodes report in
    # RTPS_LOST, nor a participant only named in a sample (#113)
    assert set(stats['participants_with_stats']) == {
        w['participant_guid_prefix'], r['participant_guid_prefix']}, (w, r, stats)
    assert p['transport'] == 'NONE' and p['confidence'] == 'certain', p
    assert p['warnings'] == ['shm-ipc-namespace-split'], p   # no stats-not-enabled-on-writer
    assert 'measured-shm-traffic' in p['reasons'], p
    m = p['measured']
    assert m['available'] and m['transports'] == ['SHM'] and not m['delivered'], m
    print('PASS: split pair with --stats: the writer\'s statistics arrive, /chatter NONE '
          '(measured SHM traffic, not delivered)')
elif scenario == 'hostnet_split_datasharing':
    # data-sharing pairs on QoS alone: the reader cannot open the writer's history in the
    # other /dev/shm and nothing arrives; both take the same SHM port number (#110)
    assert p['transport'] == 'NONE' and p['confidence'] == 'certain', p
    assert {'same-host-guid', 'datasharing-qos-enabled-both', 'both-shm-locators',
            'shm-port-collision'} <= set(p['reasons']), p
    assert 'datasharing-unverified-by-traffic' not in p['reasons'], p
    assert p['warnings'] == ['shm-ipc-namespace-split'], p
    print('PASS: data-sharing publisher and subscriber in separate IPC namespaces: /bounded NONE '
          '(shm-ipc-namespace-split, datasharing-qos-enabled-both, shm-port-collision)')
elif scenario == 'hostnet_split_datasharing_udp':
    # no SHM locator: from the writer's IPC namespace its history is here and the reader's
    # notification segment is not
    assert p['transport'] == 'NONE' and p['confidence'] == 'certain', p
    assert {'same-host-guid', 'datasharing-qos-enabled-both',
            'datasharing-reader-segment-not-visible'} <= set(p['reasons']), p
    assert not {'both-shm-locators', 'shm-port-collision', 'shm-reader-port-not-visible',
                'shm-writer-port-not-visible'} & set(p['reasons']), p
    assert p['warnings'] == ['shm-ipc-namespace-split'], p
    assert chatter['writers'][0]['datasharing_history_bytes'] > 0, chatter['writers'][0]
    print('PASS: UDPv4-only data-sharing split seen from the writer\'s IPC namespace: /bounded '
          'NONE (shm-ipc-namespace-split, datasharing-reader-segment-not-visible)')
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

scenario_stats_loss_multi_container() {
  if [ "${ROS_DISTRO:-jazzy}" = humble ]; then
    echo "SKIP: stats_loss_multi_container needs the Fast DDS statistics module (Jazzy or newer)"
    return 0
  fi
  echo "== starting talker_stats_loss / listener_stats_loss containers"
  docker compose up -d talker_stats_loss listener_stats_loss
  sleep 3
  local phase peer out attempt ok
  for phase in listener talker; do
    if [ "$phase" = listener ]; then peer=talker; else peer=listener; fi
    out="$out_dir/transport_viz_stats_loss_multi_container_$phase.json"
    echo "== dropping 30% of the ${phase}'s packets to the ${peer} (tc netem)"
    # only the packets to the other node: dropping the whole egress also loses the discovery
    # and statistics the tool needs, which a 6 s observation does not always recover from
    docker compose exec -T "${phase}_stats_loss" bash -c "
      set -e
      peer=\$(getent ahostsv4 ${peer}_stats_loss | awk 'NR==1{print \$1}')
      tc qdisc add dev eth0 root handle 1: prio
      tc qdisc add dev eth0 parent 1:3 handle 30: netem loss 30%
      tc filter add dev eth0 parent 1: protocol ip u32 match ip dst \$peer/32 flowid 1:3"
    sleep 2
    ok=
    for attempt in 1 2 3; do   # counters need a moment to accumulate
      run_viz dev "$out" --stats
      if assert "stats_loss_${phase}_drops" "$out"; then ok=1; break; fi
      echo "-- attempt $attempt: statistics incomplete, retrying"
    done
    docker compose exec -T "${phase}_stats_loss" tc qdisc del dev eth0 root
    [ -n "$ok" ] || return 1
  done
}

scenario_rate_stats() {
  if [ "${ROS_DISTRO:-jazzy}" = humble ]; then
    echo "SKIP: rate_stats needs the Fast DDS statistics module (Jazzy or newer)"
    return 0
  fi
  local hz out
  for hz in 10 100 1000; do
    out="$out_dir/transport_viz_rate_stats_$hz.json"
    echo "== rate_load pairs at $hz Hz (SHM, intraprocess, data-sharing) with transport_viz --stats"
    # everything in one container: same host id and /dev/shm; the profile enables
    # statistics and data-sharing AUTO (only the bounded topic qualifies)
    docker compose run --rm -T rate_load bash -c "
      set -e
      ros2 run fastdds_transport_viz rate_load pub --topic /rate_shm --hz $hz > /dev/null 2>&1 &
      ros2 run fastdds_transport_viz rate_load sub --topic /rate_shm > /dev/null 2>&1 &
      ros2 run fastdds_transport_viz rate_load both --topic /rate_intra --hz $hz > /dev/null 2>&1 &
      ros2 run fastdds_transport_viz rate_load pub --topic /rate_ds --hz $hz --bounded > /dev/null 2>&1 &
      ros2 run fastdds_transport_viz rate_load sub --topic /rate_ds --bounded > /dev/null 2>&1 &
      sleep 3
      ros2 run fastdds_transport_viz transport_viz --json --timeout 10 --quiet 0 --stats
      kill %1 %2 %3 %4 %5 2>/dev/null; wait" > "$out"
    jq '.topics[] | select(.topic|startswith("/rate_")) | .topic as $topic | .pairs[] | {topic: $topic, transport, measured: .measured.transports, delivered_per_s: .measured.delivered_per_s, lower_bound: .measured.delivered_per_s_lower_bound, window_s: .measured.delivered_per_s_window_s, reasons}' "$out"
    assert "rate_stats_$hz" "$out"
  done
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

scenario_hostnet_noipc_shm() {
  local out="$out_dir/transport_viz_hostnet_noipc_shm.json"
  echo "== starting talker + listener on the host network without the host IPC namespace"
  docker compose up -d pair_hostnet_noipc
  sleep 3
  run_viz hostnet "$out"
  assert hostnet_noipc_shm "$out"
}

scenario_hostnet_split_shm() {
  local out="$out_dir/transport_viz_hostnet_split_shm.json"
  echo "== starting talker and listener on the host network, an IPC namespace each"
  docker compose up -d talker_hostnet_split listener_hostnet_split
  sleep 3
  local attempt
  for attempt in 1 2 3; do   # a node may not announce its SHM locator yet
    run_viz hostnet "$out"
    if assert hostnet_split_shm "$out"; then return 0; fi
    echo "-- attempt $attempt: split not seen yet, retrying"
  done
  return 1
}

# Humble announces only the pid-based SHM port, numbered per IPC namespace: the node started
# first in the talker's namespace would take the listener's number, and a number announced
# by two participants does not say whose lock the tool sees.
scenario_hostnet_split_shm_visible() {
  if [ "${ROS_DISTRO:-jazzy}" = humble ]; then
    echo "SKIP: hostnet_split_shm_visible needs the 7000+ SHM port of Jazzy or newer"
    return 0
  fi
  local out="$out_dir/transport_viz_hostnet_split_shm_visible.json"
  echo "== starting a node and the talker, and the listener, on the host network, an IPC namespace each"
  docker compose up -d talker_hostnet_split_visible listener_hostnet_split
  sleep 6
  local attempt
  for attempt in 1 2 3; do   # a node may not announce its SHM locator yet
    run_viz hostnet_in_talker_ipc "$out"
    if assert hostnet_split_shm_visible "$out"; then return 0; fi
    echo "-- attempt $attempt: split not seen yet, retrying"
  done
  return 1
}

# #118: a second node in the listener's namespace takes the talker's 7000+ number. Humble has no
# such port, and there each participant's only SHM port is numbered per IPC namespace, so the
# talker would have no port of its own to show where it listens.
scenario_hostnet_split_shm_shared_port() {
  if [ "${ROS_DISTRO:-jazzy}" = humble ]; then
    echo "SKIP: hostnet_split_shm_shared_port needs the 7000+ SHM port of Jazzy or newer"
    return 0
  fi
  local out="$out_dir/transport_viz_hostnet_split_shm_shared_port.json"
  echo "== starting a node and the talker, and two nodes and the listener, on the host network, an IPC namespace each"
  docker compose up -d talker_hostnet_split_shared listener_hostnet_split_shared
  sleep 6
  local attempt
  for attempt in 1 2 3; do   # a node may not announce its SHM locator yet
    run_viz hostnet_in_talker_shared_ipc "$out"
    if assert hostnet_split_shm_shared_port "$out"; then return 0; fi
    echo "-- attempt $attempt: split not seen yet, retrying"
  done
  return 1
}

scenario_hostnet_split_stats() {
  if [ "${ROS_DISTRO:-jazzy}" = humble ]; then
    echo "SKIP: hostnet_split_stats needs the Fast DDS statistics module (Jazzy or newer)"
    return 0
  fi
  local out="$out_dir/transport_viz_hostnet_split_stats.json"
  echo "== starting talker_hostnet_split_stats / listener_hostnet_split_stats containers"
  docker compose up -d talker_hostnet_split_stats listener_hostnet_split_stats
  sleep 3
  local attempt
  for attempt in 1 2 3; do   # counters need a moment to accumulate
    run_viz hostnet "$out" --stats
    if assert hostnet_split_stats "$out"; then return 0; fi
    echo "-- attempt $attempt: statistics or split not seen yet, retrying"
  done
  return 1
}

scenario_hostnet_split_datasharing() {
  local out="$out_dir/transport_viz_hostnet_split_datasharing.json"
  echo "== starting bounded_pub and bounded_sub with data-sharing on the host network, an IPC namespace each"
  docker compose up -d bounded_pub_hostnet_split bounded_sub_hostnet_split
  sleep 3
  local attempt
  for attempt in 1 2 3; do   # a node may not announce its SHM locator yet
    run_viz hostnet "$out"
    if assert hostnet_split_datasharing "$out"; then return 0; fi
    echo "-- attempt $attempt: split not seen yet, retrying"
  done
  return 1
}

scenario_hostnet_split_datasharing_udp() {
  if [ "${ROS_DISTRO:-jazzy}" = humble ]; then
    echo "SKIP: hostnet_split_datasharing_udp needs FASTDDS_BUILTIN_TRANSPORTS (Fast DDS 2.11+, Jazzy or newer)"
    return 0
  fi
  local out="$out_dir/transport_viz_hostnet_split_datasharing_udp.json"
  echo "== starting bounded_pub and bounded_sub with data-sharing over UDPv4 only, an IPC namespace each"
  docker compose up -d bounded_pub_hostnet_split_udp bounded_sub_hostnet_split_udp
  sleep 3
  local attempt
  for attempt in 1 2 3; do   # the endpoints may not be discovered yet
    run_viz hostnet_in_bounded_pub_ipc "$out"
    if assert hostnet_split_datasharing_udp "$out"; then return 0; fi
    echo "-- attempt $attempt: split not seen yet, retrying"
  done
  return 1
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
  multi_container|stats_multi_container|stats_loss_multi_container|rate_stats|hostnet_shm|hostnet_noipc_shm|hostnet_split_shm|hostnet_split_shm_visible|hostnet_split_shm_shared_port|hostnet_split_stats|hostnet_split_datasharing|hostnet_split_datasharing_udp|large_data_tcp|udpv6_multi_container|easy_mode_shm|easy_mode_tcp)
    "scenario_$scenario" ;;
  all)
    for s in multi_container stats_multi_container stats_loss_multi_container rate_stats hostnet_shm hostnet_noipc_shm hostnet_split_shm hostnet_split_shm_visible hostnet_split_shm_shared_port hostnet_split_stats hostnet_split_datasharing hostnet_split_datasharing_udp large_data_tcp udpv6_multi_container easy_mode_shm easy_mode_tcp; do
      echo; echo "#### $s"
      "scenario_$s"
      cleanup
    done ;;
  *)
    echo "usage: $0 [multi_container|stats_multi_container|stats_loss_multi_container|rate_stats|hostnet_shm|hostnet_noipc_shm|hostnet_split_shm|hostnet_split_shm_visible|hostnet_split_shm_shared_port|hostnet_split_stats|hostnet_split_datasharing|hostnet_split_datasharing_udp|large_data_tcp|udpv6_multi_container|easy_mode_shm|easy_mode_tcp|all]" >&2; exit 2 ;;
esac
