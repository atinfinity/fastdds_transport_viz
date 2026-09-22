# fastdds_transport_viz

Shows **which Fast DDS transport each ROS 2 topic is communicated over** — UDPv4,
UDPv6, TCP, shared memory (SHM) or zero-copy data-sharing — **and why**.

All distros below use `rmw_fastrtps_cpp` (`rmw_fastrtps_dynamic_cpp` works the same):

| ROS 2 distro | Fast DDS | Notes |
|---|---|---|
| Humble | 2.6 | Prediction only — the binary has no statistics module |
| Jazzy | 2.14 | Prediction + `--stats` measurement |
| Lyrical | 3.6 | Prediction + `--stats` measurement |
| Rolling | 3.x (head) | Tracks Fast DDS main; best-effort in CI, not a required check |

Source and issues: [github.com/atinfinity/fastdds_transport_viz](https://github.com/atinfinity/fastdds_transport_viz).

```
$ ros2 transport list -v --stats --topic '^/(chatter|bounded)$'
TOPIC     TYPE                 PUBS  SUBS  TRANSPORT         LATENCY  HZ  LOSS  REASON
/bounded  std_msgs/msg/Int32   1     1     DATA_SHARING x1   108 µs       0     same-host-guid,datasharing-qos-enabled-both,datasharing-domain-ids-match,datasharing-confirmed-no-data-submessages
    /bounded_pub@ba8b86aa803d(688) -> /bounded_sub@ba8b86aa803d(690)  DATA_SHARING  108 µs (max 155 µs)  10.0  0  measured=SHM (unmeasured, delivered)  same-host-guid,datasharing-qos-enabled-both,datasharing-domain-ids-match,datasharing-confirmed-no-data-submessages
/chatter  std_msgs/msg/String  1     2     UDPv4 x1, SHM x1  217 µs       0     same-host-guid,datasharing-disabled-writer,reader-no-shm-locator,common-udpv4-locator,measured-udpv4-traffic,both-shm-locators,measured-shm-traffic
    /talker@ba8b86aa803d(691) -> /listener_udp@ba8b86aa803d(689)  UDPv4  185 µs (max 239 µs)  1.0  0  measured=UDPv4 9pkt 1.19 kB  same-host-guid,datasharing-disabled-writer,reader-no-shm-locator,common-udpv4-locator,measured-udpv4-traffic
    /talker@ba8b86aa803d(691) -> /listener@ba8b86aa803d(692)      SHM    217 µs (max 254 µs)  1.0  0  measured=SHM 10pkt 1.31 kB   same-host-guid,datasharing-disabled-writer,both-shm-locators,measured-shm-traffic

statistics: 448 samples from 5 participant(s)

shared memory: /dev/shm 37.4 MB used of 16.7 GB (16.6 GB free) | Fast DDS 3.90 MB in 6 segment(s), 11 port(s), 1 data-sharing history, 1 data-sharing notification(s)
```

The same capture on a terminal (`--color auto`, default when stdout is a terminal):

![colored table](images/example-table.svg)

The same run opened in the [web viewer](web-viewer.md) (table view):

![table view](images/web-viewer-table.jpg)

- The **prediction** comes from Fast DDS discovery data (announced locators and QoS) and
  needs nothing from the observed nodes.
- With `--stats`, the **measurement** comes from the Fast DDS statistics module and shows
  the transport that actually carried packets.
- Every verdict carries reason codes; `--explain` describes them.

## Features

- **Transport per pair, from discovery alone.** Every writer → reader pair gets a
  predicted transport (`UDPv4`, `UDPv6`, `TCPv4`/`TCPv6`, `SHM`, `DATA_SHARING`) with
  machine-readable reason codes; the observed nodes need no change. Pairs whose QoS do
  not match (reliability, durability, deadline, liveliness, ownership, partition) are
  shown as `NONE` with the policy that breaks them.
- **Measurement with `--stats`.** The Fast DDS statistics module supplies the packets and
  bytes that actually flowed per locator, the write-to-notification
  latency (`LATENCY`), the delivered samples per second of each pair (`HZ`), lost and
  resent packets (`LOSS`), host names and process ids, and
  the proof of zero-copy data-sharing; a measurement that contradicts the
  prediction is flagged.
- **Several front-ends.** A table with colors, `--watch` (live terminal view that marks
  what changed), `--json` with a published schema, the `ros2 transport` command, and a
  web viewer (graph and table, live updates through `transport_viz_web` with a timeline of
  the frames received so far, and recordings made with `transport_viz_web --record`
  replayed on the same timeline). `transport_viz_web` also serves the latest document as
  Prometheus metrics on `/metrics`.
- **Focus.** `--topic` / `--node` regex filters, `--explain` for the codes in use,
  `--advise` for what to change to get past them, `ros2 transport codes` for all of them.
- **Shared memory of the environment.** Capacity of `/dev/shm`, the Fast DDS segments,
  ports and data-sharing histories in it, stale leftovers, and whether the observed nodes
  share it at all.
- **Verified on** Jazzy (Fast DDS 2.14) and Lyrical / Rolling (Fast DDS 3.x), x86_64 and
  arm64, with Discovery Server, `LARGE_DATA` (TCP), `UDPv6`, `LOCALHOST` discovery range,
  large SHM samples, zero-copy data-sharing, and two physical hosts (x86_64 ↔ Jetson Orin
  NX over Wi-Fi, prediction and measurement in both directions).

## Quick start

```
docker compose build
docker compose run --rm dev bash
colcon build --symlink-install && source build/$ROS_DISTRO/install/setup.bash

ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 transport list -v --explain
```

Run the tool in the same environment (env vars, XML profile, network/IPC namespace) as
the nodes you observe.

## Usage

```
ros2 transport list [--domain N] [--timeout S] [--quiet S] [--topic REGEX] [--node REGEX]
                    [--all] [-v] [--explain] [--locators] [--advise] [--stats] [--json | --csv]
                    [--color auto|always|never] [--watch [--interval S]]
ros2 transport diff BEFORE.json AFTER.json [--key node|guid] [--changes-only] [--json]
                    [--topic REGEX] [--node REGEX] [--all] [-v] [--explain] [--locators]
                    [--advise] [--color auto|always|never]
ros2 transport codes
```

`ros2 transport` is a thin ros2cli extension (package `ros2transport`) that runs the
`transport_viz` binary of `fastdds_transport_viz`; the binary can also be run directly as
`ros2 run fastdds_transport_viz transport_viz`, with the same options plus `--list-codes`
(and `transport_viz diff` for `ros2 transport diff`).

| Option | Effect |
|---|---|
| `-v` | expand writer → reader pairs under each topic |
| `--explain` | append a legend for the reason codes used |
| `--locators` | add a line under each pair with the locator the tool selected and the locators that actually carried packets (implies `-v`; ignored with `--json`, which always carries them) |
| `--advise` | add a `fix <code>: …` line under each pair for its reason codes that have a remedy, and the remedy under each code of the legend (implies `-v` and `--explain`; ignored with `--json`, which always carries them as `reason_code_remedies`) |
| `--stats` | also show measured transports, latency and loss; observed nodes need `FASTDDS_STATISTICS`, see [Measured transports](statistics.md) |
| `--json` | machine-readable output (`schema_version: 1`); open it in the [web viewer](web-viewer.md) |
| `--csv` | one CSV row per writer → reader pair (RFC 4180 quoting, LF line ends, a header line first; an empty cell where the JSON value is null, lists joined with `;`), for a spreadsheet or pandas; exclusive with `--json`, not accepted by `diff`. With `--watch` the header is printed once and each frame adds its rows, `observed_at` telling the frames apart |
| `--topic REGEX` | only topics whose name matches |
| `--node REGEX` | only pairs involving a node whose full name matches (its unpaired endpoints stay visible) |
| `--all` | include services/actions and non-ROS DDS topics; each service or action is one `SERVICE` / `ACTION` row per client-server pair rather than its raw `rq/` / `rr/` topics |
| `--watch` | re-render every `--interval` seconds, highlighting added/changed/removed pairs; keys `q p v e a l f` (with `--json`: JSON Lines with a `changes` object) |
| `--color` | ANSI colors for transports and warnings (`auto` = only on a terminal) |

`ros2 transport diff BEFORE.json AFTER.json` compares two saved `--json` documents (change
a profile or an environment variable, run again, see what moved) without observing
anything: the after snapshot is printed with the pairs that were added (`+`), changed (`~`)
or removed (`-`) marked as in `--watch`, or with `--json` as the after document plus a
`changes` object. Pairs are matched by node names (`--key node`, the default, so a restart
of the nodes is not a change) or by GUIDs (`--key guid`, what `--watch` does);
`--changes-only` keeps only the topics that moved. The exit status is 0 without changes, 1
with, 2 on an error, so it works in scripts. The web viewer compares two documents the same
way and highlights the result. Details in
[How it works](how-it-works.md#comparing-two-snapshots).

## Documentation

- [Getting started](getting-started.md) — build (native or Docker), first run, `--stats`, watch mode, web viewer, first checks
- [How it works](how-it-works.md) — decision rules, reason codes, hosts and addresses, where to run it, watch mode
- [Measured transports (`--stats`)](statistics.md) — statistics topics, enabling them, the 10-instance pitfall
- [Data-sharing (zero-copy)](data-sharing.md) — why ROS 2 topics show `SHM` by default and how to enable data-sharing
- [Web viewer](web-viewer.md) — graph view and a topic-grouped table of `--json` output in the browser, live mode (`transport_viz_web`) and its history, recording and replay (`--record`), Prometheus `/metrics`, JSON schema
- [Architecture](architecture.md) — components, the flow of one run, data model, Fast DDS 2.14/3.x layer, extension points
- [Development, verification and tests](development.md) — Docker environment, packages, verification nodes, multi-container scenarios, tests, scale harness, roadmap
- [Verification log](verification-log.md) — dated scale results and verification runs

## Limitations

- **Fast DDS RMWs only.** `rmw_fastrtps_cpp` and `rmw_fastrtps_dynamic_cpp` are supported
  (the launch test suite passes on both, see CI); nodes on CycloneDDS or Connext are not
  covered, and Fast DDS participants that are not ROS nodes appear only with `--all`
  (Discovery Servers, which have no endpoints, only in the `participants` section of
  `--json`, the `-v` footer and the web viewer).
  The tool refuses to start on another RMW (exit 1, naming it).
- **Linux only.** macOS has no `/dev/shm`, and Docker Desktop cannot observe nodes on the
  host.
- **Run it where the nodes run.** Same domain, same environment variables and XML profile,
  same network and IPC namespace. `ROS_AUTOMATIC_DISCOVERY_RANGE=OFF` hides everything.
- **Type hashes need ROS 2 Jazzy or later.** A writer and a reader whose type *names*
  differ are shown as `NONE` with `type-name-mismatch`, except on Fast DDS 3.x (Lyrical and
  later) when both announce an XTypes `TypeInformation` that agrees: Fast DDS then matches
  them regardless of the names, and the pair carries `type-names-differ-same-type`
  ([#213](https://github.com/atinfinity/fastdds_transport_viz/issues/213)). Two versions
  of the same message definition are told apart by the ROS 2 type hash (REP-2011), which
  only the rmw of Jazzy and later announces: on Humble such a pair looks healthy, and the
  subscription still receives nothing. Fast DDS's own, older type description cannot stand
  in for it — measured on 2026-09-21, no Humble endpoint announces a `TypeIdentifier`, a
  `TypeObject` or a `TypeInformation` either, because `rmw_fastrtps` registers no type in
  the `TypeObjectFactory` those are filled from (Fast DDS 2.14, and so Jazzy, announces
  them just as little; see `docs/verification-log.md`). On Humble, rebuild and reinstall every node
  against the same version of the message package, and, if a machine with Jazzy or later is
  available, run the same graph there, where the tool does report the mismatch.
- **A prediction is a model.** The verdicts encode Fast DDS's selection rules; some
  situations stay `likely` (marked `?`) until `--stats` confirms them. Measuring requires
  `FASTDDS_STATISTICS` on the observed nodes *before they start*, and the shipped profile
  when a node talks to more than 10 locators.
- **Statistics are per participant** (one per ROS node), so several topics between the
  same two nodes share one measurement.
- **Not a benchmark.** `LATENCY` is Fast DDS's own `HISTORY_LATENCY` statistic
  (write-to-notification between the two histories), sampled during a short observation; it
  does not replace a load test or an end-to-end measurement, and across hosts it includes
  the clock offset. `HZ` counts the samples that reached each reader
  ([#143](https://github.com/atinfinity/fastdds_transport_viz/issues/143)); there is no
  writer-side publish rate: `PUBLICATION_THROUGHPUT` looked like one but is not
  ([#137](https://github.com/atinfinity/fastdds_transport_viz/issues/137)).
- **DDS Security (SROS2) is not supported** and untested: the tool's participants carry no
  security configuration, so participants inside a secure enclave are not discovered.
- **Footprint.** The tool adds two participants of its own to the domain, three on Humble
  (`fastdds_transport_viz_names`, a participant without SHM that reads the node names); all
  are filtered from the output.
- **Split shared memory.** Nodes with the same host id in different IPC namespaces
  (`network_mode: host` without `ipc: host`) still select SHM or data-sharing between each
  other and lose every message. The pair is `NONE` with `shm-ipc-namespace-split` when the
  tool can tell: the two announce the same SHM port number (`shm-port-collision`, the usual
  case with one node per container), or the tool shares the IPC namespace of one of them
  (their SHM ports or data-sharing segments are only half there). Otherwise the pair stays
  `SHM` or `DATA_SHARING` and only `shm-not-visible` in the shared-memory line hints at it
  ([#101](https://github.com/atinfinity/fastdds_transport_viz/issues/101), [#110](https://github.com/atinfinity/fastdds_transport_viz/issues/110)).
- **Native-buffer companions.** On Lyrical and later `rmw_fastrtps_cpp` sends the samples
  of types with an unbounded `uint8[]` field over a companion topic `<topic>/_buf_cpu`. The
  tool adds the companion's counters to the parent pair (`buffer-companion-folded`) and shows
  the companion topic only with `--all`; a companion it cannot link to its parent stays
  visible with `buffer-companion-unmatched`, and its parent pair then shows no traffic of its
  own ([#119](https://github.com/atinfinity/fastdds_transport_viz/issues/119)).

## Quality declaration

Both packages claim [REP 2004](https://www.ros.org/reps/rep-2004.html) **Quality Level 3**,
with one exception: Linux only, so Windows 10 (a tier 1 platform of REP 2000) is not
supported. See the quality declarations of
[`fastdds_transport_viz`](https://github.com/atinfinity/fastdds_transport_viz/blob/main/src/fastdds_transport_viz/QUALITY_DECLARATION.md) and
[`ros2transport`](https://github.com/atinfinity/fastdds_transport_viz/blob/main/src/ros2transport/QUALITY_DECLARATION.md) for the version policy and
public API, change control, testing, dependencies and platforms, and
[SECURITY.md](https://github.com/atinfinity/fastdds_transport_viz/blob/main/SECURITY.md) to report a vulnerability.

## License

Apache-2.0
