# Development, verification and tests

## Docker environment

The repository ships a `compose.yaml` and `docker/Dockerfile` based on `ros:jazzy`
(multi-arch: x86_64 and arm64). `ROS_DISTRO=lyrical docker compose build` (or `rolling`)
builds the same environment on Fast DDS 3.x, `ROS_DISTRO=humble` on Fast DDS 2.6; the image is tagged
`fastdds_transport_viz:<distro>` and every `docker compose` command below then needs
the same `ROS_DISTRO` in the environment.

```
docker compose build
docker compose run --rm dev bash       # shell in the dev container, repo mounted at /ws
colcon build --symlink-install
source install/setup.bash
```

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
| `stats_multi_container` | `talker_stats`, `listener_stats`: as above with `FASTDDS_STATISTICS` | measured `UDPv4`, two different `PHYSICAL_DATA` host names |
| `hostnet_shm` | two `hostnet` containers: `network_mode: host` + `ipc: host` ⇒ same host id, shared `/dev/shm` | `SHM`, `same-host-guid`, the nodes' segments visible in `shm` |
| `hostnet_noipc_shm` | `pair_hostnet_noipc`: talker + listener in one container, `network_mode: host` without `ipc: host` ⇒ the tool's host id, another `/dev/shm`; the tool in `hostnet` | `SHM`, `same-host-guid`, `shm-not-visible` with every SHM port of the nodes missing |
| `hostnet_split_shm` | `talker_hostnet_split`, `listener_hostnet_split`: `network_mode: host`, an IPC namespace each ⇒ same host id, two `/dev/shm`; the tool in `hostnet` | `NONE`, `shm-ipc-namespace-split`, `shm-port-collision` |
| `hostnet_split_shm_visible` | `talker_hostnet_split_visible` (a node started first takes the 7000 port) and `listener_hostnet_split`; the tool in `hostnet_in_talker_ipc`, the talker's IPC namespace (Jazzy or newer, skipped on Humble) | `NONE`, `shm-ipc-namespace-split`, `shm-reader-port-not-visible`, no `shm-port-collision` |
| `hostnet_split_stats` | `talker_hostnet_split_stats`, `listener_hostnet_split_stats`: `hostnet_split_shm` with `FASTDDS_STATISTICS`; the tool in `hostnet` with `--stats` (skipped on Humble, which has no statistics module) | the writer's statistics arrive, `NONE`, `shm-ipc-namespace-split`, `measured-shm-traffic`, not delivered, no `stats-not-enabled-on-writer` |
| `hostnet_split_datasharing` | `bounded_pub_hostnet_split`, `bounded_sub_hostnet_split`: `hostnet_split_shm` with `bounded_pub` / `bounded_sub` and data-sharing (`datasharing_auto.xml`); the tool in `hostnet` | `/bounded` `NONE`, `shm-ipc-namespace-split`, `datasharing-qos-enabled-both`, `shm-port-collision` |
| `hostnet_split_datasharing_udp` | `bounded_pub_hostnet_split_udp`, `bounded_sub_hostnet_split_udp`: the same with `FASTDDS_BUILTIN_TRANSPORTS=UDPv4`; the tool in `hostnet_in_bounded_pub_ipc`, the publisher's IPC namespace (skipped on Humble) | `/bounded` `NONE`, `shm-ipc-namespace-split`, `datasharing-reader-segment-not-visible`, no SHM reason |
| `large_data_tcp` | `talker_large_data`, `listener_large_data`: bridged, `FASTDDS_BUILTIN_TRANSPORTS=LARGE_DATA` + statistics | `TCPv4`, `common-tcpv4-locator`, measured `TCPv4` |
| `udpv6_multi_container` | `talker_udpv6`, `listener_udpv6`: bridged (the project network has IPv6), `DEFAULTv6` | `UDPv6`, `common-udpv6-locator` |
| `easy_mode_shm` | two `hostnet` containers with `ROS2_EASY_MODE=127.0.0.1` (Fast DDS 3.2+: `ROS_DISTRO=kilted`, `lyrical` or `rolling`, skipped otherwise) | `SHM`, `same-host-guid`, no multicast locator (P2P) |
| `easy_mode_tcp` | `talker_easy_mode`, `listener_easy_mode`: bridged with fixed addresses, `ROS2_EASY_MODE` pointing at the talker's, statistics; the tool runs on the talker's host | `TCPv4`, `common-tcpv4-locator`, measured `TCPv4`, no multicast locator |
| `all` | the thirteen above in sequence | |

