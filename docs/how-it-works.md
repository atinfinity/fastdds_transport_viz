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

0. **Do the QoS match at all?** Fast DDS only matches a writer and a reader whose
   request/offer policies agree: reliability (a BEST_EFFORT writer cannot serve a
   RELIABLE reader), durability (the writer must offer at least what the reader
   requests: VOLATILE < TRANSIENT_LOCAL < TRANSIENT < PERSISTENT), deadline (the
   writer's period must not exceed the reader's), liveliness (kind and lease duration),
   ownership (both SHARED or both EXCLUSIVE) and partition (a common name, patterns
   allowed). Otherwise the pair is `NONE` with `qos-incompatible-<policy>` reasons and the
   warning `qos-incompatible`: no data flows, whatever the transports. ROS 2 reports the
   same situation as an incompatible QoS event on the publisher / subscription.
1. **Same host?** Fast DDS considers two participants to be on the same host when the
   first 4 bytes of their GUID prefixes are equal.
2. Same host and both endpoints announce data-sharing (zero-copy), and their domain ids
   intersect or at least one side announces none → `DATA_SHARING` (confidence `likely`,
   see [data-sharing.md](data-sharing.md)). Announced but disjoint domain ids fall through.
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

## The RATE, LATENCY and LOSS columns

With `--stats`, `RATE` shows the payload throughput of the topic's writers (sum) and, in
the pair rows, of that writer: the `PUBLICATION_THROUGHPUT` statistic, averaged over the
observation, in SI units (`24 B/s`, `1.31 MB/s`). It counts serialized samples handed to
the writer, so it is independent of the transport and present for zero-copy pairs too.
`LATENCY` is the `HISTORY_LATENCY` statistic: the time from the writer's `write()` to the
notification of the reader, per pair, as mean and maximum over the observation (`420 µs
(max 1.30 ms)`); the topic row shows the mean of its slowest pair. It is measured with
the clocks of the two hosts, so between machines it includes their offset (a negative
mean is flagged `latency-clock-skew-suspected`); on one host it is exact. `LOSS` sums
the reliability counters of the pair during the observation: `lost` is what the reader's
participant reported missing from the writer's locators (`RTPS_LOST`, sequence-number
gaps; warning `rtps-packets-lost`), `resent` the DATA submessages the writer sent again
(`RESENT_DATAS`); `0` when nothing was lost or resent. Heartbeats, gaps, acknacks and
nackfrags are in the JSON `measured.reliability` object and the web viewer's pair card.
Without statistics the three columns show `-`. The `measured=` cell of a pair row gives what
the transport actually carried during the observation (`SHM 148pkt 7.63 MB`, or `(idle)`
when packets flowed only before the observation).

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

## Fast DDS 2.6 (ROS 2 Humble)

Two things differ on Humble's Fast DDS 2.6:

- **No statistics.** The Humble binary is built without the statistics module
  (`FASTDDS_STATISTICS` off in `config.h`), so the observed nodes cannot publish
  statistics whatever `FASTDDS_STATISTICS` says. `--stats` prints a warning and every
  pair shows `stats-not-enabled-on-writer`; `RATE`, `LATENCY` and `measured=` stay empty.
  A Fast DDS built with the module on works with the tool's 2.6 support.
- **Same-host locators are filtered.** Fast DDS below 2.10 announces only the SHM locator
  of a participant on the same host to the tool. When the other side has no SHM locator
  (a UDP-only participant), no common locator kind is visible; the tool then predicts
  `UDPv4?` with the reason `same-host-locators-hidden`, because both sides keep the
  builtin UDPv4 transport, and Fast DDS does fall back to it.
- `FASTDDS_BUILTIN_TRANSPORTS` (Fast DDS 2.12+) and `ROS_AUTOMATIC_DISCOVERY_RANGE` /
  `ROS_STATIC_PEERS` (ROS 2 Iron+) do not exist there; transports are configured through
  an XML profile (`test/launch/udpv4_only.xml` is an example of a UDPv4-only participant).

## Hosts

Without `--stats`, hosts are shown as `local` (same host id as the tool) or
`host:<4-byte hex>`. With `--stats`, host names and process ids come from the
statistics `PHYSICAL_DATA` topic. Containers with separate network namespaces on one
machine can share a host id while announcing different IP addresses; this is reported as
the warning `host-id-match-but-ip-differs`.

