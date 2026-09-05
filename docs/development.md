# Development, verification and tests

## Docker environment

The repository ships a `compose.yaml` and `docker/Dockerfile` based on `ros:jazzy`
(multi-arch: x86_64 and arm64). `ROS_DISTRO=kilted docker compose build` (or `rolling`)
builds the same environment on Fast DDS 3.x; the image is tagged
`fastdds_transport_viz:<distro>` and every `docker compose` command below then needs
the same `ROS_DISTRO` in the environment.

```
docker compose build
docker compose run --rm dev            # shell in the dev container, repo mounted at /ws
colcon build --symlink-install
source install/setup.bash
```

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
| `multi_container` (default) | `talker`, `listener`: separate network and IPC namespaces ⇒ different host ids | `UDPv4`, `different-host` |
| `stats_multi_container` | `talker_stats`, `listener_stats`: as above with `FASTDDS_STATISTICS` | measured `UDPv4`, two different `PHYSICAL_DATA` host names |
| `hostnet_shm` | two `hostnet` containers: `network_mode: host` + `ipc: host` ⇒ same host id, shared `/dev/shm` | `SHM`, `same-host-guid` |
| `large_data_tcp` | `talker_large_data`, `listener_large_data`: bridged, `FASTDDS_BUILTIN_TRANSPORTS=LARGE_DATA` + statistics | `TCPv4`, `common-tcpv4-locator`, measured `TCPv4` |
| `udpv6_multi_container` | `talker_udpv6`, `listener_udpv6`: bridged (the project network has IPv6), `DEFAULTv6` | `UDPv6`, `common-udpv6-locator` |
| `all` | the five above in sequence | |

Output goes to `/tmp/transport_viz_<scenario>.json`.

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