Output goes to `${TMPDIR:-/tmp}/transport_viz_<scenario>.json`.

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
(see the results table). Jetson specifics: Docker on JetPack cannot create bridge
networks (`iptables: can't initialize iptables table 'raw'`), so build the image with
`docker build --network=host --build-arg ROS_DISTRO=jazzy -t fastdds_transport_viz:jazzy
-f docker/Dockerfile .` instead of `docker compose build`; `hostnet` itself needs no
bridge. The image build takes about 10 minutes on the Orin NX.

Easy Mode (Fast DDS 3.2+) is the built-in answer to a LAN without multicast: set
`ROS2_EASY_MODE=<address of host A>` on both hosts and the tool, nothing else. Verified on
2026-09-13 with the same two machines (Jetson re-installed with Ubuntu 24.04, Lyrical
image on both, started with `docker run --rm --network host --ipc host -v
$PWD:/ws fastdds_transport_viz:lyrical ros2 run ...` on the Jetson): `TCPv4`, measured
`TCPv4` in both directions, seen from either host (see the results table and
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
- `test_decision` also covers `diff_snapshots()`: the GUID key sees a restart as removed +
  added, the node key does not, several endpoints of one node, the GUID fallback.
- `test_cli_args` (pytest): `--help`, `--list-codes`, unknown options, missing values,
  invalid regexes and color modes exit with the documented codes and messages; `--color
  always` paints a non-terminal table; the `ROS_AUTOMATIC_DISCOVERY_RANGE=OFF` warning;
  `transport_viz diff` on the fixture pair `web/sample/diff_before.json` /
  `diff_after.json` (node vs GUID key, `--changes-only`, the `--json` shape, filters, stdin
  and JSON Lines input, colors, every exit status).
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
  `test_discovery_server.py` (`fastdds discovery` server, SUPER_CLIENT vs plain client),
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
  GUID key; a second listener is an added pair under both).
- `test_json_schema` / `test_json_schema_live.py`: sample and live `--json` output against
  `schema/transport_viz.schema.json`.
- `test_web_serve` (pytest, fake `transport_viz`) / `test_web_live.py` (real one): the live
  server's SSE stream, `/latest.json` and shutdown behaviour.
- `ros2transport/test/test_cli.py` (pytest, fake `transport_viz`): argument translation of
  `ros2 transport list` and `diff`, parity with the binary's `--help`, `codes`, missing
  binary and missing input files.
  `test_list_live.py`: `ros2 transport list --json` against real demo nodes.

Line coverage of the C++ sources, measured with `scripts/coverage.sh` inside the dev
container (a `--coverage` build in `build_cov/`, the whole test suite, then `gcovr`):
98 % as of 2026-09-06 (`shm_info.cpp` 100 %, `main.cpp`, `render_table.cpp`,
`render_json.cpp` 99 %, `stats_observer.cpp` 98 %, `decision.cpp` and
`discovery_observer.cpp` 96 %). What is left is unreachable by construction: subscriber
and reader creation failures inside Fast DDS, `getifaddrs` errors, `default:` labels of
switches over enums whose every value is handled, and discovery statuses Fast DDS 2.14 never
reports for our participant.

The web viewer's pure functions (`web/model.js`: document → nodes/hosts/pairs, filters
with the `--node` semantics, edge bundling, number formatting, the shared-memory line, and
the comparison of two documents: `diffDocuments()` must reproduce `web/sample/diff.json`,
what the binary printed for `diff --all --json` on the fixture pair, which `test_cli_args`
asserts from the C++ side) are unit-tested under Node without a browser:

```
node --test "web/test/*.test.js"
```

CI runs this in the `web viewer unit tests (node)` job; `app.js` (DOM, d3, live mode) is
exercised through `test_web_live.py`.