## Shared memory of the environment

Every run ends with one line about the shared memory of the environment the tool runs
in, because SHM verdicts depend on it:

```
shared memory: /dev/shm 396 MB used of 16.7 GB (16.3 GB free) | Fast DDS 63.4 MB in 114 segment(s) (110 stale), 14 port(s) (7 stale), 6 data-sharing histories (6 unmatched)
  !shm-stale-files: 117 file(s) without a living owner, run 'fastdds shm clean'
```

- **Capacity** is `statvfs("/dev/shm")`: total, used and free bytes of the tmpfs. Docker
  gives a container 64 MB unless `--shm-size` or `--ipc=host` is used; Fast DDS needs one
  segment per participant (512 KB by default, more for large data) and fails to create it
  when the directory is full.
- **Fast DDS files** are the `fastrtps_*` (Fast DDS 3.x: `fastdds_*`) and
  `fast_datasharing_*` entries: one
  *segment* (`fastrtps_<hex>`) per participant, one *port* ring buffer
  (`fastrtps_port<N>`) per SHM locator, and one *data-sharing history* per writer that
  uses zero-copy delivery. Their sizes (plus the small `sem.fastrtps_*` mutex files) are
  summed.
- **Stale** files are segments and ports whose `_el` lock file exists but nobody holds
  (the same `flock` probe `fastdds shm clean` uses): their owner died without cleaning
  up, and they keep consuming `/dev/shm`. The warning `shm-stale-files` suggests
  `fastdds shm clean`, which removes exactly these. When none of the observed nodes is
  in this IPC namespace (see visibility below) the stale counts are not reported, because
  nothing here can be theirs.
  Data-sharing histories have no lock; those that belong to a discovered writer are
  reported on the writer (`datasharing_history_bytes` in JSON, the web viewer's endpoint
  details), the rest are counted as *unmatched* (another domain, or a finished writer).
- **Visibility**: a node in the tool's IPC namespace holds the lock of its SHM port file
  (`fastrtps_port<N>_el`). When an observed node has another host id, or its port is not
  held here (or is the tool's own port number, i.e. the same participant id in another
  network namespace), the node uses another `/dev/shm` and the warning `shm-not-visible`
  says so: the figures then describe the tool's environment, not the nodes', and SHM
  between the nodes and this process is impossible. One case escapes the check: same
  host id but separate IPC namespaces (`network_mode: host` without `ipc: host`), where
  the tool's own participant opens the announced port here to send to it.
- `shm-nearly-full` warns at 90 % usage or less than 16 MiB free.

The line is omitted where there is no `/dev/shm` (macOS). In JSON the same data is the
`shm` object; `--watch` refreshes it every frame.

## Watch mode

`--watch` re-observes and re-renders every `--interval` seconds. On a terminal it uses the
alternate screen buffer (no flicker, restored on exit), truncates lines to the terminal
width, and highlights what changed since the previously rendered frame:

| Mark | Meaning |
|---|---|
| `+` (green) | pair appeared |
| `~` (yellow) | transport, confidence, measured transport or warnings changed |
| `-` (dim) | pair disappeared; the row is kept as a dimmed ghost |

Every mark stays for three frames after the change, then the row returns to normal
(ghost rows are dropped).

A topic row carries the mark of its pairs; a `changes:` summary line follows the table.
One frame, right after a new listener appeared and the UDP listener went away:

![watch frame](images/example-watch.svg)

Keys while watching: `q` quit, `p` pause/resume (changes made while paused are
highlighted on resume), `v` toggle pair rows, `e` toggle the reason-code legend, `a` toggle
`--all`. Unless both stdin and stdout are a terminal the frames are printed one after
another (without escape sequences, unless `--color always`); with `--json` every frame is one JSON Lines document that additionally
carries a `changes` object (`added_pairs`, `removed_pairs`, `changed_pairs` with
`from`/`to`).

Colors (`--color auto|always|never`, default `auto`, which honours `NO_COLOR`) apply to
the one-shot table as well: transports use the same palette as the web viewer and
warnings are red.

![colored table](images/example-table.svg)

Both images are real output (`scripts/render_examples.sh` captures it with
`--color always` and `scripts/ansi2svg.py` turns the ANSI colors into SVG).
