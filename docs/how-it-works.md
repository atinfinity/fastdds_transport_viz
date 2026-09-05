# How it works

The tool builds against Fast DDS 2.14 (ROS 2 Jazzy) and 3.x (Kilted, Rolling); the
API differences live in `include/fastdds_transport_viz/fastdds_compat.hpp`. The decision
rules below are the same in both.

`ros2 topic info -v` cannot tell you the transport: the rmw layer exposes no locator
information. `transport_viz` therefore creates its own Fast DDS `DomainParticipant` and
listens to endpoint discovery, which carries every remote writer's/reader's **announced
locators** (`UDPv4`, `SHM`, ...) and QoS. It then applies the same rules Fast DDS 2.14 uses
to select a transport for each writer → reader pair.

## Decision rules

1. **Same host?** Fast DDS considers two participants to be on the same host when the
   first 4 bytes of their GUID prefixes are equal.
2. Same host and both endpoints announce data-sharing (zero-copy) with intersecting
   domain ids → `DATA_SHARING` (confidence `likely`, see [data-sharing.md](data-sharing.md)).
3. Same host and both announce a SHM locator → `SHM`. Fast DDS then uses shared memory
   exclusively for user data between those participants; discovery still goes over UDP.
4. Otherwise the first network locator kind the reader announces that the writer also
   speaks → `UDPv4` / `UDPv6` / `TCPv4` / `TCPv6`.
5. Nothing in common → `NONE`.

Topics with only publishers or only subscriptions are listed with `-` and the reason
`no-matching-reader` / `no-matching-writer`.

`--topic REGEX` keeps the topics whose name matches. `--node REGEX` keeps the pairs in
which the writer or the reader belongs to a node whose full name (`/ns/name`) matches,
together with that node's unpaired endpoints; the other side of a kept pair stays
visible even if it does not match. Both filters combine with AND. An invalid regex is
rejected at start-up (exit code 2).

## Reason codes

Every verdict carries machine-readable reason codes (`same-host-guid`,
`reader-no-shm-locator`, ...) and, where relevant, warnings prefixed with `!`.
`--explain` appends a legend for the codes used in the current output;
`transport_viz --list-codes` prints all of them. A `?` after a transport means confidence
`likely` rather than `certain`.

The decision logic lives in `src/fastdds_transport_viz/src/decision.cpp` as pure
functions with no DDS dependency, and is covered by `test/test_decision.cpp`.

## Node names and the tool's own footprint

ROS node names are resolved through the rclcpp graph API (endpoint GID → node), so the
tool registers a hidden node `_transport_viz_<pid>`. Its own endpoints are excluded from
the output. Discovery is observed by a second, raw Fast DDS participant so that rmw's own
discovery listener is never touched.

## Run it where the nodes run

The tool reads the same environment Fast DDS reads and never modifies it: run it in the
same shell environment as the nodes you observe — same `FASTDDS_BUILTIN_TRANSPORTS`,
`FASTRTPS_DEFAULT_PROFILES_FILE` (the observer participant takes the default participant
profile from it, like the nodes), `ROS_DISCOVERY_SERVER`, `ROS_AUTOMATIC_DISCOVERY_RANGE`,
`ROS_STATIC_PEERS`, and the same network and IPC namespace (for containers:
`network_mode` / `ipc`). If the tool cannot see the nodes, `ros2 topic list` in that
environment will not either. For hosts on a network without multicast see
[development.md](development.md#two-physical-hosts).

Transport-specific notes (all covered by launch tests or the multi-container scenarios,
see [development.md](development.md#verification-results)):

- `FASTDDS_BUILTIN_TRANSPORTS=LARGE_DATA` announces TCPv4 next to SHM; on one host SHM
  still wins (`both-shm-locators`), between hosts the verdict is `TCPv4`
  (`common-tcpv4-locator`) and `--stats` measures the TCP traffic. Start the tool with
  `LARGE_DATA` too when using `--stats`: the statistics samples travel over TCP.
- `UDPv6` / `DEFAULTv6` need an interface with an IPv6 address (Docker's default bridge
  has none); the tool must speak UDPv6 as well to hear the discovery traffic.
- `ROS_DISCOVERY_SERVER`: a plain client only learns about the endpoints it matches
  (Fast DDS 2.14 and 3.2; 3.6 in Rolling relays everything), so the tool makes itself a
  `SUPER_CLIENT` when the variable is set (a message on stderr says so). An explicit
  `ROS_SUPER_CLIENT` is respected. The server is `fastdds discovery -i 0 -l <ip> -p <port>`
  on Jazzy and `fastdds discovery -l <ip> -p <port>` on Kilted / Rolling.
- `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` works unchanged (nodes announce loopback
  locators only). `OFF` limits every participant to itself, so nothing can be observed;
  the tool prints a warning in that case.
- Large samples (2 MB `UInt8MultiArray`) stay on SHM; Fast DDS fragments them to the
  transport's maximum message size.

## Hosts

Without `--stats`, hosts are shown as `local` (same host id as the tool) or
`host:<4-byte hex>`. With `--stats`, host names and process ids come from the
statistics `PHYSICAL_DATA` topic. Containers with separate network namespaces on one
machine can share a host id while announcing different IP addresses; this is reported as
the warning `host-id-match-but-ip-differs`.

## Watch mode

`--watch` re-observes and re-renders every `--interval` seconds. On a terminal it uses the
alternate screen buffer (no flicker, restored on exit), truncates lines to the terminal
width, and highlights what changed since the previously rendered frame:

| Mark | Meaning |
|---|---|
| `+` (green) | pair appeared |
| `~` (yellow) | transport, confidence, measured transport or warnings changed |
| `-` (dim) | pair disappeared; kept for three frames, then dropped |

A topic row carries the mark of its pairs; a `changes:` summary line follows the table.
Keys while watching: `q` quit, `p` pause/resume (changes made while paused are
highlighted on resume), `v` toggle pair rows, `e` toggle the reason-code legend, `a` toggle
`--all`. When stdout is not a terminal the frames are printed one after another without
escape sequences; with `--json` every frame is one JSON Lines document that additionally
carries a `changes` object (`added_pairs`, `removed_pairs`, `changed_pairs` with
`from`/`to`).

Colors (`--color auto|always|never`, default `auto`; `NO_COLOR` is honoured) apply to
the one-shot table as well: transports use the same palette as the web viewer and
warnings are red.