Things learned while trying this on a Wi-Fi LAN
([#15](https://github.com/atinfinity/fastdds_transport_viz/issues/15), still open):

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
- On the x86_64 ↔ macOS (native RoboStack) pair, **Discovery Server** does not help
  either: a root `tcpdump` on the Mac shows its Fast DDS sending for 1.6 s after start
  and then never reacting to inbound RTPS again, while raw UDP (including IP fragments)
  flows both ways and Docker Desktop's bridge, macOS's 9216-byte UDP datagram limit and
  `maxMessageSize` were ruled out. Details and next steps in
  [#15](https://github.com/atinfinity/fastdds_transport_viz/issues/15).

## Verification nodes

Shipped with the package for reproducing the scenarios in the docs:

| Executable | Purpose |
|---|---|
| `bounded_pub` / `bounded_sub` | `std_msgs/Int32`, data-sharing eligible |
| `unbounded_pub` | `std_msgs/String`, never data-sharing |
| `large_array_pub --size-kb N` / `large_array_sub` | large `std_msgs/UInt8MultiArray` samples |

## Tests

```
colcon test && colcon test-result --verbose
```

- `test_decision`: gtest over the pure decision logic, the statistics overlay and name
  demangling.
- `test_same_host_shm.py` / `test_same_host_udp.py`: launch_testing against real demo
  nodes (SHM, and UDPv4 fallback via `FASTDDS_BUILTIN_TRANSPORTS=UDPv4`).
- `test_stats.py`: demo nodes with `FASTDDS_STATISTICS`; asserts measured SHM / UDPv4 and
  host names.
- `test_udpv6.py` (`FASTDDS_BUILTIN_TRANSPORTS=UDPv6`, skipped without an IPv6
  interface), `test_localhost_range.py` (`ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST`),
  `test_discovery_server.py` (`fast-discovery-server`, SUPER_CLIENT vs plain client),
  `test_large_data_same_host.py` (`LARGE_DATA`: TCPv4 announced, SHM chosen),
  `test_large_shm.py` (2 MB samples measured on SHM), `test_datasharing_stats.py`
  (`bounded_pub`/`bounded_sub` with `datasharing_auto_stats.xml`: `DATA_SHARING` becomes
  `certain` through `HISTORY_LATENCY` + `DATA_COUNT`).
- `test_json_schema` / `test_json_schema_live.py`: sample and live `--json` output against
  `schema/transport_viz.schema.json`.
- `test_web_serve` (pytest, fake `transport_viz`) / `test_web_live.py` (real one): the live
  server's SSE stream, `/latest.json` and shutdown behaviour.
- `ros2transport/test/test_cli.py` (pytest, fake `transport_viz`): argument translation of
  `ros2 transport list`, parity with the binary's `--help`, `codes`, missing binary.
  `test_list_live.py`: `ros2 transport list --json` against real demo nodes.

## Continuous integration

`.github/workflows/ci.yml` runs `rosdep install`, `colcon build`, `colcon test` inside
`ros:jazzy`, `ros:kilted` and `ros:rolling` containers for every pull request that touches
code (docs-only changes skip the job); Rolling may break with upstream changes and does
not block (`continue-on-error`). Test result XML files and launch logs are uploaded as a
workflow artifact per distribution. `main` is protected: pull requests merge only when
the Jazzy and Kilted jobs and the `mkdocs build --strict` job of the Docs workflow have
passed or were skipped for a change that does not concern them.

A second job, `integration`, runs `scripts/integration_test.sh all` on the x86_64 runner
VM for the merge commit on `main` and on `workflow_dispatch` (not for pull requests, to
keep PR CI short); the `transport_viz` JSON of each scenario is uploaded as an artifact.
The matrix itself does not run again on `main`: each change is built once, in its pull
request.

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
| 2026-09-05 | `ROS_DISCOVERY_SERVER` (fast-discovery-server on 127.0.0.1:11811) | x86_64 | 2.14.6 | `SHM` pair seen as SUPER_CLIENT; a plain CLIENT does not see `/chatter` | `test_discovery_server.py` |
| 2026-09-05 | `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` | x86_64 | 2.14.6 | `SHM`, nodes announce `127.0.0.1` only; `OFF` sees nothing (warning printed) | `test_localhost_range.py` |
| 2026-09-05 | 2 MB samples over SHM, `--stats` | x86_64 | 2.14.6 | `SHM`, measured `SHM`, bytes consistent with the sample size | `test_large_shm.py` |
| 2026-09-05 | full launch test suite on Fast DDS 3.x (Kilted 3.2.4, Rolling 3.6.2) | x86_64 | 3.2.4 / 3.6.2 | all pass; Rolling: demo nodes publish `example_interfaces/msg/String`, Discovery Server relays every endpoint to plain clients | `ROS_DISTRO=kilted docker compose build dev` + `colcon test` |
| 2026-09-05 | two physical hosts on one Wi-Fi LAN: x86_64 Ubuntu (Docker `hostnet`) ↔ macOS arm64 (native RoboStack Jazzy); multicast, static peers, Discovery Server | x86_64 + arm64 | 2.14.6 both | not established: the Mac's Fast DDS stops sending 1.6 s after start (capture), network ruled out ([#15](https://github.com/atinfinity/fastdds_transport_viz/issues/15)) | see "Two physical hosts" |

## Documentation site

`mkdocs.yml` builds this documentation with Material for MkDocs and `mkdocs-static-i18n`
(English at `/`, Japanese at `/ja/` from the `*.ja.md` files, English fallback for
untranslated pages). `.github/workflows/docs.yml` runs `mkdocs build --strict` on every
pull request and deploys to GitHub Pages on pushes to `main`. Locally:

```
pip install -r docs/requirements.txt
mkdocs serve          # http://127.0.0.1:8000/
```

## Japanese documentation

`README.ja.md` and `docs/*.ja.md` mirror the English user documentation (README,
how-it-works, statistics, data-sharing, web-viewer); English is the source of truth and
tool output stays English. When one of those English files changes, update its `.ja.md`
and the "as of" date in its header. This file has no translation.

## Layout

Two packages: `fastdds_transport_viz` (C++, all Fast DDS logic, the `transport_viz`
binary and `transport_viz_web`) and `ros2transport` (Python ros2cli extension that
execs the binary: `ros2 transport list`, `ros2 transport codes`).

```
src/ros2transport/
  ros2transport/api/               binary lookup, option mirroring, exec
  ros2transport/command/, verb/    ros2cli entry points (transport; list, codes)
  test/                            pytest with a fake binary + launch test
src/fastdds_transport_viz/
  include/fastdds_transport_viz/   model.hpp, decision.hpp, discovery_observer.hpp,
                                   ros_graph_resolver.hpp, stats_observer.hpp, render.hpp
  src/                             implementation + main.cpp
  src/test_nodes/                  verification nodes
  config/                          statistics.xml, datasharing_auto.xml, datasharing_auto_stats.xml,
                                   unicast_discovery.xml
web/                               static viewer (index.html, app.js), serve.py (transport_viz_web), sample/
schema/                            JSON Schema for --json output
  third_party/fastdds_statistics_types/     vendored generated statistics types (Fast DDS 2.14)
  third_party/fastdds_statistics_types_v3/  same for Fast DDS 3.x
  include/.../fastdds_compat.hpp            2.14 / 3.x API differences
  test/                            gtest + launch tests
```

## Roadmap

As of 2026-09-05. The [issue tracker](https://github.com/atinfinity/fastdds_transport_viz/issues)
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
- Japanese README and user docs — [#8](https://github.com/atinfinity/fastdds_transport_viz/issues/8)

Open:

- Two physical hosts on a Wi-Fi LAN —
  [#15](https://github.com/atinfinity/fastdds_transport_viz/issues/15)