## Continuous integration

`.github/workflows/ci.yml` runs `rosdep install`, `colcon build`, `colcon test` inside
`ros:humble`, `ros:jazzy`, `ros:lyrical` and `ros:rolling` containers for every pull request that touches
code (docs-only changes skip the job); Rolling may break with upstream changes and does
not block (`continue-on-error`). Jazzy and Lyrical are built and tested twice, on the
x86_64 runner and on GitHub's `ubuntu-24.04-arm` runner (the `ros:<distro>` images are
multi-arch); the arm64 jobs block pull requests like the x86_64 ones. Humble on arm64 is
not run (Fast DDS 2.6, prediction only), and Rolling on arm64 is left to the scheduled
Rolling run ([#79](https://github.com/atinfinity/fastdds_transport_viz/issues/79)). Test
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
selected scenarios on other images: Lyrical for `easy_mode_shm`, `easy_mode_tcp`,
`hostnet_noipc_shm`, `hostnet_split_shm`, `hostnet_split_shm_visible`,
`hostnet_split_stats`, `hostnet_split_datasharing` and `hostnet_split_datasharing_udp`,
Humble for `hostnet_split_shm`, `hostnet_noipc_shm` and `hostnet_split_datasharing`. The matrix itself does not
run again on `main`: each change is built once, in its pull request. Every job has a
30-minute `timeout-minutes` (queue time excluded).

## Verification results

| Date | Scenario | Arch | Fast DDS | Result | Reproduce |
|---|---|---|---|---|---|
| 2026-09-04 | two bridged containers | arm64 | 2.14 (`ros:jazzy`) | `UDPv4`, `different-host` | `scripts/integration_test.sh multi_container` |
| 2026-09-05 | two bridged containers | x86_64 | 2.14.6 | `UDPv4`, `different-host` | `scripts/integration_test.sh multi_container` |
| 2026-09-05 | two bridged containers, `--stats` | x86_64 | 2.14.6 | measured `UDPv4`, host names differ (container ids) | `scripts/integration_test.sh stats_multi_container` |
| 2026-09-05 | two `hostnet` containers | x86_64 | 2.14.6 | `SHM`, `same-host-guid`, no `host-id-match-but-ip-differs` | `scripts/integration_test.sh hostnet_shm` |
| 2026-09-05 | `LARGE_DATA`, two bridged containers, `--stats` | x86_64 | 2.14.6 | `TCPv4`, `common-tcpv4-locator`, measured `TCPv4` (physical port matches `RTPS_SENT`) | `scripts/integration_test.sh large_data_tcp` |
| 2026-09-05 | `LARGE_DATA`, one host | x86_64 | 2.14.6 | TCPv4 announced, `SHM` chosen (`both-shm-locators`) | `test_large_data_same_host.py` |
| 2026-09-05 | `DEFAULTv6`, two bridged containers (IPv6 network) | x86_64 | 2.14.6 | `UDPv6`, `common-udpv6-locator` | `scripts/integration_test.sh udpv6_multi_container` |
| 2026-09-05 | `UDPv6`, one host | x86_64 | 2.14.6 | `UDPv6`, `same-host-guid` (needs an IPv6 interface; multicast only, `ROS_STATIC_PEERS` is IPv4-only in Jazzy) | `test_udpv6.py` |
| 2026-09-05 | `ROS_DISCOVERY_SERVER` (`fastdds discovery` server on 127.0.0.1:11811) | x86_64 | 2.14.6 | `SHM` pair seen as SUPER_CLIENT; a plain CLIENT does not see `/chatter` | `test_discovery_server.py` |
| 2026-09-05 | `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` | x86_64 | 2.14.6 | `SHM`, nodes announce `127.0.0.1` only; `OFF` sees nothing (warning printed) | `test_localhost_range.py` |
| 2026-09-05 | 2 MB samples over SHM, `--stats` | x86_64 | 2.14.6 | `SHM`, measured `SHM`, bytes consistent with the sample size | `test_large_shm.py` |
| 2026-09-06 | reliability counters over Wi-Fi: x86_64 ↔ Jetson Orin NX (Docker `hostnet`, both directions, `--stats` with all 11 aliases) | x86_64 + arm64 | 2.14.6 both | `LOSS` `0` (no `RTPS_LOST`, no resends at 1 Hz), 3–4 heartbeats and acknacks per 10 s observation, `LATENCY` ≈ 46 ms mean / 93 ms max (includes the clock offset of the two hosts) | "Two physical hosts" |
| 2026-09-06 | shared-memory line: nodes in the tool's IPC namespace visible (`hostnet_shm`), bridged containers reported `shm-not-visible` (`multi_container`) | x86_64 | 2.14.6 | as expected | `scripts/integration_test.sh multi_container`, `hostnet_shm`, `test_shm.py` |
| 2026-09-13 | shared-memory line, host network: the tool alone; talker + listener in one container with `network_mode: host` and without `ipc: host`; the same with `ipc: host` (tool in `hostnet`) | arm64 | 2.6 (`ros:humble`), 2.14.6 (`ros:jazzy`), 3.6 (`ros:lyrical`) | alone: `checked_ports` empty (Lyrical reported its own ports `[7000, 19411]` as the nodes'); without host IPC: every port of the nodes missing, `shm-not-visible` (including 7000/7001, the same numbers as the tool's own); with host IPC: visible. `/chatter` is `SHM`, `certain` in both node cases, yet without host IPC a listener in a *second* such container receives nothing ([#101](https://github.com/atinfinity/fastdds_transport_viz/issues/101)). Also with `rmw_fastrtps_dynamic_cpp` on Lyrical. Found and fixed: the tool's rmw participant was recognised only through endpoints resolving to its node name, none on Lyrical/Rolling ([#51](https://github.com/atinfinity/fastdds_transport_viz/issues/51)) | `scripts/integration_test.sh hostnet_noipc_shm`, `test_shm_own_ports.py` |
| 2026-09-13 | same host id, separate IPC namespaces: talker and listener on the host network in an IPC namespace each, the tool in `hostnet`; the same with another node started first in the talker's container and the tool in the talker's IPC namespace; unit suites on the three distributions | arm64 | 2.6 (`ros:humble`), 2.14.6 (`ros:jazzy`), 3.6 (`ros:lyrical`) | `/chatter` `NONE`, `certain`, `!shm-ipc-namespace-split` with `shm-port-collision` (both nodes on port 7000 on Jazzy/Lyrical, the same pid-based port on Humble), the listener receives nothing; from the talker's IPC namespace with different numbers: `shm-reader-port-not-visible`, no collision (Jazzy, Lyrical; Humble has no such placement to tell). `hostnet_shm` / `hostnet_noipc_shm` unchanged (`SHM`). `colcon test`: Jazzy 424 tests, Lyrical 416, Humble 409, 0 failures ([#101](https://github.com/atinfinity/fastdds_transport_viz/issues/101)) | `scripts/integration_test.sh hostnet_split_shm`, `hostnet_split_shm_visible`, `web/sample/shm_split.json` |
| 2026-09-13 | `--stats` with the tool in a third IPC namespace: talker and listener with `FASTDDS_STATISTICS` on the host network, an IPC namespace each, the tool in `hostnet`; unit suites on the three distributions | arm64 | 2.6 (`ros:humble`), 2.14.6 (`ros:jazzy`), 3.6 (`ros:lyrical`) | the writer's statistics arrive (before: none, `!stats-not-enabled-on-writer`); `/chatter` stays `NONE`, `!shm-ipc-namespace-split`, with `measured-shm-traffic` and no delivery (Jazzy, Lyrical; Humble has no statistics module). `hostnet_split_shm`, `hostnet_shm`, `stats_multi_container` unchanged. `colcon test`: Jazzy 431 tests, Lyrical 420, Humble 413, 0 failures ([#106](https://github.com/atinfinity/fastdds_transport_viz/issues/106)) | `scripts/integration_test.sh hostnet_split_stats` |
| 2026-09-05 | full launch test suite on Fast DDS 3.x (Kilted 3.2.4, Rolling 3.6.2) | x86_64 | 3.2.4 / 3.6.2 | all pass; Rolling: demo nodes publish `example_interfaces/msg/String`, Discovery Server relays every endpoint to plain clients | `ROS_DISTRO=kilted docker compose build dev` + `colcon test` |
| 2026-09-10 | full test suite on ROS 2 Lyrical Luth (Ubuntu 26.04) | x86_64 | 3.6 (`ros:lyrical`) | 341 tests, 0 failures, 49 skipped; demo nodes publish `example_interfaces/msg/String` and the Discovery Server relays every endpoint to plain clients, as on Rolling; the vendored statistics types generated from 3.2.4 build unchanged | `ROS_DISTRO=lyrical docker compose build dev` + `colcon test` |
| 2026-09-13 | full test suite on `rmw_fastrtps_dynamic_cpp`, ROS 2 Jazzy | arm64 | 2.14.6 (`ros:jazzy`) | 370 tests, 0 failures, 51 skipped (cppcheck per-file placeholders only); every verdict as on `rmw_fastrtps_cpp`. The former "not verified yet" warning line was 143 columns wide and broke `test_watch_tty.py`'s 60-column check; gone with the warning | `RMW_IMPLEMENTATION=rmw_fastrtps_dynamic_cpp docker compose run --rm dev bash` + `colcon test` |
| 2026-09-13 | full test suite on `rmw_fastrtps_dynamic_cpp`, ROS 2 Lyrical | arm64 | 3.6 (`ros:lyrical`) | 363 tests, 0 failures, 52 skipped (cppcheck placeholders and the Fast DDS 3.6 Discovery Server skip, as on `rmw_fastrtps_cpp`) | `RMW_IMPLEMENTATION=rmw_fastrtps_dynamic_cpp ROS_DISTRO=lyrical docker compose run --rm dev bash` + `colcon test` |
| 2026-09-13 | all five multi-container scenarios on `rmw_fastrtps_dynamic_cpp` (Jazzy image) | arm64 | 2.14.6 | `UDPv4` / measured `UDPv4` / `SHM` / `TCPv4` measured `TCPv4` / `UDPv6`, reason codes identical to the `rmw_fastrtps_cpp` runs | `RMW_IMPLEMENTATION=rmw_fastrtps_dynamic_cpp scripts/integration_test.sh all` |
| 2026-09-06 | two physical hosts on one Wi-Fi LAN: x86_64 Ubuntu 24.04 (Docker `hostnet`) ↔ Jetson Orin NX, JetPack 6 / Ubuntu 22.04 arm64 (Docker `hostnet`, Jazzy image), plain multicast discovery, `--stats` on both nodes, tool on the x86 host | x86_64 + arm64 | 2.14.6 both | both directions: `UDPv4`, `different-host`, `certain`, measured `UDPv4` (`measured=UDPv4 7pkt 1.06 kB` Jetson → x86, `8pkt 1.16 kB` x86 → Jetson), hosts `jetson-orin-nx01` / `ubuntu2404-desktop01` from `PHYSICAL_DATA`, `RATE` 24 B/s. Found and fixed: a reader on the tool's host is announced as `127.0.0.1`, so the remote writer's `RTPS_SENT` did not match (`delivered-without-measured-traffic`) | "Two physical hosts" below |
| 2026-09-13 | Easy Mode (`ROS2_EASY_MODE=127.0.0.1`), one host: two `hostnet` containers, and the launch test on a private domain | x86_64 | 3.6.2 (`ros:lyrical`), 3.2.4 (`ros:kilted`) | `SHM`, `same-host-guid`, `both-shm-locators`; every endpoint announces `SHM` + `TCPv4` unicast and no multicast (P2P); the auto-started server listens on 7400 + 250 × domain + 2. Found and fixed: the `fastdds discovery` CLI that Fast DDS runs for every participant printed on stdout and corrupted `--json` | `ROS_DISTRO=lyrical scripts/integration_test.sh easy_mode_shm`, `test_easy_mode.py` |
| 2026-09-13 | Easy Mode, two hosts: bridged containers with fixed addresses, `ROS2_EASY_MODE=<talker address>`, `--stats`, tool on the talker's host | x86_64 | 3.6.2 (`ros:lyrical`), 3.2.4 (`ros:kilted`) | `TCPv4`, `different-host`, `common-tcpv4-locator`, measured `TCPv4`; the remote endpoint shows its `TCPv4` locator only. From a third host without a node of the topic's type the tool (and `ros2 topic list`) never see `/chatter`: Fast DDS 3 relays an endpoint only once its type is resolved and the server-to-server lookup does not complete (see how-it-works.md, Easy Mode) | `ROS_DISTRO=lyrical scripts/integration_test.sh easy_mode_tcp` |
| 2026-09-13 | Easy Mode on two physical hosts over Wi-Fi: x86_64 Ubuntu 24.04 (Docker `hostnet`, master, `ROS2_EASY_MODE=192.168.1.8`) ↔ Jetson Orin NX, Ubuntu 24.04 arm64 (`docker run --network host --ipc host`, same variable), Lyrical image on both, `--stats` on both nodes, both directions | x86_64 + arm64 | 3.6.2 both | both directions: `TCPv4`, `different-host`, `common-tcpv4-locator`, measured `TCPv4` (23–49 packets per 8 s observation), hosts `ubuntu2404-desktop01` / `jetson-orin-nx-16gb` from `PHYSICAL_DATA`, `LATENCY` ≈ 94 ms with the clock offset (`latency-clock-skew-suspected` in one run); the tool saw the pair from the master host and from the Jetson alike, each next to one of the nodes; the remote endpoint shows its `TCPv4` locator only, no multicast anywhere | "Two physical hosts" below |
| 2026-09-05 | two physical hosts on one Wi-Fi LAN: x86_64 Ubuntu (Docker `hostnet`) ↔ macOS arm64 (native RoboStack Jazzy), Discovery Server on the x86 host | x86_64 + arm64 | 2.14.6 both | `UDPv4`, `different-host`, `common-udpv4-locator` observed (writer `host:010f0956`, reader on `ubuntu2404-desktop01`); no `--stats` measurement: the Mac's Fast DDS `sendto()` intermittently fails with `EHOSTUNREACH` while plain UDP from the Mac works ([#15](https://github.com/atinfinity/fastdds_transport_viz/issues/15)) | see "Two physical hosts" |
| 2026-09-14 | same host id, separate IPC namespaces, data-sharing: `bounded_pub` and `bounded_sub` with `datasharing_auto.xml` on the host network in an IPC namespace each, the tool in `hostnet`; the same with `FASTDDS_BUILTIN_TRANSPORTS=UDPv4` on both nodes and the tool in the publisher's IPC namespace; unit suites on the three distributions | arm64 | 2.6 (`ros:humble`), 2.14.6 (`ros:jazzy`), 3.6 (`ros:lyrical`) | `/bounded` `NONE`, `certain`, `!shm-ipc-namespace-split` with `datasharing-qos-enabled-both`, `both-shm-locators` and `shm-port-collision` (all three distributions; before the fix `DATA_SHARING` / `likely` while the subscriber received nothing); UDPv4 only: `datasharing-reader-segment-not-visible` and no SHM reason, the publisher's IPC namespace holds its history but not the subscriber's notification segment (Jazzy, Lyrical; Humble has no `FASTDDS_BUILTIN_TRANSPORTS`). `hostnet_split_shm` (Jazzy, Lyrical) and `hostnet_shm` (Jazzy) unchanged. `colcon test`: Jazzy 433 tests, Lyrical 422, Humble 415, 0 failures (Humble after a line its older uncrustify formats differently was rewritten) ([#110](https://github.com/atinfinity/fastdds_transport_viz/issues/110)) | `scripts/integration_test.sh hostnet_split_datasharing`, `hostnet_split_datasharing_udp` |
| 2026-09-14 | same host id, separate IPC namespaces with `--stats`: `talker` and `listener` with `FASTDDS_STATISTICS` on the host network in an IPC namespace each, the tool in a third; unit suites on the three distributions | arm64 | 2.6 (`ros:humble`), 2.14.6 (`ros:jazzy`), 3.6 (`ros:lyrical`) | `/chatter` `NONE`, `certain`, warnings only `shm-ipc-namespace-split` (no `shm-ipc-namespace-split-but-non-shm-traffic`; the measured traffic is SHM only) on Jazzy and Lyrical. `colcon test`: Jazzy 434 tests, Lyrical 423, Humble 416, 0 failures ([#111](https://github.com/atinfinity/fastdds_transport_viz/issues/111)) | `scripts/integration_test.sh hostnet_split_stats` |

## Documentation site

`mkdocs.yml` builds this documentation with Material for MkDocs and `mkdocs-static-i18n`
(English at `/`, Japanese at `/ja/` from the `*.ja.md` files, English fallback for
untranslated pages). `.github/workflows/docs.yml` runs `mkdocs build --strict` on pull
requests and deploys to GitHub Pages on pushes to `main`, in both cases only when `docs/**`,
`README*.md`, `mkdocs.yml` or the workflow itself changed (the aggregate `Docs result`
check passes otherwise). Locally:

```
pip install -r docs/requirements.txt
mkdocs serve          # http://127.0.0.1:8000/
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
getting-started, how-it-works, statistics, data-sharing, web-viewer); English is the
source of truth and tool output stays English. When one of those English files changes,
update its `.ja.md` and the "as of" date in its header. The developer guide (this file
and architecture.md) has no translation.

## Layout

See [architecture.md](architecture.md): components, the flow of one run, the data model,
the Fast DDS 2.14 / 3.x compatibility layer, the repository layout and extension points.

## Roadmap

As of 2026-09-13. The [issue tracker](https://github.com/atinfinity/fastdds_transport_viz/issues)
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
- `RATE` column (payload throughput) and the shared-memory line (`/dev/shm` capacity,
  Fast DDS files, stale files, IPC-namespace visibility)

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

Open, by priority (labels `priority/1-high` … `priority/3-low` on the issues):

`priority/1-high`: none open.

`priority/2-medium`:

- Distribution: bloom release for Jazzy/Humble/Lyrical — [#50](https://github.com/atinfinity/fastdds_transport_viz/issues/50)
- Verification on a large real system (Nav2 / Autoware scale) — [#74](https://github.com/atinfinity/fastdds_transport_viz/issues/74)
- `--advise`: what to change to get the intended transport — [#76](https://github.com/atinfinity/fastdds_transport_viz/issues/76)
- `transport_viz diff`: compare two `--json` snapshots — [#77](https://github.com/atinfinity/fastdds_transport_viz/issues/77)

`priority/3-low`:

- DDS Security (SROS2) — [#49](https://github.com/atinfinity/fastdds_transport_viz/issues/49)
- Ready-to-run Docker image on GHCR — [#78](https://github.com/atinfinity/fastdds_transport_viz/issues/78)
- Dependabot for GitHub Actions, weekly scheduled run for Rolling — [#79](https://github.com/atinfinity/fastdds_transport_viz/issues/79)
- Coverage report and badge from `scripts/coverage.sh` in CI — [#80](https://github.com/atinfinity/fastdds_transport_viz/issues/80)
- Web viewer: browser-level test of rendering and filters — [#81](https://github.com/atinfinity/fastdds_transport_viz/issues/81)
- Record and replay `--watch` frames with a timeline in the web viewer — [#82](https://github.com/atinfinity/fastdds_transport_viz/issues/82)
- Metrics export: Prometheus endpoint in `transport_viz_web`, CSV output — [#83](https://github.com/atinfinity/fastdds_transport_viz/issues/83)
- Group service and action endpoints under `--all` — [#84](https://github.com/atinfinity/fastdds_transport_viz/issues/84)
- Type mismatches on the same topic (type name / type hash) — [#85](https://github.com/atinfinity/fastdds_transport_viz/issues/85)
- Web viewer: Discovery Servers and the clients they serve — [#86](https://github.com/atinfinity/fastdds_transport_viz/issues/86)
- `QUALITY_DECLARATION.md` (REP 2004) for both packages — [#87](https://github.com/atinfinity/fastdds_transport_viz/issues/87)
- `participants_with_stats` lists the tool's own participants — [#113](https://github.com/atinfinity/fastdds_transport_viz/issues/113)
