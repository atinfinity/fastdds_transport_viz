# Development, verification and tests

## Docker environment

The repository ships a `compose.yaml` and `docker/Dockerfile` based on `ros:jazzy`
(multi-arch: x86_64 and arm64). `ROS_DISTRO=lyrical docker compose build` (or `rolling`)
builds the same environment on Fast DDS 3.x, `ROS_DISTRO=humble` on Fast DDS 2.6; the image is tagged
`fastdds_transport_viz:<distro>` and every `docker compose` command below then needs
the same `ROS_DISTRO` in the environment. The image upgrades the base's ROS packages before
installing its own: `ros:<distro>` lags the apt repo, and a freshly published `demo_nodes_cpp`
next to the preinstalled typesupport dies with an undefined symbol
([#162](https://github.com/atinfinity/fastdds_transport_viz/issues/162),
[#171](https://github.com/atinfinity/fastdds_transport_viz/issues/171)).

```
docker compose build
docker compose run --rm dev bash       # shell in the dev container, repo mounted at /ws
colcon build --symlink-install
source build/$ROS_DISTRO/install/setup.bash
```

Each image builds into its own tree in the repository, `build/<distro>/{build,install,log}`:
the image's colcon defaults (`COLCON_DEFAULTS_FILE`) and `COLCON_LOG_PATH` set the bases,
so plain `colcon build`, `colcon test` and `colcon test-result` use them, and the
entrypoint of every container sources `build/<distro>/install/setup.bash` when it exists.
Switching `ROS_DISTRO` never reuses another distribution's CMake caches or setup files,
and `colcon test-result` only sees that distribution's results; `rm -rf build/<distro>`
starts one over. A top-level `install/` is no longer sourced (the entrypoint says so on
stderr). Native builds and CI keep colcon's default `build/` and `install/`
([#123](https://github.com/atinfinity/fastdds_transport_viz/issues/123)).

Every compose service takes `RMW_IMPLEMENTATION` from the host shell (default
`rmw_fastrtps_cpp`), so `RMW_IMPLEMENTATION=rmw_fastrtps_dynamic_cpp docker compose run
--rm dev bash` gives a shell whose `colcon test` runs the whole suite on the dynamic RMW,
and the same prefix on `scripts/integration_test.sh` switches the observed nodes.

Try it in that shell:

```
ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 transport list -v --explain                               # /chatter -> SHM

FASTDDS_BUILTIN_TRANSPORTS=UDPv4 ros2 run demo_nodes_cpp listener &
ros2 transport list -v                                         # second pair -> UDPv4, reader-no-shm-locator
```

## Multi-container scenarios

`scripts/integration_test.sh <scenario>` (run on the Docker host, not inside a container)
builds the workspace, starts demo nodes in separate containers, runs `transport_viz`
in a third container on the same scope and asserts the verdict:

| Scenario | Containers | Expected |
|---|---|---|
| `multi_container` (default) | `talker`, `listener`: separate network and IPC namespaces ⇒ different host ids | `UDPv4`, `different-host`, `shm-not-visible` |
| `stats_multi_container` | `talker_stats`, `listener_stats`: as above with `FASTDDS_STATISTICS` | measured `UDPv4`, two different `PHYSICAL_DATA` host names, `participants_with_stats` exactly the two nodes |
| `stats_loss_multi_container` | `talker_stats_loss` (`ros2 topic pub` at 20 Hz), `listener_stats_loss`: `stats_multi_container` with `NET_ADMIN`, `tc netem` dropping 30 % of one node's packets to the other at a time (the tool's traffic is left alone) (skipped on Humble) | listener dropping: `/chatter` `lost_packets` 0, no `rtps-packets-lost`; talker dropping: `lost_packets` > 0, `rtps-packets-lost` |
| `hostnet_shm` | two `hostnet` containers: `network_mode: host` + `ipc: host` ⇒ same host id, shared `/dev/shm` | `SHM`, `same-host-guid`, the nodes' segments visible in `shm` |
| `hostnet_noipc_shm` | `pair_hostnet_noipc`: talker + listener in one container, `network_mode: host` without `ipc: host` ⇒ the tool's host id, another `/dev/shm`; the tool in `hostnet` | `SHM`, `same-host-guid`, `shm-not-visible` with every SHM port of the nodes missing |
| `hostnet_split_shm` | `talker_hostnet_split`, `listener_hostnet_split`: `network_mode: host`, an IPC namespace each ⇒ same host id, two `/dev/shm`; the tool in `hostnet` | `NONE`, `shm-ipc-namespace-split`, `shm-port-collision` |
| `hostnet_split_shm_visible` | `talker_hostnet_split_visible` (a node started first takes the 7000 port) and `listener_hostnet_split`; the tool in `hostnet_in_talker_ipc`, the talker's IPC namespace (Jazzy or newer, skipped on Humble) | `NONE`, `shm-ipc-namespace-split`, `shm-reader-port-not-visible`, no `shm-port-collision` |
| `hostnet_split_shm_shared_port` | `talker_hostnet_split_shared` (a node started first takes 7000, the talker 7001) and `listener_hostnet_split_shared` (two nodes started first take 7000 and 7001, the talker's number); the tool in `hostnet_in_talker_shared_ipc`, the talker's IPC namespace (Jazzy or newer, skipped on Humble) | `NONE`, `shm-ipc-namespace-split`, `shm-reader-port-not-visible`, no `shm-port-collision`; in `participants` the talker's 7001 is `held` and announced by two participants, so the proof is a port of its own that only it announces, and the listener's participant is `not-visible` ([#118](https://github.com/atinfinity/fastdds_transport_viz/issues/118), [#125](https://github.com/atinfinity/fastdds_transport_viz/issues/125)) |
| `hostnet_split_stats` | `talker_hostnet_split_stats`, `listener_hostnet_split_stats`: `hostnet_split_shm` with `FASTDDS_STATISTICS`; the tool in `hostnet` with `--stats` (skipped on Humble, which has no statistics module) | `participants_with_stats` exactly the two nodes, `NONE`, `shm-ipc-namespace-split`, `measured-shm-traffic`, not delivered, no `stats-not-enabled-on-writer` |
| `hostnet_split_datasharing` | `bounded_pub_hostnet_split`, `bounded_sub_hostnet_split`: `hostnet_split_shm` with `bounded_pub` / `bounded_sub` and data-sharing (`datasharing_auto.xml`); the tool in `hostnet` | `/bounded` `NONE`, `shm-ipc-namespace-split`, `datasharing-qos-enabled-both`, `shm-port-collision` |
| `hostnet_split_datasharing_udp` | `bounded_pub_hostnet_split_udp`, `bounded_sub_hostnet_split_udp`: the same with `FASTDDS_BUILTIN_TRANSPORTS=UDPv4`; the tool in `hostnet_in_bounded_pub_ipc`, the publisher's IPC namespace (skipped on Humble) | `/bounded` `NONE`, `shm-ipc-namespace-split`, `datasharing-reader-segment-not-visible`, no SHM reason |
| `large_data_tcp` | `talker_large_data`, `listener_large_data`: bridged, `FASTDDS_BUILTIN_TRANSPORTS=LARGE_DATA` + statistics | `TCPv4`, `common-tcpv4-locator`, measured `TCPv4` |
| `rate_stats` | `rate_load`: one container with `FASTDDS_STATISTICS` and data-sharing, five `rate_load` processes per rate (10, 100 and 1000 Hz): an SHM pair (`/rate_shm`, two processes), a same-process pair (`/rate_intra`, `both`) and a data-sharing pair (`/rate_ds`, `--bounded`); the tool inside with `--stats --timeout 10` (skipped on Humble) | every pair's `delivered_per_s` within 3 % of the publish rate, or - when the tool reports a lower bound - between 0.9 and 1.03 times it (the load's statistics writer is keep-last 10, ~10 ms of slack at 1000 Hz, so a stall of a shared CI runner loses samples and the tool honestly says `≥`; seen four times on 2026-09-20 at 975.7-983.7/s, [#186](https://github.com/atinfinity/fastdds_transport_viz/issues/186)), `delivered_per_s_window_s` = `observation_seconds`; `/rate_shm` `SHM` measured `SHM`, `/rate_intra` `SHM` with no measured transport and the reason `intra-process`, `/rate_ds` `DATA_SHARING` |
| `intra_process` | `rate_load`: one container with a single `rate_load both --topic /rate_intra` process (writer and reader in one process), the tool inside with `--timeout 30 --quiet 2`, once with `--stats` (skipped on Humble) and once without | the pair carries `intra-process` and no warning on both runs; with `--stats` the run stops on `settled` well before `--timeout` (5.0 s on Jazzy and Lyrical), `stats.measurable_pairs` 0, `stats.pairs_delivered` 0, no `rtps-sent-absent`, nothing on stderr ([#201](https://github.com/atinfinity/fastdds_transport_viz/issues/201)) |
| `udpv6_multi_container` | `talker_udpv6`, `listener_udpv6`: bridged (the project network has IPv6), `DEFAULTv6` | `UDPv6`, `common-udpv6-locator` |
| `easy_mode_shm` | two `hostnet` containers with `ROS2_EASY_MODE=127.0.0.1` (Fast DDS 3.2+: `ROS_DISTRO=lyrical` or `rolling`, skipped otherwise; Kilted has it too but is out of scope since 1.1.0) | `SHM`, `same-host-guid`, no multicast locator (P2P) |
| `easy_mode_tcp` | `talker_easy_mode`, `listener_easy_mode`: bridged with fixed addresses, `ROS2_EASY_MODE` pointing at the talker's, statistics; the tool runs on the talker's host | `TCPv4`, `common-tcpv4-locator`, measured `TCPv4`, no multicast locator |
| `record_flip` | `talker_stats` (`dev` on Humble, without `--stats`): `demo_nodes_cpp` talker and listener and `transport_viz_web --port 0 --record /tmp/rec.jsonl --interval 1 --topic '^/chatter$'` in one container; after 10 s the talker is restarted with `FASTDDS_BUILTIN_TRANSPORTS=UDPv4`, after 10 more with the default, then everything gets SIGTERM | every line of the recording is a document, at least 10 frames, `/chatter` goes `SHM` -> `UDPv4` -> `SHM`, some frame's `changes` is not empty, and with `--stats` something is measured; the Jazzy capture is `web/sample/recording.jsonl` ([#82](https://github.com/atinfinity/fastdds_transport_viz/issues/82)) |
| `all` | every scenario in sequence | |

Output goes to `${TMPDIR:-/tmp}/transport_viz_<scenario>.json`, with these exceptions:
`record_flip` writes the recording to `transport_viz_record_flip.jsonl`, `intra_process`
writes `transport_viz_intra_process.json` and `transport_viz_intra_process_stats.json` (each
with its stderr next to it as `.stderr`), `rate_stats` one file per rate
(`transport_viz_rate_stats_<hz>.json`) and `stats_loss_multi_container` one per dropping node
(`transport_viz_stats_loss_multi_container_{listener,talker}.json`). `hostnet_noipc_shm` and
every `hostnet_split_*` scenario also assert the real `writer_node` / `reader_node`, which
reach the tool only through its own `ros_discovery_info` reader
([#112](https://github.com/atinfinity/fastdds_transport_viz/issues/112)).

## Multicast stamping experiment

`scripts/multicast_stamping_test.sh [rung...]` (run on the Docker host) answers a question
about Fast DDS rather than about this tool
([#130](https://github.com/atinfinity/fastdds_transport_viz/issues/130)): does a sender with
several interfaces inflate a remote receiver's `RTPS_LOST`? It starts a 20 Hz `BEST_EFFORT`
talker and two `ros2 topic echo` readers on `239.255.0.7:7900`, runs `transport_viz --json
--stats` three times per rung from a container of its own, and `scripts/multicast_stamping_report.py`
divides each reporter's `RTPS_LOST` by the sender's `RTPS_SENT` on that locator:

| Rung | Sender | Expected ratio |
|---|---|---|
| `mcast1`, `mcast2`, `mcast3` | one, two and three container networks, the readers on one of them | 1, 2 and 3 - the interface count |
| `control` | `network_mode: host`, the readers with it in one network namespace | 0 - every copy arrives |
| `whitelist` | unicast with an `<interfaceWhiteList>` of all three addresses | 0 - no false loss on unicast |

`RTPS_SENT` is emitted once per logical message and is the honest denominator, so the
verdict needs no timing accuracy; both counters come out of one `--stats` document. The
services live in the `multicast` compose profile, so no other run starts them, and the
experiment is deliberately outside `scripts/integration_test.sh` and CI: it measures a
Fast DDS behaviour that should stop reproducing the day eProsima changes it. It needs a
statistics-capable Fast DDS, so Humble cannot run it. The results are in
[Verification results](verification-log.md#verification-results) and the mechanism in
[Statistics](statistics.md).

## Two physical hosts

Run the `hostnet` service on each machine so the nodes use the real LAN interfaces (the
image has to be built on both, `docker compose build dev`; `colcon build` is only needed
where `transport_viz` runs). A native ROS 2 install works the same way.

```
# host A
docker compose run --rm hostnet ros2 run demo_nodes_cpp talker
# host B
docker compose run --rm hostnet ros2 run demo_nodes_cpp listener
# either host
docker compose run --rm hostnet ros2 run fastdds_transport_viz transport_viz -v --explain
```

Expected: `/chatter` = `UDPv4`, `different-host`, hosts `local` and `host:<id>`. Add
`--stats` with `FASTDDS_STATISTICS` on the nodes to see the two host names and measured
`UDPv4`.

Verified on 2026-09-06 with a Jetson Orin NX (JetPack 6, Ubuntu 22.04, Wi-Fi) as host B
and the x86_64 desktop as host A, both through the `hostnet` service; multicast discovery
worked without any peer configuration and the `--stats` row came out in both directions
(see [Verification results](verification-log.md#verification-results)). Jetson specifics: Docker on JetPack cannot create bridge
networks (`iptables: can't initialize iptables table 'raw'`), so build the image with
`docker build --network=host --build-arg ROS_DISTRO=jazzy -t fastdds_transport_viz:jazzy
-f docker/Dockerfile .` instead of `docker compose build`; `hostnet` itself needs no
bridge. The image build takes about 10 minutes on the Orin NX.

Easy Mode (Fast DDS 3.2+) is the built-in answer to a LAN without multicast: set
`ROS2_EASY_MODE=<address of host A>` on both hosts and the tool, nothing else. Verified on
2026-09-13 with the same two machines (Jetson re-installed with Ubuntu 24.04, Lyrical
image on both, started with `docker run --rm --network host --ipc host -v
$PWD:/ws fastdds_transport_viz:lyrical ros2 run ...` on the Jetson): `TCPv4`, measured
`TCPv4` in both directions, seen from either host (see [Verification results](verification-log.md#verification-results) and
[how-it-works.md](how-it-works.md#run-it-where-the-nodes-run)).

Things learned while trying this on a Wi-Fi LAN with a Mac as host B
([#15](https://github.com/atinfinity/fastdds_transport_viz/issues/15)):

- **Docker Desktop on macOS cannot be one of the hosts.** Its "host network" is the
  Linux VM's: the participant announces `192.168.65.x` / `192.168.64.x` / `172.17.0.1`
  locators that the LAN cannot reach (and that collide with the other machine's own
  Docker bridges). Use a native install or a Linux host.
- If multicast does not cross the network (common on Wi-Fi access points), set
  `ROS_STATIC_PEERS=<other host IP>` on both sides. That alone is not always enough: with
  the default locators Fast DDS sends discovery data to the *multicast* metatraffic
  locator as soon as two peers share it, so a node with any other local peer stops
  announcing to the static peer by unicast. `config/unicast_discovery.xml` removes the
  multicast metatraffic locator; use it via `FASTRTPS_DEFAULT_PROFILES_FILE` on every
  participant (nodes and `transport_viz`) and list this host's own IP in
  `ROS_STATIC_PEERS` as well so that the local nodes still find `transport_viz`.
- `ROS_AUTOMATIC_DISCOVERY_RANGE=OFF` disables discovery completely (rmw_fastrtps caps
  the participant count at one); `LOCALHOST` makes nodes announce only `127.0.0.1`.
  Neither helps across hosts.
- On the x86_64 ↔ macOS (native RoboStack) pair, **Discovery Server** on the Linux host
  is the configuration that worked: the tool reported `UDPv4` / `different-host` for the
  cross-host pair. It is unreliable on that Mac, though: its Fast DDS `sendto()` calls
  intermittently return `EHOSTUNREACH` for minutes at a time while plain UDP sockets on
  the same machine deliver everything (found with a `DYLD_INSERT_LIBRARIES` interposer;
  `nettop` counts attempted bytes, not delivered ones). Datagram size, interface
  whitelist, netmask filter, SHM, Docker Desktop's bridge and macOS privacy settings were
  ruled out. Details and next steps in
  [#15](https://github.com/atinfinity/fastdds_transport_viz/issues/15).

## Verification nodes

Shipped with the package for reproducing the scenarios in the docs:

| Executable | Purpose |
|---|---|
| `bounded_pub` / `bounded_sub` | `std_msgs/Int32`, data-sharing eligible |
| `unbounded_pub` / `unbounded_sub` [`--best-effort`] [`--transient-local`] | `std_msgs/String`, never data-sharing; QoS options for the request/offer tests |
| `large_array_pub --size-kb N [--period-ms M]` / `large_array_sub` | large `std_msgs/UInt8MultiArray` samples (default 200 ms period) |
| `scale_load --index I --processes P [--nodes N] [--topics T] [--readers R] [--seed S] [--rate HZ]` | one process of a synthetic system for the scale verification (below): every topic has one writer and R readers in other processes chosen from the seed, `std_msgs/String` at 10 Hz, every tenth topic `std_msgs/Int32` |
| `rate_load pub\|sub\|both --topic NAME [--hz HZ] [--bounded]` | one publisher and/or subscription on one topic at a fixed rate for the delivered-rate verification (`rate_stats`): `std_msgs/String`, or `std_msgs/Int32` with `--bounded` (data-sharing eligible); `both` keeps rclcpp intra-process communication off so the same-process pair still goes through Fast DDS; prints the published and received counts on exit |

## Tests

```
colcon test && colcon test-result --verbose
```

- `test_decision`: gtest over the pure decision logic, the statistics overlay and name
  demangling.
- `test_render`: gtest over the table renderer (visible width, truncation, colors, watch
  marks and ghost rows, every `measured=` cell value, the statistics and shared-memory
  footers, the `--explain` legend, host labels).
- `test_render_json`: gtest over the JSON renderer (every documented key, the `stats` and
  `shm` objects, the `--watch` and `diff` `changes` objects, JSON Lines mode) and its
  inverse `parse_json()` (a rendered document parses back into a snapshot that renders
  identically; JSON Lines; foreign documents are rejected).
- `test_render_csv`: gtest over the `--csv` renderer
  ([#83](https://github.com/atinfinity/fastdds_transport_viz/issues/83)): the header, one row per pair with every cell the value of
  the JSON document's field (empty where that field is null), RFC 4180 quoting of commas,
  quotes and line breaks, the header alone without pairs and rows alone after the first
  `--watch` frame.
- `test_rmw_check`: gtest over the RMW check at startup
  ([#72](https://github.com/atinfinity/fastdds_transport_viz/issues/72), [#73](https://github.com/atinfinity/fastdds_transport_viz/issues/73)): `rmw_fastrtps_cpp` and
  `rmw_fastrtps_dynamic_cpp` are accepted silently, another middleware is rejected naming it
  and the fix, an RMW that does not load is rejected with the rmw error.
- `test_decision` also covers `diff_snapshots()`: the GUID key sees a restart as removed +
  added, the node key does not, several endpoints of one node, the GUID fallback.
- `test_cli_args` (pytest): `--help`, `--list-codes`, unknown options, missing values,
  invalid regexes and color modes exit with the documented codes and messages; `--color
  always` paints a non-terminal table; the `ROS_AUTOMATIC_DISCOVERY_RANGE=OFF` warning;
  `transport_viz diff` on the fixture pair `web/sample/diff_before.json` /
  `diff_after.json` (node vs GUID key, `--changes-only`, the `--json` shape, filters, stdin
  and JSON Lines input, colors, every exit status).
- `test_stats_profiles` (pytest): the installed statistics profiles CMake generated from the
  templates (#152, #154, #159): a profile for every statistics alias, the writer QoS the
  statistics module expects, `heartbeat_period` only on Fast DDS 3.x, and the large-SHM
  transport of the test fixture.
- `test_scale_measure` / `test_multicast_stamping_report` (pytest, the scripts loaded from
  the source tree, skipped without it): the budget judgement of `scripts/scale_measure.py
  --judge` (#167: named failures with value and limit, the frame median and p95 judged
  separately, the host share, budgets without a value, a dead load) and the arithmetic of
  `scripts/multicast_stamping_report.py` (#130: the counters' gain in the window, rows per
  reporter and locator, the expected ratio of each rung, one bad run failing).
- `test_launch_common` (pytest): the launch tests' `run_tool` helper reports how a failing
  tool run ended - the signal, also when `ros2 run` passes it on as an exit code - and what it
  printed (#215).
- `test_shm_info`: gtest over the `/dev/shm` scan on a temporary directory (sizes, stale
  detection through `flock`, data-sharing file names, IPC-namespace visibility, the
  capacity warning).
- `test_observers`: gtest with real Fast DDS participants on domain 200 (no ROS nodes):
  locator and QoS conversions, participant creation failure (a death test with an invalid
  lease configuration), the statistics observer reusing an existing topic and rejecting a
  content-filtered topic under a statistics topic name.
- `test_same_host_shm.py` / `test_same_host_udp.py`: launch_testing against real demo
  nodes (SHM, and UDPv4 fallback via `FASTDDS_BUILTIN_TRANSPORTS=UDPv4`).
- `test_stats.py`: demo nodes with `FASTDDS_STATISTICS`; asserts measured SHM / UDPv4,
  host names, the `LATENCY` values and the reliability counters (no loss on one host).
- Humble: tests that need the statistics module, `FASTDDS_BUILTIN_TRANSPORTS` or
  `ROS_AUTOMATIC_DISCOVERY_RANGE` skip themselves (`skip_without_*` in `_common.py`);
  UDPv4-only participants come from `udpv4_only.xml` instead of the environment variable.
- `test_udpv6.py` (`FASTDDS_BUILTIN_TRANSPORTS=UDPv6`, skipped without an IPv6
  interface), `test_localhost_range.py` (`ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST`),
  `test_discovery_server.py` (`fastdds discovery` server, SUPER_CLIENT vs plain client, the
  server in `participants` with the nodes attributed to it, #86), `test_easy_mode.py`
  (`DiscoveryServerAuto` reported and attributed by host, #86),
  `test_large_data_same_host.py` (`LARGE_DATA`: TCPv4 announced, SHM chosen),
  `test_large_shm.py` (2 MB samples measured on SHM), `test_datasharing_stats.py`
  (`bounded_pub`/`bounded_sub` with `datasharing_auto_stats.xml`: `DATA_SHARING` becomes
  `certain` through `HISTORY_LATENCY` + `DATA_COUNT`), `test_unbounded_vs_bounded.py`
  (`datasharing_auto.xml`: the bounded pair is `DATA_SHARING?`, the `unbounded_pub` String
  writer resolves to `OFF` and stays on `SHM`), `test_shm.py` (the shared-memory report for
  nodes in the tool's IPC namespace), `test_watch.py` (`--watch` without a terminal: a pair
  appears, disappears and comes back while watching, `+`/`-` marks, ghost rows, the
  `changes` object in `--watch --json`), `test_watch_tty.py` (`--watch` on a pseudo
  terminal: alternate screen, in-place painting, the `q`/`p`/`v`/`e`/`a` keys, width
  truncation, Ctrl-C), `test_large_data_v6.py` (`LARGE_DATAv6`: TCPv6 announced, SHM
  chosen; skipped without an IPv6 interface), `test_multicast_locators.py` (endpoints
  announcing a multicast locator through `defaultMulticastLocatorList`),
  `test_qos_incompatible.py` (`unbounded_pub --best-effort` and `unbounded_sub
  --transient-local` next to the default ones: the incompatible pairs are `NONE` with
  `qos-incompatible-*`), `test_diff.py` (`transport_viz diff` on two live captures: a
  restarted listener is no change under the node key and a removed + added pair under the
  GUID key; a second listener is an added pair under both), `test_type_mismatch.py`
  (`ros2 topic pub` of `Int32` next to `ros2 topic echo` of `String` on one topic: a `NONE`
  pair with `type-name-mismatch`, #85), `test_shm_own_ports.py` (the tool alone on a private
  domain checks no SHM port: its own participants are not nodes, #51),
  `test_discovery_completeness.py` (the `discovery` object and the stderr warning agree on
  whether discovery had settled, and the field survives a saved document, #133),
  `test_service_action.py` (`add_two_ints_server` with a client and a `Fibonacci` action server
  and client: one `SERVICE` row with 1/1 members and one `ACTION` row with 3/5 under `--all`, an
  uncalled parameter service still a service, a plain topic keeping its kind, #84).
- `test_json_schema` / `test_json_schema_live.py`: sample and live `--json` output against
  `schema/transport_viz.schema.json`. Every key `render_json.cpp` writes has to be declared
  there: `stats.measured_instances` was rendered and parsed back for a whole issue's worth of
  work while the schema never named it
  ([#199](https://github.com/atinfinity/fastdds_transport_viz/issues/199)), and nothing
  failed, because `$defs.stats` takes additional properties and validation cannot tell a field
  the schema documents from one it forgot. The schema stays permissive on purpose - a document
  written by a newer tool must not be rejected by an older schema - so the completeness of the
  declarations is a test (`test_sample_carries_no_key_the_schema_leaves_undeclared`, which
  walks each shipped document against the schema and reports keys no property declares)
  rather than `additionalProperties: false`. A new field in the JSON output therefore comes
  with its declaration in the same pull request.
- `test_web_serve` (pytest, fake `transport_viz`): the live server's SSE stream (documents,
  the `id:` of each and the `retry: 1000` of #191, the closing status event), `/latest.json`,
  `--record` (every line written as it came, an unwritable path failing before the start),
  `/metrics` before and after the first document, without statistics and after the end
  (#83), and SIGTERM / SIGHUP reaping the `transport_viz` child (#195).
  `test_web_live.py` (real one): `/latest.json` and the first SSE document.
- `ros2transport/test/test_cli.py` (pytest, fake `transport_viz`): argument translation of
  `ros2 transport list` and `diff`, parity with the binary's `--help`, `codes`, missing
  binary and missing input files.
  `test_list_live.py`: `ros2 transport list --json` against real demo nodes.

Line coverage of the C++ sources is measured with `scripts/coverage.sh` (a `--coverage`
build in `build/<distro>/coverage/`, the package's test suite, then `gcovr`; extra `gcovr`
arguments pass through), by hand inside the dev container and in CI for every pull request
and merge commit ([#80](https://github.com/atinfinity/fastdds_transport_viz/issues/80)): the
current number is the Coverage badge of the README (Coveralls, `main`), the per-file report
is the `coverage-html-jazzy-x86_64` artifact of the run. It has stayed between 91 and 98 %
(91.1 % lines in the first CI run on 2026-09-20 with 63 tests skipped there; `shm_info.cpp` 100 %, `main.cpp`, `render_table.cpp`, `render_json.cpp` 99 %,
`stats_observer.cpp` 98 %, `decision.cpp` and `discovery_observer.cpp` 96 % as of
2026-09-06). What is left is unreachable by construction: subscriber
and reader creation failures inside Fast DDS, `getifaddrs` errors, `default:` labels of
switches over enums whose every value is handled, and discovery statuses Fast DDS 2.14 never
reports for our participant.

The web viewer's pure functions (`web/model.js`: document → nodes/hosts/pairs, filters
with the `--node` semantics, edge bundling, number formatting, the shared-memory line, and
the comparison of two documents: `diffDocuments()` must reproduce `web/sample/diff.json`,
what the binary printed for `diff --all --json` on the fixture pair, which `test_cli_args`
asserts from the C++ side; `web/scene.js`: the scene of the current filters, the column
layout, the edge curves and the label midpoints; `web/replay.js`, tested by `replay.test.js`:
a recording split into frames with their byte offsets, the per-pair series of the strip
charts across restarts and gaps, the next change, the SSE id tracking of live mode and the
dropping of the oldest frames under `?history=`, and the replay of
`web/sample/recording.jsonl`) are unit-tested under Node without a browser:

```
node --test "web/test/*.test.js"
```

`app.js` itself is an IIFE that only exists inside a page, so it is tested in a real
browser instead: `web/test/browser.test.js` and `web/test/live.test.js` drive headless
Chrome over the DevTools protocol (`web/test/cdp.js`, no dependencies and no build step -
the same protocol `scripts/scale_viewer.js` uses) against the viewer served from `web/`,
one tab per test. Together they cover the first render of `web/sample/sample.json`
(arrows, nodes, host boxes, labels, `#meta`) and of the embedded `sample.js`, the topic and
node filters including an invalid regular expression, the transport checkboxes and *Show
internal topics*, the edge and node panels, the table's topic rows, collapse and sorting and
a pair row's card, a `?diff=` load (summary, `.added` / `.changed` marks, ghost rows,
`?key=guid`, *changes only*) and a document carrying its own `changes`, drag and drop, a
failed `?src=`, replay of `web/sample/recording.jsonl` (the timeline with a tick per change,
next / previous, the arrow keys, *change* and `?frame=`, a selected pair keeping its card and
charts, a click on a chart, *Compare with…*, a dropped recording with foreign lines), and
live mode through `web/serve.py` with a fake producer (`web/test/fake_transport_viz.js`,
frames released by a step file): the first frame, a new frame keeping the selection, the
history timeline following the newest frame, ◀ and *Pause* stopping on a past frame while
frames keep coming and live ▶| / End going back, *Save recording*, the end-of-stream banner
with the history kept, `?history=` dropping the oldest frames, and - through a TCP proxy the
test cuts under the browser - the reconnect banner and the recovery after it. The expectations come from `web/model.js` and `web/scene.js`
rather than from hard-coded numbers, so regenerating the samples does not break them.

Everything runs from the same command. The browser tests need Node >= 22 (global
`WebSocket`) and a Chrome or Chromium on the `PATH` - `$CHROME` names another one, and they
skip with a reason when there is none. `FTV_REQUIRE_BROWSER=1` turns that skip into a
failure, and `FTV_CHROME_NO_SANDBOX=1` adds `--no-sandbox` (needed where unprivileged user
namespaces are forbidden, as on the Ubuntu runner image); running as root, as in the dev
container, adds it by itself. CI
runs the whole file set with both set, in the `web viewer tests (node)` job.
`test_web_serve` / `test_web_live.py` stay the tests of the server itself.

## Continuous integration

`.github/workflows/ci.yml` runs `rosdep install`, `colcon build`, `colcon test` inside
`ros:humble`, `ros:jazzy`, `ros:lyrical` and `ros:rolling` containers for every pull request that touches
code (docs-only changes skip the job); Rolling may break with upstream changes and does
not block (`continue-on-error`). Jazzy and Lyrical are built and tested twice, on the
x86_64 runner and on GitHub's `ubuntu-24.04-arm` runner (the `ros:<distro>` images are
multi-arch); the arm64 jobs block pull requests like the x86_64 ones. Humble on arm64 is
not run (Fast DDS 2.6, prediction only), and Rolling on arm64 is left to the weekly
Rolling run below ([#79](https://github.com/atinfinity/fastdds_transport_viz/issues/79)). Test
result XML files and launch logs are uploaded as a workflow artifact per distribution,
architecture and RMW. The matrix's `rmw` axis is `rmw_fastrtps_cpp` everywhere plus one
`rmw_fastrtps_dynamic_cpp` job each for Humble, Jazzy and Lyrical on x86_64
([#73](https://github.com/atinfinity/fastdds_transport_viz/issues/73)); `package.xml`
declares `rmw_fastrtps_dynamic_cpp` as a test dependency so `rosdep` installs it. These
jobs block pull requests like the default-RMW ones. `main` is protected: pull requests merge only when the `CI result` and
`Docs result` jobs are green; they succeed when every job of their workflow passed or was
skipped for a change that does not concern it (Rolling does not count).

A second job, `integration`, runs `scripts/integration_test.sh all` on the runner VM,
once on x86_64 and once on arm64, for the merge commit on `main` and on
`workflow_dispatch` (not for pull requests, to keep PR CI short); the `transport_viz` JSON
of each scenario is uploaded as an artifact per architecture. Two more x86_64 rows run
selected scenarios on other images: Lyrical for the Easy Mode scenarios the Jazzy image skips
and the split IPC namespace ones, Humble for the scenarios Fast DDS 2.6 can run there, and
both for `intra_process` and `record_flip`. The `integration` matrix in `ci.yml` is the list,
with a comment on why each scenario is in it. The matrix itself does not
run again on `main`: each change is built once, in its pull request. The build-and-test,
`integration` and `coverage` jobs have a 30-minute `timeout-minutes`, the `web viewer tests
(node)` job 10 minutes (queue time excluded). The `rosdep` / `colcon build` /
`colcon test` steps live in the composite action `.github/actions/colcon-build-test`, which
the Rolling workflow shares.

An `actionlint` job runs [actionlint](https://github.com/rhysd/actionlint) (with shellcheck
on the `run:` scripts) over every workflow when a change touches `.github/workflows/**` or
`.github/actions/**`, on any event; it counts towards `CI result`. Its version is pinned in
the job (`ACTIONLINT_VERSION`) and Dependabot does not bump it. Locally, `actionlint` from
the repository root checks the same files.

A `coverage` job runs `scripts/coverage.sh` in `ros:jazzy` on x86_64 for pull requests
that touch code and for the merge commit on `main`
([#80](https://github.com/atinfinity/fastdds_transport_viz/issues/80)): the C++ package
built with `--coverage`, its tests, and `gcovr` writing the summary to the job summary, an
HTML report to the `coverage-html-jazzy-x86_64` artifact and an lcov file (lines only:
Coveralls would fold gcovr's branch records into its percentage) that
`coverallsapp/github-action` uploads with the workflow token (no repository secret).
Coveralls provides the README badge (`main`) and comments the delta on pull requests. The
job counts towards `CI result` like the matrix - a failing coverage build or test blocks -
but nothing gates on the percentage, and a Coveralls outage does not fail the job
(`fail-on-error: false`).

`.github/workflows/rolling.yml` runs every Monday at 03:00 UTC (and on `workflow_dispatch`)
against `ros:rolling`, which tracks the Fast DDS head and is only a non-blocking x86_64 job
in the pull-request matrix ([#79](https://github.com/atinfinity/fastdds_transport_viz/issues/79)):
`colcon build` and `colcon test` on x86_64 and arm64 (plus `rmw_fastrtps_dynamic_cpp` on
x86_64, 30 minutes each) and `scripts/integration_test.sh all` on both architectures (45
minutes each), none of them `continue-on-error`. Its `report` job opens the issue "Scheduled Rolling run failed" (label
`rolling-ci`) with the run URL and the failed jobs when something broke, adds a comment to it
while it stays open, and closes it once a run passes again. A `simulate_failure` input of
`workflow_dispatch` exercises the issue path without a real failure. `.github/dependabot.yml`
keeps the actions pinned in the workflows current, as one grouped pull request a week
(Monday), verified by the ordinary matrix.

## Scale verification

How the tool holds up on a large system
([#74](https://github.com/atinfinity/fastdds_transport_viz/issues/74)). Run on the Docker
host, one scenario at a time (they share the machine with the load); not part of CI:

```
[ROS_DISTRO=jazzy] scripts/scale_test.sh <small|medium|large|large_multi|limit|nav2> [--no-build]
```

| Scenario | Load |
|---|---|
| `small` / `medium` / `large` | 10 / 20 / 40 `scale_load` processes of one node each, 100 / 500 / 1000 topics, 4 readers per topic: 400 / 2000 / 4000 `/scale` pairs, plus `/parameter_events` (every node publishes and subscribes: processes² pairs) and `/rosout` |
| `large_multi` | `large` split across two bridged containers: processes 0–19 in `scale_load_a` (the tool's container, SHM among them), 20–39 in `scale_load_b` (UDPv4 to the others) |
| `limit` | records only: 60, 90, 135, … processes with 25 topics each, until the load or the tool falls over or less than 10 % of the memory is left; `limit_p<N>` per step |
| `nav2` | Nav2 + TurtleBot3 (`tb3_simulation_launch.py headless:=True`, default composition, initial pose, no goal) in the `nav2_tb3` service (image target `nav2`; Jazzy only: no Nav2 binaries for Lyrical, and Kilted, which has them, is out of scope since 1.1.0) |

The loads run with `FASTDDS_STATISTICS` in compose services of the `scale` profile. The tool
runs inside the load's container (`docker compose exec`, so it shares the host id and
`/dev/shm`) through `scripts/scale_measure.py`, after a 20 s warm-up (90 s for Nav2):

- one-shot: the default table three times (median), and once with `-v` for its line count;
- `--stats --json` with the default stop rule (settled, or the 30 s cap; [#168](https://github.com/atinfinity/fastdds_transport_viz/issues/168)) and at `--timeout 30`;
- `--watch --interval 2 --stats` for 60 s, without and with `-v`.

Budgets (the `limit` steps only record):

| Budget | Limit | Measured as |
|---|---|---|
| one-shot table | < 2 s | `collect` + `render` after the discovery wait, median of three |
| `--watch` frame | median < 250 ms, p95 < 500 ms | median and p95 of the `frame` times of the three 60 s runs (`--stats`, `--stats -v` and without `--stats`), the worst run of the three for each; the `Host` column and the `FAIL` line say what the host was doing (whole-VM cores before the tool and during the slowest watch run, the tool's own cores) ([#135](https://github.com/atinfinity/fastdds_transport_viz/issues/135), [#177](https://github.com/atinfinity/fastdds_transport_viz/issues/177)) |
| web viewer | first render < 3 s, every action (filter, select) < 100 ms at `large` | the 30 s `--stats` document, by hand (below the `FTV_PROFILE` table) |
| tool CPU | < 1 core | CPU time / wall time of the `--watch` runs and the 30 s `--stats` run, the highest |
| tool memory | < 300 MB | peak RSS (`wait4`) over every run |
| `--watch` statistics coverage | ≥ 95 % | pairs with measured packets / pairs whose deliveries `HISTORY_LATENCY` proves, at the last frame of a 60 s `--watch` run (`pairs_delivered_unmeasured` and `pairs_delivered` of the `apply_stats` phase, `stats.*` in the document), the lower of the two `--stats` runs; not judged when neither run saw a delivered pair ([#147](https://github.com/atinfinity/fastdds_transport_viz/issues/147)) |
| `--stats` coverage | ≥ 95 % | pairs with measured packets (`measured.packets > 0`, as for the `--watch` coverage) in the default `--stats` one-shot among those at `--timeout 30`; for the synthetic loads only the `/scale` pairs count (`/parameter_events` often sends nothing within a short window), for Nav2 every pair; 0 when the 30 s run measured none of them ([#153](https://github.com/atinfinity/fastdds_transport_viz/issues/153), [#168](https://github.com/atinfinity/fastdds_transport_viz/issues/168)) |

A `--watch` run whose frames never held a pair fails its row ("saw no pairs"): its frame times
would pass without timing anything. Only `medium` is judged: there `scale_test.sh` runs
`scale_measure.py --judge`, which prints `PASS` or `FAIL - <budget> <value> (must be …)` and
exits 1 when a budget with a value fails or the load died, on every distribution (Humble's
`stats_*` budgets have no value and are not judged), and `scale_test.sh` exits with it
([#167](https://github.com/atinfinity/fastdds_transport_viz/issues/167)). The other rungs
exit 0 whatever their row says: `large` takes most of the host and `limit` is meant to exceed
the budgets. The output of the runs is kept as
`<label>.watch-plain.txt` / `<label>.watch-verbose.txt` / `<label>.watch-nostats.txt`.

Machine: the host of every row (architecture, CPUs and memory as the containers see them) is
written into its row; nothing is pinned, the load and the tool share the machine. The CPU of
the whole VM is recorded with the load alone for 5 s before the tool runs
(`load_before.vm_cores_load_only`) and during every run, next to the tool's. The result lands
in `build/<distro>/scale/<label>.json` with every run and phase time, `<label>.viz.json` (the
30 s document), `<label>.stats-default.json` (the default one-shot), `<label>.table-v.txt`, and the Markdown
row in `rows.md`; copy the row into [Scale results](verification-log.md#scale-results).

The one-shot pair count in a row is the lowest of the three default runs. Under load the
discovery events arrive in bursts, and the default `--quiet 1` can stop between two of them
and print part of the system; on a large system, pass `--quiet 3` or a longer `--timeout`
(see `discovery` in the profile below).

`FTV_PROFILE=1` makes `transport_viz` write one JSON line per phase to stderr (not a stable
interface; nothing in `--json` changes):

| `ftv_profile` | When | Fields besides `ms` |
|---|---|---|
| `discovery` | after the discovery wait, from start | `endpoints` (every endpoint the raw participant knows), `events` (discovery callbacks), `first_event_ms` (polled every 50 ms, 0 when nothing was discovered), `last_event_ms` |
| `drain` | `--stats`: reading the statistics readers | `samples`, `sample_lost`, `sample_lost_latency`, `sample_lost_at_start`, `sample_rejected`, `writers_incompatible_qos`, `drain_errors` (cumulative) |
| `resolve` | node names from the ROS graph (two rmw queries per topic), only on the frames that follow a discovery event or a `ros_discovery_info` sample, or come within 5 s of the last event ([#135](https://github.com/atinfinity/fastdds_transport_viz/issues/135)) | `refreshed` (1: the frame queried the graph) |
| `summarize` | pairing writers and readers | `endpoints`, `topics`, `pairs` (before the view filters) |
| `apply_stats` | `--stats` overlay | `pairs_delivered`, `pairs_delivered_unmeasured`, `pairs_delivered_absent` (the `stats.*` of the same names) |
| `collect` | the whole snapshot, the three above included | `endpoints`, `topics`, `pairs` (as shown) |
| `update` | `--watch`: changes against the previous frame | |
| `render` | table or JSON text | `bytes`, `lines` |
| `frame` | `--watch`: collect + update + render + output | `pairs` |

Web viewer, by hand (automation:
[#81](https://github.com/atinfinity/fastdds_transport_viz/issues/81)): serve the repository
root (`python3 -m http.server 8000` on the Docker host), open
`http://localhost:8000/web/index.html` and run `scripts/scale_viewer.js` in the page with
`SRC` set to `/build/<distro>/scale/<label>.viz.json`. It loads the viewer five times in a
1400×900 iframe and reports medians:
- first render: from creating the iframe until the graph's SVG holds its edges (page, fetch, parse, layout);
- filter response: from setting a filter input and dispatching `input` until the viewer has rendered (its `document.body.dataset.render` counter moves) and one `requestAnimationFrame` later; since [#136](https://github.com/atinfinity/fastdds_transport_viz/issues/136) this includes the viewer's 100 ms typing debounce, so a filter cell reads about 100 ms above the work it timed;
- the filters are a topic filter `t00` (100 topics), a node filter `p00` (10 processes), and clearing each;
- select: the same from a click on the first arrow, and from the click on the background that clears the selection (#136).

Two animation frames at 60 Hz are the floor of every response cell: about 33 ms.

The tab must be in front: a hidden tab pauses `requestAnimationFrame`, and the snippet never
finishes. A tab driven by Claude in Chrome in a background window is hidden; the numbers in the log
were taken in headless Chrome (`--headless=new`, `Runtime.evaluate` over the DevTools
protocol), where the page counts as visible.

### Current status

As of 2026-09-22 ([Scale results](verification-log.md#scale-results) has every row):

- `medium` passes on Jazzy and Lyrical; the judged run can still fail when the host is busy,
  so re-run it before blaming a change.
- `large`, `large_multi` and the `limit` steps are over budget and not judged: the load alone
  takes most of the host, and the tool gets what is left.
- Humble has no statistics, so its `stats_*` budgets have no value.
- The web viewer holds its budget at `large` since [#136](https://github.com/atinfinity/fastdds_transport_viz/issues/136).

## Quality declaration

Both packages declare REP 2004 Quality Level 3 (Linux only, so with an exception for
Windows 10): [`fastdds_transport_viz`](https://github.com/atinfinity/fastdds_transport_viz/blob/main/src/fastdds_transport_viz/QUALITY_DECLARATION.md),
[`ros2transport`](https://github.com/atinfinity/fastdds_transport_viz/blob/main/src/ros2transport/QUALITY_DECLARATION.md), and the
[security policy](https://github.com/atinfinity/fastdds_transport_viz/blob/main/SECURITY.md)
([#87](https://github.com/atinfinity/fastdds_transport_viz/issues/87)). They state facts about
this repository, so keep them true: update them in the same pull request when a change
touches what they declare - the public API or its versioning, a runtime dependency in
`package.xml`, the supported distributions or platforms (the CI matrix), the required checks
or the security policy.

## Documentation site

`mkdocs.yml` builds this documentation with Material for MkDocs and `mkdocs-static-i18n`
(English at `/`, Japanese at `/ja/` from the `*.ja.md` files, English fallback for
untranslated pages). `.github/workflows/docs.yml` runs `mkdocs build --strict` on pull
requests and deploys to GitHub Pages on pushes to `main`, in both cases only when `docs/**`,
any `*.md` file, `mkdocs.yml` or the workflow itself changed (the aggregate `Docs result`
check passes otherwise). Broken links and anchors fail the build: `mkdocs.yml` sets its
`validation` checks to `warn`, and `--strict` turns warnings into errors. The same workflow
runs [lychee](https://github.com/lycheeverse/lychee) offline over every Markdown file of
the repository, which covers the files outside `docs/` and the links as GitHub renders them.
The two differ on one point: on the site a Japanese page's link to `x.md` goes to `x.ja.md`,
on GitHub it does not, so a Japanese page links `x.ja.md` directly. The English-only pages
(architecture, development) are also served under `/ja/`, where their links go to the
Japanese pages; an anchor they use there needs an `<a id="...">` in the `.ja.md` file (see
how-it-works.ja.md). Locally:

```
pip install -r docs/requirements.txt
mkdocs serve          # http://127.0.0.1:8000/
docker run --rm -v "$PWD":/w -w /w lycheeverse/lychee --offline --include-fragments --exclude-path build './**/*.md'
```

## Colored output examples

`docs/images/example-table.svg` and `example-watch.svg` (used by the README and
how-it-works) are real output: `scripts/render_examples.sh` starts the demo and
verification nodes in the dev container, captures `transport_viz --color always` into
`docs/images/*.ansi`, and `scripts/ansi2svg.py` renders the ANSI colors as SVG. Re-run
the script (Docker host) after changing the table layout or the palette and commit the
`.ansi` and `.svg` files; it is not part of CI.

## Japanese documentation

`README.ja.md` and `docs/*.ja.md` mirror the English user documentation (README,
index, getting-started, how-it-works, statistics, data-sharing, web-viewer); English is the
source of truth and tool output stays English. When one of those English files changes,
update its `.ja.md` and the "as of" date in its header. The developer guide (this file
and architecture.md) has no translation.

## Layout

See [architecture.md](architecture.md): components, the flow of one run, the data model,
the Fast DDS 2.14 / 3.x compatibility layer, the repository layout and extension points.

## Release procedure

Both packages are released together under one version; 1.1.0 and 2.0.0 were made this way.

1. Check that the CHANGELOGs are complete: go through `git log vX.Y.Z..main` (the last tag)
   and add a `Forthcoming` entry for every user-visible change that lacks one, in
   `src/fastdds_transport_viz/CHANGELOG.rst` and `src/ros2transport/CHANGELOG.rst`.
2. Choose the version by the `semver` rules of the
   [quality declaration](https://github.com/atinfinity/fastdds_transport_viz/blob/main/src/fastdds_transport_viz/QUALITY_DECLARATION.md#api-stability-policy-1iv)
   (a breaking change to the declared public API is a major release), and list the breaking
   changes at the top of the new section.
3. On a `release/X.Y.Z` branch, set the version in both `package.xml` files and in
   `src/ros2transport/setup.py`.
4. In both CHANGELOGs, turn `Forthcoming` into `X.Y.Z (YYYY-MM-DD)`.
5. Update the [Roadmap](#roadmap) below: the Done entries, the Open list and its "As of" date.
6. Open the pull request and merge it once CI is green.
7. Tag the merge commit and push the tag:
   `git tag -a vX.Y.Z -m "fastdds_transport_viz X.Y.Z" <merge commit>` and
   `git push origin vX.Y.Z`.
8. `gh release create vX.Y.Z --title vX.Y.Z --notes-file <notes> --latest`, with a short
   summary followed by the CHANGELOG section as the notes.

Releasing into the ROS build farm with `bloom-release` is not done yet
([#50](https://github.com/atinfinity/fastdds_transport_viz/issues/50)).

## Roadmap

As of 2026-09-22. The [issue tracker](https://github.com/atinfinity/fastdds_transport_viz/issues)
is the source of truth; update this list when closing an issue.

Done:

- M1 discovery-based prediction, table/JSON, tests, Docker env
- M2 `--stats`: measured per-locator traffic via the Fast DDS statistics module, host
  names, mismatch detection
- M3 data-sharing confidence from `HISTORY_LATENCY` + traffic
- M4 multi-host verification with containers on x86_64 and arm64 —
  [#1](https://github.com/atinfinity/fastdds_transport_viz/issues/1)
- M5 richer `--watch`: change marks, colors, keys, alternate screen —
  [#2](https://github.com/atinfinity/fastdds_transport_viz/issues/2)
- Web viewer for `--json` (graph/table) —
  [#3](https://github.com/atinfinity/fastdds_transport_viz/issues/3), live mode
  (`transport_viz_web`) — [#10](https://github.com/atinfinity/fastdds_transport_viz/issues/10)
- `ros2 transport list` / `codes` (ros2cli extension) —
  [#4](https://github.com/atinfinity/fastdds_transport_viz/issues/4)
- `--node` filter — [#7](https://github.com/atinfinity/fastdds_transport_viz/issues/7)
- Transport configurations verified: `LARGE_DATA`/TCPv4, UDPv6, Discovery Server,
  `LOCALHOST`/`OFF`, large SHM samples —
  [#6](https://github.com/atinfinity/fastdds_transport_viz/issues/6)
- Data-sharing confirmed with `--stats` through `DATA_COUNT` —
  [#9](https://github.com/atinfinity/fastdds_transport_viz/issues/9)
- Web viewer: node filter — [#18](https://github.com/atinfinity/fastdds_transport_viz/issues/18)
- Fast DDS 3.x (Kilted / Rolling) next to 2.14 (Jazzy) —
  [#5](https://github.com/atinfinity/fastdds_transport_viz/issues/5)
- Humble (Fast DDS 2.6): prediction, statistics unavailable in the binary —
  [#48](https://github.com/atinfinity/fastdds_transport_viz/issues/48)
- Japanese README and user docs — [#8](https://github.com/atinfinity/fastdds_transport_viz/issues/8)
- the shared-memory line (`/dev/shm` capacity, Fast DDS files, stale files,
  IPC-namespace visibility)

- Two physical hosts on a Wi-Fi LAN (x86_64 ↔ Jetson Orin NX), prediction and `--stats`
  in both directions — [#15](https://github.com/atinfinity/fastdds_transport_viz/issues/15)
- QoS request/offer check: incompatible pairs shown as `NONE` —
  [#45](https://github.com/atinfinity/fastdds_transport_viz/issues/45)
- `LATENCY` column from `HISTORY_LATENCY` —
  [#46](https://github.com/atinfinity/fastdds_transport_viz/issues/46)
- `LOSS` column and reliability counters (`RTPS_LOST`, resends, heartbeats, acknacks) —
  [#47](https://github.com/atinfinity/fastdds_transport_viz/issues/47)
- Selected and measured locators: `--locators`, the JSON `locator` /
  `measured.locators[]` fields and the `measured-locator-mismatch` warning —
  [#63](https://github.com/atinfinity/fastdds_transport_viz/issues/63)
- ROS 2 Lyrical Luth (Fast DDS 3.6) in place of Kilted, which reaches EOL in December
  2026 — [#68](https://github.com/atinfinity/fastdds_transport_viz/issues/68)
- Release 1.1.0: `--locators`, the locator fields in the JSON, Lyrical in place of
  Kilted — [#70](https://github.com/atinfinity/fastdds_transport_viz/issues/70)
- CI on arm64 runners (Jazzy / Lyrical build & test, integration scenarios) —
  [#75](https://github.com/atinfinity/fastdds_transport_viz/issues/75)
- Refuse to start on an RMW other than Fast DDS's (exit 1, naming it) —
  [#72](https://github.com/atinfinity/fastdds_transport_viz/issues/72)
- `rmw_fastrtps_dynamic_cpp` verified and accepted; CI runs the suite on it —
  [#73](https://github.com/atinfinity/fastdds_transport_viz/issues/73)
- `transport_viz diff` / `ros2 transport diff`: compare two `--json` snapshots; the web
  viewer compares two documents and highlights `changes` —
  [#77](https://github.com/atinfinity/fastdds_transport_viz/issues/77)
- Fast DDS 3.x Easy Mode (`ROS2_EASY_MODE`) and the `P2P` builtin transport verified on
  Kilted 3.2.4 and Lyrical 3.6.2 (launch test, `easy_mode_shm` / `easy_mode_tcp`
  scenarios); the discovery CLI no longer corrupts `--json` —
  [#71](https://github.com/atinfinity/fastdds_transport_viz/issues/71)
- Scale verification on Nav2 + TurtleBot3 and a synthetic `scale_load` ladder
  (`scripts/scale_test.sh`, `FTV_PROFILE`, "Scale results") —
  [#74](https://github.com/atinfinity/fastdds_transport_viz/issues/74)
- Type mismatches on the same topic: `type-name-mismatch` as a `NONE` pair and the
  `type-hash-mismatch` warning from the REP-2011 type hash —
  [#85](https://github.com/atinfinity/fastdds_transport_viz/issues/85)
- Services and actions grouped under `--all`: one `SERVICE` / `ACTION` row per
  client-server pair, `kind` / `group` / `direction` in `--json`, the same grouping in the
  web viewer's Table tab —
  [#84](https://github.com/atinfinity/fastdds_transport_viz/issues/84)
- Metrics export: `/metrics` in the Prometheus text format from `transport_viz_web`, and
  `--csv` (one row per pair) —
  [#83](https://github.com/atinfinity/fastdds_transport_viz/issues/83)
- REP 2004 quality declarations (Level 3) for both packages and `SECURITY.md` —
  [#87](https://github.com/atinfinity/fastdds_transport_viz/issues/87)
- `--advise`: the remedy of every reason code —
  [#76](https://github.com/atinfinity/fastdds_transport_viz/issues/76)
- CI: Dependabot for the actions, a weekly Rolling run, a coverage job with a Coveralls
  badge — [#79](https://github.com/atinfinity/fastdds_transport_viz/issues/79),
  [#80](https://github.com/atinfinity/fastdds_transport_viz/issues/80)
- Web viewer: browser-level tests, record and replay with a timeline, the live history,
  Discovery Servers and their clients —
  [#81](https://github.com/atinfinity/fastdds_transport_viz/issues/81),
  [#82](https://github.com/atinfinity/fastdds_transport_viz/issues/82),
  [#218](https://github.com/atinfinity/fastdds_transport_viz/issues/218),
  [#86](https://github.com/atinfinity/fastdds_transport_viz/issues/86)
- Release 2.0.0: split IPC namespaces, the `HZ` column, the settle rule, type
  mismatches, services and actions, `diff`, record/replay, metrics export
- Release 2.0.1: documentation brought in line with 2.0.0, the verification log, the
  release procedure, link and workflow checks in CI —
  [#224](https://github.com/atinfinity/fastdds_transport_viz/issues/224)

Open, by priority (labels `priority/1-high` … `priority/3-low` on the issues):

`priority/1-high`: none open.

`priority/2-medium`:

- Distribution: bloom release for Jazzy/Humble/Lyrical — [#50](https://github.com/atinfinity/fastdds_transport_viz/issues/50)
- `transport_viz` segfaulted once on Rolling CI — [#215](https://github.com/atinfinity/fastdds_transport_viz/issues/215)
- Report the Fast DDS 3.6 statistics heartbeat stall upstream — [#157](https://github.com/atinfinity/fastdds_transport_viz/issues/157)
- Report the per-socket statistics sequence stamping upstream — [#198](https://github.com/atinfinity/fastdds_transport_viz/issues/198)

`priority/3-low`:

- DDS Security (SROS2) — [#49](https://github.com/atinfinity/fastdds_transport_viz/issues/49)
- Ready-to-run Docker image on GHCR — [#78](https://github.com/atinfinity/fastdds_transport_viz/issues/78)
- Report to eProsima: Easy Mode hides endpoints from a host without a node of their type — [#100](https://github.com/atinfinity/fastdds_transport_viz/issues/100)
