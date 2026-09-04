# Development, verification and tests

## Docker environment

The repository ships a `compose.yaml` and `docker/Dockerfile` based on `ros:jazzy`
(multi-arch: x86_64 and arm64).

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
ros2 run fastdds_transport_viz transport_viz -v --explain      # /chatter -> SHM

FASTDDS_BUILTIN_TRANSPORTS=UDPv4 ros2 run demo_nodes_cpp listener &
ros2 run fastdds_transport_viz transport_viz -v                # second pair -> UDPv4, reader-no-shm-locator
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
| `all` | the three above in sequence | |

Output goes to `/tmp/transport_viz_<scenario>.json`.

## Two physical hosts

Run the `hostnet` service on each machine so the nodes use the real LAN interfaces (the
image has to be built on both, `docker compose build dev`; `colcon build` is only needed
where `transport_viz` runs):

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
`UDPv4`. If discovery does not happen (multicast filtered, e.g. on some Wi-Fi access
points), set `ROS_STATIC_PEERS=<other host IP>` on both sides.

## Verification nodes

Shipped with the package for reproducing the scenarios in the docs:

| Executable | Purpose |
|---|---|
| `bounded_pub` / `bounded_sub` | `std_msgs/Int32`, data-sharing eligible |
| `unbounded_pub` | `std_msgs/String`, never data-sharing |
| `large_array_pub --size-kb N` | large `std_msgs/UInt8MultiArray` samples |

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
- `test_json_schema` / `test_json_schema_live.py`: sample and live `--json` output against
  `schema/transport_viz.schema.json`.
- `test_web_serve` (pytest, fake `transport_viz`) / `test_web_live.py` (real one): the live
  server's SSE stream, `/latest.json` and shutdown behaviour.

## Continuous integration

`.github/workflows/ci.yml` runs the same steps on every push to `main` and every pull
request inside a `ros:jazzy` container: `rosdep install`, `colcon build`, `colcon test`.
Test result XML files and launch logs are uploaded as a workflow artifact.

A second job, `integration`, runs `scripts/integration_test.sh all` on the x86_64 runner
VM on pushes to `main` and on `workflow_dispatch` (not for pull requests, to keep PR CI
short); the `transport_viz` JSON of each scenario is uploaded as an artifact.

## Verification results

| Date | Scenario | Arch | Fast DDS | Result | Reproduce |
|---|---|---|---|---|---|
| 2026-09-04 | two bridged containers | arm64 | 2.14 (`ros:jazzy`) | `UDPv4`, `different-host` | `scripts/integration_test.sh multi_container` |
| 2026-09-05 | two bridged containers | x86_64 | 2.14.6 | `UDPv4`, `different-host` | `scripts/integration_test.sh multi_container` |
| 2026-09-05 | two bridged containers, `--stats` | x86_64 | 2.14.6 | measured `UDPv4`, host names differ (container ids) | `scripts/integration_test.sh stats_multi_container` |
| 2026-09-05 | two `hostnet` containers | x86_64 | 2.14.6 | `SHM`, `same-host-guid`, no `host-id-match-but-ip-differs` | `scripts/integration_test.sh hostnet_shm` |
| — | two physical hosts on one LAN | — | — | not yet run ([#1](https://github.com/atinfinity/fastdds_transport_viz/issues/1)) | see "Two physical hosts" |

## Layout

```
src/fastdds_transport_viz/
  include/fastdds_transport_viz/   model.hpp, decision.hpp, discovery_observer.hpp,
                                   ros_graph_resolver.hpp, stats_observer.hpp, render.hpp
  src/                             implementation + main.cpp
  src/test_nodes/                  verification nodes
  config/                          statistics.xml, datasharing_auto.xml
web/                               static viewer (index.html, app.js), serve.py (transport_viz_web), sample/
schema/                            JSON Schema for --json output
  third_party/fastdds_statistics_types/   vendored generated statistics types
  test/                            gtest + launch tests
```

## Roadmap

- [x] M1 discovery-based prediction, table/JSON, tests, Docker env
- [x] M2 `--stats`: measured per-locator traffic via the Fast DDS statistics module,
      host names, mismatch detection
- [x] M3 data-sharing confidence from `HISTORY_LATENCY` + traffic
- [ ] M4 multi-host verification: containers on x86_64 and arm64 verified, two physical hosts pending — [#1](https://github.com/atinfinity/fastdds_transport_viz/issues/1)
- [ ] M5 richer `--watch` — [#2](https://github.com/atinfinity/fastdds_transport_viz/issues/2)
- [ ] Web visualization on top of `--json` — [#3](https://github.com/atinfinity/fastdds_transport_viz/issues/3)
- [ ] Everything else: see the [issue tracker](https://github.com/atinfinity/fastdds_transport_viz/issues)
