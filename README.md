# fastdds_transport_viz

English | [日本語](README.ja.md)

[![CI](https://github.com/atinfinity/fastdds_transport_viz/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/atinfinity/fastdds_transport_viz/actions/workflows/ci.yml)

Shows **which Fast DDS transport each ROS 2 topic is communicated over** — UDPv4,
UDPv6, TCP, shared memory (SHM) or zero-copy data-sharing — **and why**.

Targets: ROS 2 Jazzy (Fast DDS 2.14) and Kilted / Rolling (Fast DDS 3.x) with `rmw_fastrtps_cpp`.

```
$ ros2 transport list -v --stats
TOPIC     TYPE                 PUBS  SUBS  TRANSPORT         REASON
/chatter  std_msgs/msg/String  1     2     SHM x1, UDPv4 x1  same-host-guid,...
    /talker@myhost(136) -> /listener@myhost(135)      SHM    measured=SHM 47pkt    same-host-guid,datasharing-disabled-writer,both-shm-locators,measured-shm-traffic
    /talker@myhost(136) -> /listener_udp@myhost(134)  UDPv4  measured=UDPv4 49pkt  same-host-guid,datasharing-disabled-writer,reader-no-shm-locator,common-udpv4-locator,measured-udpv4-traffic

statistics: 63 samples from 3 participant(s)
```

- The **prediction** comes from Fast DDS discovery data (announced locators and QoS) and
  needs nothing from the observed nodes.
- With `--stats`, the **measurement** comes from the Fast DDS statistics module and shows
  the transport that actually carried packets.
- Every verdict carries reason codes; `--explain` describes them.

## Quick start

```
docker compose build
docker compose run --rm dev
colcon build --symlink-install && source install/setup.bash

ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 transport list -v --explain
```

Run the tool in the same environment (env vars, XML profile, network/IPC namespace) as
the nodes you observe.

## Usage

```
ros2 transport list [--domain N] [--timeout S] [--quiet S] [--topic REGEX] [--node REGEX]
                    [--all] [-v] [--explain] [--stats] [--json] [--color auto|always|never]
                    [--watch [--interval S]]
ros2 transport codes
```

`ros2 transport` is a thin ros2cli extension (package `ros2transport`) that runs the
`transport_viz` binary of `fastdds_transport_viz`; the binary can also be run directly as
`ros2 run fastdds_transport_viz transport_viz`, with the same options plus `--list-codes`.

| Option | Effect |
|---|---|
| `-v` | expand writer → reader pairs under each topic |
| `--explain` | append a legend for the reason codes used |
| `--stats` | also show measured transports (observed nodes need `FASTDDS_STATISTICS`, see [docs/statistics.md](docs/statistics.md)) |
| `--json` | machine-readable output (`schema_version: 1`, see `schema/`); open it in the [web viewer](docs/web-viewer.md) |
| `--topic REGEX` | only topics whose name matches |
| `--node REGEX` | only pairs involving a node whose full name matches (its unpaired endpoints stay visible) |
| `--all` | include services/actions and non-ROS DDS topics |
| `--watch` | re-render every `--interval` seconds, highlighting added/changed/removed pairs; keys `q p v e a` (with `--json`: JSON Lines with a `changes` object) |
| `--color` | ANSI colors for transports and warnings (`auto` = only on a terminal) |

## Documentation

- [How it works](docs/how-it-works.md) — decision rules, reason codes, hosts, where to run it, watch mode
- [Measured transports (`--stats`)](docs/statistics.md) — statistics topics, enabling them, the 10-instance pitfall
- [Data-sharing (zero-copy)](docs/data-sharing.md) — why ROS 2 topics show `SHM` by default and how to enable data-sharing
- [Web viewer](docs/web-viewer.md) — graph/table view of `--json` output in the browser, live mode (`transport_viz_web`), JSON schema
- [Development, verification and tests](docs/development.md) — Docker environment, packages, verification nodes, multi-container scenarios, tests, verification results, roadmap

## License

Apache-2.0
