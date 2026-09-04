# fastdds_transport_viz

Shows **which Fast DDS transport each ROS 2 topic is communicated over** — UDPv4,
UDPv6, TCP, shared memory (SHM) or zero-copy data-sharing — **and why**.

Target: ROS 2 Jazzy with `rmw_fastrtps_cpp` (Fast DDS 2.14).

```
$ ros2 run fastdds_transport_viz transport_viz -v --explain
TOPIC     TYPE                 PUBS  SUBS  TRANSPORT           REASON
/chatter  std_msgs/msg/String  1     2     SHM x1, UDPv4 x1    same-host-guid,datasharing-disabled-writer,both-shm-locators,reader-no-shm-locator,common-udpv4-locator
    /talker@local -> /listener@local       SHM    same-host-guid,datasharing-disabled-writer,both-shm-locators
    /talker@local -> /listener_udp@local   UDPv4  same-host-guid,datasharing-disabled-writer,reader-no-shm-locator,common-udpv4-locator

Reason codes:
  both-shm-locators
      Both endpoints announce a SHM locator. On the same host Fast DDS then uses the shared
      memory transport exclusively for user data (discovery still goes over UDP).
  ...
```

## How it works

`ros2 topic info -v` cannot tell you the transport: the rmw layer exposes no locator
information. This tool instead creates its own Fast DDS `DomainParticipant` and listens
to endpoint discovery, which carries every remote writer's/reader's **announced
locators** (`UDPv4`, `SHM`, ...) and QoS. It then applies the same rules Fast DDS 2.14
uses to select a transport for each writer → reader pair:

1. Same host? (first 4 bytes of the GUID prefix are equal)
2. Both announce data-sharing (zero-copy) with intersecting domain ids → `DATA_SHARING`
3. Same host and both announce a SHM locator → `SHM`
4. Otherwise the first network locator kind the reader announces that the writer also
   speaks → `UDPv4` / `UDPv6` / `TCPv4` / `TCPv6`
5. Nothing in common → `NONE`

Every decision is annotated with machine-readable reason codes
(`transport_viz --list-codes` prints all of them with descriptions). A `?` after a
transport means confidence `likely` rather than `certain`.

ROS node names are resolved through the rclcpp graph API, so the tool registers a
hidden node `_transport_viz_<pid>` (excluded from its own output).

## Usage

```
transport_viz [--domain N] [--timeout S] [--quiet S] [--topic REGEX] [--all]
              [-v] [--explain] [--json] [--watch [--interval S]] [--list-codes]
```

- `--json` emits a document with `schema_version: 1` — the integration point for other
  front-ends. No compatibility promise while the schema version stays at 1.
- `--all` includes services/actions (`rq/`, `rr/`) and non-ROS DDS topics.
- `--watch` re-renders every `--interval` seconds.

**Run the tool in the same environment as the nodes you observe**: same
`FASTDDS_BUILTIN_TRANSPORTS` / `FASTRTPS_DEFAULT_PROFILES_FILE` /
`ROS_DISCOVERY_SERVER` / `ROS_AUTOMATIC_DISCOVERY_RANGE`, same network and IPC
namespace (for containers: `network_mode`/`ipc` settings). The tool never modifies these.

## Build & try it (Docker)

```
docker compose build
docker compose run --rm dev            # shell in the dev container, repo mounted at /ws
colcon build --symlink-install
source install/setup.bash

ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 run fastdds_transport_viz transport_viz -v --explain      # /chatter -> SHM

FASTDDS_BUILTIN_TRANSPORTS=UDPv4 ros2 run demo_nodes_cpp listener &
ros2 run fastdds_transport_viz transport_viz -v                # second pair -> UDPv4, reader-no-shm-locator
```

Two containers (different host ids ⇒ UDPv4), run from the host:

```
scripts/integration_test.sh
```

Verification nodes shipped with the package: `bounded_pub`/`bounded_sub`
(`std_msgs/Int32`, data-sharing eligible), `unbounded_pub` (`std_msgs/String`),
`large_array_pub --size-kb N`.

### Data-sharing (zero-copy)

`rmw_fastrtps_cpp` announces data-sharing `OFF` for every endpoint by default, so ROS 2
topics normally show `SHM`, never `DATA_SHARING`. Fast DDS resolves `AUTO` *before*
discovery, so what the tool sees is the effective setting. To enable it for bounded
types (e.g. `std_msgs/Int32`):

```
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/datasharing_auto.xml
export RMW_FASTRTPS_USE_QOS_FROM_XML=1
ros2 run fastdds_transport_viz bounded_pub &
ros2 run fastdds_transport_viz bounded_sub &
ros2 run fastdds_transport_viz transport_viz -v      # /bounded -> DATA_SHARING? (likely)
```

The `?` stays until `--stats` (M2) can confirm that no RTPS traffic flows for the pair.

## Tests

```
colcon test && colcon test-result --verbose
```

- `test_decision`: gtest over the pure decision logic and name demangling.
- `test_same_host_shm.py` / `test_same_host_udp.py`: launch_testing against real demo nodes.

## Roadmap

- [x] M1 discovery-based prediction, table/JSON, tests, Docker env
- [ ] M2 `--stats`: measured per-locator traffic via the Fast DDS statistics module
      (`FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;PHYSICAL_DATA_TOPIC"` on the
      observed nodes), host names, `MISMATCH` detection
- [ ] M3 data-sharing confidence upgrade from traffic
- [ ] M4 multi-host verification (x86_64 / arm64)
- [ ] M5 richer `--watch`

## License

Apache-2.0
