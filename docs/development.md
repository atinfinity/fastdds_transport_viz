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

Two containers (separate network and IPC namespaces ⇒ different host ids ⇒ UDPv4),
run from the Docker host:

```
scripts/integration_test.sh
```

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

## Continuous integration

`.github/workflows/ci.yml` runs the same steps on every push to `main` and every pull
request inside a `ros:jazzy` container: `rosdep install`, `colcon build`, `colcon test`.
Test result XML files and launch logs are uploaded as a workflow artifact.

## Layout

```
src/fastdds_transport_viz/
  include/fastdds_transport_viz/   model.hpp, decision.hpp, discovery_observer.hpp,
                                   ros_graph_resolver.hpp, stats_observer.hpp, render.hpp
  src/                             implementation + main.cpp
  src/test_nodes/                  verification nodes
  config/                          statistics.xml, datasharing_auto.xml
  third_party/fastdds_statistics_types/   vendored generated statistics types
  test/                            gtest + launch tests
```

## Roadmap

- [x] M1 discovery-based prediction, table/JSON, tests, Docker env
- [x] M2 `--stats`: measured per-locator traffic via the Fast DDS statistics module,
      host names, mismatch detection
- [x] M3 data-sharing confidence from `HISTORY_LATENCY` + traffic
- [ ] M4 multi-host verification on x86_64 (arm64 verified)
- [ ] M5 richer `--watch`
- [ ] Other front-ends on top of `--json`
