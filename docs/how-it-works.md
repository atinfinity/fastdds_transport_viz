# How it works

The tool builds against Fast DDS 2.14 (ROS 2 Jazzy) and 3.x (Lyrical, Rolling); the
API differences live in `include/fastdds_transport_viz/fastdds_compat.hpp`. The decision
rules below are the same in both.

`ros2 topic info -v` cannot tell you the transport: the rmw layer exposes no locator
information. `transport_viz` therefore creates its own Fast DDS `DomainParticipant` and
listens to endpoint discovery, which carries every remote writer's/reader's **announced
locators** (`UDPv4`, `SHM`, ...) and QoS. It then applies the same rules Fast DDS 2.14 uses
to select a transport for each writer → reader pair.

## Decision rules

0. **Do the two announce the same type?** Fast DDS matches a writer and a reader by the
   type name of the topic, so endpoints of different types never see each other: the pair
   is `NONE` with the reason `type-name-mismatch`. Both endpoints are still shown, and so
   is the pair, because what is wrong is between them. The same type name with a different
   **ROS 2 type hash** (REP-2011) means the two message definitions differ: the pair keeps
   the transport of the rules below and carries the warning `type-hash-mismatch`, because
   what happens next depends on the Fast DDS version - 2.x matches the pair and delivers
   the samples over that transport, and the rmw drops them before the subscription
   callback; 3.x does not match the pair at all. Either way the subscription receives
   nothing. Where at least one side announces no ROS 2 type hash - a non-ROS Fast DDS peer,
   or ROS 2 Humble - the same question is asked of the XTypes `TypeInformation` the
   endpoints announce instead: two EK_COMPLETE equivalence hashes that differ describe
   different types under one name, and the pair carries the warning
   `type-information-mismatch` ([#206](https://github.com/atinfinity/fastdds_transport_viz/issues/206)).
   Fast DDS 2.x announces none under `rmw_fastrtps` ([#193](https://github.com/atinfinity/fastdds_transport_viz/issues/193)),
   so this reaches a 3.x peer only. An endpoint that announces no `TypeInformation` at all -
   every Fast DDS 2.x endpoint, a 3.x application that registered no TypeObject, and any
   peer of another vendor, whose `TypeInformation` Fast DDS 3.x ignores - is matched on the
   type name alone, and the tool cannot check its definition either. When that definition
   differs, the reader's `take()` drops the sample or fills the missing member with its
   default, depending on the payload, without a log line or a lost/rejected count; the
   statistics are recorded before deserialization, so `--stats` still shows the pair as
   delivered ([#210](https://github.com/atinfinity/fastdds_transport_viz/issues/210)).
1. **Do the QoS match at all?** Fast DDS only matches a writer and a reader whose
   request/offer policies agree: reliability (a BEST_EFFORT writer cannot serve a
   RELIABLE reader), durability (the writer must offer at least what the reader
   requests: VOLATILE < TRANSIENT_LOCAL < TRANSIENT < PERSISTENT), deadline (the
   writer's period must not exceed the reader's), liveliness (kind and lease duration),
   ownership (both SHARED or both EXCLUSIVE) and partition (a common name, patterns
   allowed). Otherwise the pair is `NONE` with `qos-incompatible-<policy>` reasons and the
   warning `qos-incompatible`: no data flows, whatever the transports. ROS 2 reports the
   same situation as an incompatible QoS event on the publisher / subscription.
2. **Same host?** Fast DDS considers two participants to be on the same host when the
   first 4 bytes of their GUID prefixes are equal.
3. Same host and both endpoints announce data-sharing (zero-copy), and their domain ids
   intersect or at least one side announces none → `DATA_SHARING` (confidence `likely`,
   see [data-sharing.md](data-sharing.md)). Announced but disjoint domain ids fall through.
   When the two are seen to use different `/dev/shm`, the pair is `NONE` instead (see
   [Split IPC namespaces](#split-ipc-namespaces)).
4. Same host and both announce a SHM locator → `SHM`. Fast DDS then uses shared memory
   exclusively for user data between those participants; discovery still goes over UDP.
   When the two participants are seen to listen in different IPC namespaces, the pair is
   `NONE` instead (see [Split IPC namespaces](#split-ipc-namespaces)).
5. Otherwise the first network locator kind the reader announces that the writer also
   speaks → `UDPv4` / `UDPv6` / `TCPv4` / `TCPv6`.
6. Nothing in common → `NONE`.

Topics with only publishers or only subscriptions are listed with `-` and the reason
`no-matching-reader` / `no-matching-writer`.

`--topic REGEX` keeps the topics whose name matches. `--node REGEX` keeps the pairs in
which the writer or the reader belongs to a node whose full name (`/ns/name`) matches,
together with that node's unpaired endpoints; the other side of a kept pair stays
visible even if it does not match. Both filters combine with AND. An invalid regex is
rejected at start-up (exit code 2).

### The ROS 2 type hash

The rmw announces the hash of the message definition (REP-2011) in the endpoint's
USER_DATA, as `typehash=RIHS01_<64 hex>;`. `transport_viz` reads it from the discovery
data, compares the two sides of every pair, and shows it as `type_hash` in the JSON and in
the web viewer's endpoint panel. Only the rmw of ROS 2 Jazzy and later announces one: an
endpoint of ROS 2 Humble, or of a participant that is not a ROS 2 node, announces none and
is compared on the type name alone. Two nodes built against different versions of the same
message package therefore look healthy on Humble - the table shows the transport the wire
uses, and the subscription still receives nothing.

### Intra-process delivery

A writer and a reader of the *same process* never put a sample on a transport: Fast DDS
hands it over inside the domain participant factory (`intraprocess_delivery`, `FULL` by
default, which `rmw_fastrtps` does not change). It also beats data-sharing -
`ReaderLocator::start` clears the data-sharing flag for a local reader, so the writer fills
its shared history and never notifies the reader through the segment.

The tool recognises such a pair from the GUID prefixes alone. An eProsima prefix is
`[0-1]` the vendor id `01.0f`, `[2-3]` the host id, `[4-5]` the low bytes of the pid,
`[6-7]` a value `std::random_device` gives the process once and `[8-11]` the participant
id, so two endpoints share their first 8 bytes exactly when they are in one process - which
is what `RTPSDomainImpl::should_intraprocess_between()` compares. The pair keeps the
transport the locators predict (what it *would* use from another process, usually `SHM`)
and carries the reason `intra-process`, on every run and every distribution. In the
`MEASURED` column it reads `(intra-process)` rather than `(unmeasured, delivered)`: no
packet is missing, none was ever sent. A pair with data-sharing QoS on both sides gets
`intra-process` in place of `datasharing-unverified-by-traffic` and no `certain` upgrade
from a silent `DATA_COUNT`, because a `DATA_COUNT` of 0 says nothing here.

Measurement wins over the prediction: a pair that *did* report packets - a custom
`<prefix>`, a build with intra-process delivery off - loses the reason and is reported as
measured. See [Intra-process pairs](statistics.md#intra-process-pairs) for what this means
for `--stats`.

## The LATENCY, HZ and LOSS columns

`HZ` is the number of samples per second that reached the reader of a pair, counted by the
tool from the `HISTORY_LATENCY` statistic (one sample per delivered sample, on every delivery
path) over the whole observation, or over the last 5 s under `--watch`. `≥120` says the
statistics of the reader's participant were partly lost on their way to the tool, so the value
is a lower bound. Only pair rows have it. It replaces the `RATE` column removed in
[#137](https://github.com/atinfinity/fastdds_transport_viz/issues/137), which was fed by
`PUBLICATION_THROUGHPUT` - a per-write payload/interval sample, not a rate - and it is a
delivered rate, not a publish rate: see
[statistics.md](statistics.md#the-hz-column-delivered-samples-per-second)
([#143](https://github.com/atinfinity/fastdds_transport_viz/issues/143)).

With `--stats`, `LATENCY` is the `HISTORY_LATENCY` statistic: the time from the writer's `write()` to the
notification of the reader, per pair, as mean and maximum over the observation (`420 µs
(max 1.30 ms)`); the topic row shows the mean of its slowest pair. It is measured with
the clocks of the two hosts, so between machines it includes their offset (a negative
mean is flagged `latency-clock-skew-suspected`); on one host it is exact. `LOSS` sums
the reliability counters of the pair during the observation: `lost` is what the reader's
participant reported missing from the writer's participant on the reader's unicast
locators (`RTPS_LOST`, sequence-number gaps, per participant pair, see
[statistics.md](statistics.md#rtps_lost); warning `rtps-packets-lost`; `- lost` when the
reader's participant does not publish it), `resent` the DATA submessages the writer sent again
(`RESENT_DATAS`); `0` when nothing was lost or resent. Heartbeats, gaps, acknacks and
nackfrags are in the JSON `measured.reliability` object and the web viewer's pair card.
Without statistics the three columns show `-`. The `measured=` cell of a pair row gives what
the transport actually carried during the observation (`SHM 148pkt 7.63 MB`, or `(idle)`
when packets flowed only before the observation; `(unmeasured, delivered)` when deliveries
were proven meanwhile, so the statistics samples rather than the packets are missing).

## Reason codes

Every verdict carries machine-readable reason codes (`same-host-guid`,
`reader-no-shm-locator`, ...) and, where relevant, warnings prefixed with `!`.
`--explain` appends a legend for the codes used in the current output;
`transport_viz --list-codes` prints all of them. A `?` after a transport means confidence
`likely` rather than `certain`.

A code says what happened; `--advise` adds what to change. Every code has either a
remedy (one sentence naming the environment variable, XML element or QoS policy, e.g.
`reader-no-shm-locator` → unset `FASTDDS_BUILTIN_TRANSPORTS` or add a SHM transport
descriptor to the reader's profile; `shm-stale-files` → `fastdds shm clean`) or none,
when it describes a normal state (`same-host-guid`), a measured fact
(`measured-shm-traffic`) or asks for a bug report (`measured-transport-mismatch`). The
remedies are target-agnostic: the tool does not know which transport you intend, so each
sentence names what the code stands in the way of, and you pick. `--advise` prints a
`fix <code>: …` line under each pair for its codes that have one (also for `NONE` pairs,
whose QoS remedies are the most useful) and the remedy under each code of the legend;
`--list-codes` / `ros2 transport codes` print it after each description; `--json` carries
them as `reason_code_remedies` (same keys as `reason_code_descriptions`, `null` for none);
the web viewer shows them under the descriptions. Descriptions themselves no longer
contain remedies, so each is said once.

### Native-buffer companion topics

On Lyrical and later, `rmw_fastrtps_cpp` gives every writer and reader of a type with an
unbounded `uint8[]` field (`std_msgs/msg/UInt8MultiArray`, `sensor_msgs/msg/Image`, ...) a
companion on `<topic>/_buf_cpu` in the same participant ("native buffers"). When every
subscription of the topic supports native buffers the samples go through the companions
only, so the parent pair on its own shows no DATA submessage, no heartbeat and no delivery.
The tool links each companion to its parent (same participant, same kind, same type, the
topic name without `/_buf_cpu`; several candidates are told apart by the entity key, which
the rmw allocates right after the parent's) and adds the companion's statistics counters to
the parent pair: delivered samples, DATA submessages, resends, heartbeats, gaps, acknacks,
nackfrags and latency. `RTPS_SENT` and `RTPS_LOST` are per participant and
already cover both. The parent pair carries `buffer-companion-folded`; the verdict rules are
unchanged. The companion topic keeps its own numbers with `buffer-companion` and is left out
of the output unless `--all` when every endpoint on it is linked. A companion the tool cannot
link stays visible with `buffer-companion-unmatched`. In JSON a linked companion endpoint
names its parent in `buffer_parent_guid`. `rmw_fastrtps_dynamic_cpp` creates no companions,
and neither does any RMW on Humble or Jazzy
([#119](https://github.com/atinfinity/fastdds_transport_viz/issues/119)).

The decision logic lives in `src/fastdds_transport_viz/src/decision.cpp` as pure
functions with no DDS dependency, and is covered by `test/test_decision.cpp`.

## Services and actions

`rmw_fastrtps` mangles a ROS name into one of three DDS names: `rt/<topic>` for a topic,
`rq/<service>Request` and `rr/<service>Reply` for a service. A service is therefore two DDS
topics, and since both demangle to the same ROS name, `--all` used to print two rows with
identical text. `rcl_action` builds an action out of five members under `<action>/_action/`
-- the `send_goal`, `cancel_goal` and `get_result` services and the `feedback` and `status`
topics -- so an action is eight DDS topics and sixteen endpoints.

`--all` collects them into one row per group per client-server pair
([#84](https://github.com/atinfinity/fastdds_transport_viz/issues/84)):

```
TOPIC                                                    TYPE                               PUBS  SUBS  TRANSPORT
SERVICE /add_two_ints  /caller -> /add_two_ints_server   example_interfaces/srv/AddTwoInts  1     1     SHM -> SHM
ACTION /fibonacci  /ftv_act_client -> /ftv_act_server    example_interfaces/action/Fibonacci  3   5     SHM -> SHM
```

The two sides are **directions of travel**, not request and reply: `PUBS` counts the member
pairs going to the server and `SUBS` those coming back, which is what lets an action fit on
one line (`feedback` and `status` travel back, like the three replies). A complete service
reads `1`/`1` and a complete action `3`/`5`, so an incomplete group is readable at a glance.
The row is keyed on the **participant** of each side rather than the node, because service
endpoints are absent from `ros_discovery_info` and carry no node name: a side whose name is
unknown shows its participant prefix instead. Every client of one service gets a row of its
own, and endpoints in no pair still get a half-open row (`- -> /talker`, `0`/`0`), so an
uncalled parameter service is visible rather than missing. `TYPE` is the members' type with
the `_Request` / `_Response` tail taken off; an action takes its type from `send_goal`,
`get_result` and `feedback` alone, because `cancel_goal` and `status` carry `action_msgs`
types every action shares. Transports, latency, loss and reason codes are the members'
aggregated the way a topic's pairs already are, and `-v` names the member each pair belongs
to.

`ACTION` is claimed only when the **names and the types** agree: the action name is
everything before the last `/_action/`, the suffix is one of the five members, and every
member present announces the type `rcl_action` would have given it
(`<pkg>::action::dds_::<Action>_SendGoal_*`, `action_msgs::srv::dds_::CancelGoal_*`,
`action_msgs::msg::dds_::GoalStatusArray_`, ...), with at least one member typed
`::action::`. `/_action/` is not reserved -- a plain service may be named
`/fibonacci2/_action/send_goal`, and `ros2 action list` is itself fooled by a pair of plain
`feedback` / `status` topics -- so a group that fails the type check falls back to `SERVICE`
or to plain topics rather than inventing an action. `<node>/_service_event` stays a plain
topic: it is an `rt/` topic visible in the default view, and folding it into a group would
delete a default-view row.

In `--json` each entry of `topics[]` carries `kind` (`topic`, `service`, `action` or
`other`), `group` (the owning ROS name, `""` for a plain topic) and `direction`
(`to_server`, `to_client` or `""`); the raw `rq/` / `rr/` topics stay in the document, so
the table replaces rows and the JSON loses nothing. A document written before these keys
existed is classified again from its own DDS names when it is read back, so `diff` groups it
too.

## Node names and the tool's own footprint

ROS node names are resolved through the rclcpp graph API (endpoint GID → node), so the
tool registers a hidden node `_transport_viz_<pid>`. Its own endpoints are excluded from
the output. Discovery is observed by a second, raw Fast DDS participant so that rmw's own
discovery listener is never touched.

The graph API reads `ros_discovery_info`, where the rmw of every node publishes its node
names and endpoint GIDs. rclcpp's participant announces SHM, so a node with the tool's host
id in another IPC namespace writes those samples into its own `/dev/shm`, and rclcpp
reports the rmw's `_NODE_NAMESPACE_UNKNOWN_/_NODE_NAME_UNKNOWN_`. The raw participant
therefore reads `ros_discovery_info` itself too, with a reader that announces only the
participant's non-SHM unicast locators (UDP, or TCP with `LARGE_DATA`), like the
statistics readers. On Humble that reader has a participant of its own without the SHM
transport: Fast DDS 2.6 keeps only the SHM locator of a same-host endpoint in the
discovery data of a participant with SHM, so the nodes' writers would reach a reader on
the raw participant over SHM alone. Its names fill the endpoints the graph API cannot
name; when both know a name, the graph API's is used. A name still cannot be read when the node or the tool has
no transport but SHM (`FASTDDS_BUILTIN_TRANSPORTS=SHM`), or when the node's samples do not
decode (a node of another ROS distribution: the GIDs are 24 bytes on Humble, 16 on Jazzy
and newer). Such an endpoint gets an empty node name, like a raw DDS endpoint: the table
labels it by GUID, `--node` does not match it, `diff` matches it by GUID and the web viewer
shows its participant. The unknown name in JSON documents written by earlier versions is
read as empty as well ([#112](https://github.com/atinfinity/fastdds_transport_viz/issues/112)).

## Run it where the nodes run

The tool reads the same environment Fast DDS reads and never modifies it: run it in the
same shell environment as the nodes you observe — same `FASTDDS_BUILTIN_TRANSPORTS`,
`FASTRTPS_DEFAULT_PROFILES_FILE` (the observer participant takes the default participant
profile from it, like the nodes), `ROS_DISCOVERY_SERVER`, `ROS2_EASY_MODE`, `ROS_AUTOMATIC_DISCOVERY_RANGE`,
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
  on Jazzy and `fastdds discovery -l <ip> -p <port>` on Lyrical / Rolling.
  The server itself is a participant without endpoints, so it has no row in the table, but
  it is in the `participants` section of `--json` and in the web viewer
  ([#86](https://github.com/atinfinity/fastdds_transport_viz/issues/86)): every participant
  reports the `PARTICIPANT_TYPE` it announced, verbatim (`SIMPLE`, `CLIENT`,
  `SUPER_CLIENT`, `SERVER`, `BACKUP`; `""` when it announced none), its name, vendor and
  metatraffic unicast locators, and `discovery{}` says how the tool itself took part
  (`observer_protocol`, the parsed `discovery_servers`, `discovery_server_env`,
  `easy_mode`). `-v` adds one footer line naming the servers seen and how many
  participants each serves. Which server a client uses is *not* on the wire (Fast DDS
  puts a client's server list into its participant data only with the opt-in
  `fastdds.serialize_optional_qos` property of 3.2+, never for ROS 2 nodes), so
  `discovery_server` is inferred and only where it is certain: with exactly one `SERVER`
  discovered every client is its client; under Easy Mode a client's server is the
  `DiscoveryServerAuto` of its own host; otherwise it is `null`. A server nobody reaches
  cannot be reported: its clients are never relayed. Fast DDS 3.6 (Lyrical, Rolling)
  announces a plain `CLIENT` as `SUPER_CLIENT`. A legacy `fastdds discovery -i N` server
  carries the fixed prefix `44.53.<N>.5f.45.50.52.4f.53.49.4d.41`, so its `host_id` is
  not a real host and the viewer shows it in a host column of its own.
- `ROS2_EASY_MODE=<ip>` (Fast DDS 3.2+: Kilted, Lyrical, Rolling) makes Fast DDS spawn one
  Discovery Server per host and domain (port 7400 + 250 × domain + 2, `fastdds discovery
  list` shows it; the CLI finds a running server with `ss`, so `iproute2` must be
  installed) and switch every participant to the `P2P` builtin transport: SHM and
  TCPv4 for user data, UDPv4 unicast to the local server for discovery, no multicast at
  all. The verdicts are those of `LARGE_DATA`: `SHM` on one host, `TCPv4`
  (`common-tcpv4-locator`) between hosts, `--stats` measures the TCP traffic. Start the
  tool with the same `ROS2_EASY_MODE` value as the nodes (a note on stderr confirms it); a
  participant without it cannot see them. Run it on a host where the nodes run, the master
  host is a good choice: Fast DDS 3 hands an endpoint to the tool only once its type is
  resolved, and a host without a node of that type never gets it resolved through the
  server mesh, so from such a host the tool, like `ros2 topic list`, sees the nodes but
  none of their topics. The auto-started server (`DiscoveryServerAuto`) is a participant
  without endpoints, so it has no row in the table, `--all` included, but it is in the
  `participants` section, in the `-v` footer and in the web viewer, with every node of
  its host attributed to it ([#86](https://github.com/atinfinity/fastdds_transport_viz/issues/86));
  with `--stats` it also shows up among the statistics participants when the node that
  spawned it had `FASTDDS_STATISTICS` set. The
  `fastdds discovery` CLI that Fast DDS runs for every participant prints on stdout; the
  tool sends that to stderr so that `--json` stays parseable.
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
  pair shows `stats-not-enabled-on-writer`; `LATENCY`, `HZ` and `measured=` stay empty.
  A Fast DDS built with the module on works with the tool's 2.6 support.
- **Same-host locators are filtered.** Fast DDS below 2.10 announces only the SHM locator
  of a participant on the same host to the tool. When the other side has no SHM locator
  (a UDP-only participant), no common locator kind is visible; the tool then predicts
  `UDPv4?` with the reason `same-host-locators-hidden`, because both sides keep the
  builtin UDPv4 transport, and Fast DDS does fall back to it.
- `FASTDDS_BUILTIN_TRANSPORTS` (Fast DDS 2.12+) and `ROS_AUTOMATIC_DISCOVERY_RANGE` /
  `ROS_STATIC_PEERS` (ROS 2 Iron+) do not exist there; transports are configured through
  an XML profile (`test/launch/udpv4_only.xml` is an example of a UDPv4-only participant).

## Hosts and addresses

Without `--stats`, hosts are shown as `local` (same host id as the tool) or
`host:<4-byte hex>`. With `--stats`, host names and process ids come from the
statistics `PHYSICAL_DATA` topic: the table then labels every endpoint
`node@host(pid)`, and `--json` carries `host_name` (reported as
`<hostname>:<numeric host id>`) and `process` per endpoint, plus the raw
`stats.physical` table (host, user and process per participant). Containers with
separate network namespaces on one machine can share a host id while announcing
different IP addresses; this is reported as the warning
`host-id-match-but-ip-differs`.

The addresses themselves are not a table column - the table shows the transport kind and
the reason codes - but `--locators` adds a line under each pair of the verbose table with
the locator the tool selected and, with `--stats`, the locators that actually carried
packets:

```
$ ros2 transport list -v --locators --stats --topic '^/(chatter|bounded)$'
    /talker@host(61) -> /listener_udp@host(49)  UDPv4  414 us  0  measured=UDPv4 9pkt 1.19 kB  ...
        locators: UDPv4 127.0.0.1:7411 (selected = measured, 9 pkt)
    /talker@host(61) -> /listener@host(50)      SHM    453 us  0  measured=SHM 10pkt 1.31 kB   ...
        locators: SHM port 7413 (selected = measured, 10 pkt)
    /bounded_pub@host(56) -> /bounded_sub@host(55)  DATA_SHARING  195 us  0  ...
        locators: selected DATA_SHARING (no locator) | measured SHM port 7419 (1 pkt)
```

The word `selected` appears only where a measured side is printed next to it. An SHM
locator names the `/dev/shm` port the writer writes into rather than an address, so it
lines up with the `fastrtps_port<N>` files counted in the shared-memory line below the
table. A group address is marked `(multicast)`, zero-copy data-sharing has no locator at
all, and on Fast DDS below 2.10 the line says `UDPv4 (hidden by Fast DDS < 2.10)` because
the locator the prediction would use never reaches the tool. When the selected locator carried no packets at all - the traffic took
another locator the reader also announced, typically a different interface of a
multi-homed host - the pair is flagged `!measured-locator-mismatch`.

`--locators` implies `-v` and is ignored with `--json`, which always carries the same
data in `pairs[].locator` and `pairs[].measured.locators[]`. The announced locators of
every endpoint are in `--json` too, from discovery alone, with no `--stats` needed:

```
ros2 transport list --json | jq '.topics[].writers[] | {node, unicast_locators}'
```

```json
{
  "node": "/bounded_pub",
  "unicast_locators": [
    { "kind": "SHM",   "address": "",          "port": 8169 },
    { "kind": "UDPv4", "address": "127.0.0.1", "port": 8169 }
  ]
}
```

`unicast_locators` and `multicast_locators` are part of the published schema
(`schema/transport_viz.schema.json`). Three things they do not tell you:

- They are the locators a participant **announces**, that is, the addresses it is
  willing to receive on - not the address a packet came from. Behind NAT, or in a
  container on a bridge network, a participant can announce addresses that are
  unreachable from where the tool runs.
- SHM locators have an empty `address`; the `port` is the Fast DDS shared-memory port
  (`fastrtps_port<N>` in `/dev/shm`), not a network port.
- Below Fast DDS 2.10 only the SHM locator of a same-host peer reaches the tool, see
  [Fast DDS 2.6](#fast-dds-26-ros-2-humble).

With `--stats` the locators that actually carried packets are reported as well:
`stats.traffic[]` has `dst_locator` (`RTPS_SENT`, keyed by the sending participant)
and `stats.lost[]` has `dst_locator` too (`RTPS_LOST`, keyed by the reporting receiver
and the sending participant), both with `kind`, `address` and `port`. Fast DDS reports the locators of
a same-host participant as `127.0.0.1` / `::1` while a remote writer's `RTPS_SENT`
names the real address, so the tool matches both spellings when it attributes traffic
to a reader.

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
  A data-sharing reader keeps a *notification* segment of the same form
  (`fast_datasharing_<reader guid>`); those of discovered readers are counted apart
  (`datasharing_notifications`, shown when there are any).
- **Visibility**: a node in the tool's IPC namespace holds the lock of its SHM port file
  (`fastrtps_port<N>_el`). When an observed node has another host id, or its port is not
  held here (or is the tool's own port number, i.e. the same participant id in another
  network namespace), the node uses another `/dev/shm` and the warning `shm-not-visible`
  says so: the figures then describe the tool's environment, not the nodes', and SHM
  between the nodes and this process is impossible. The tool's own ports are those of
  every participant in its process (`DomainParticipantFactory::lookup_participants()`)
  plus the port locks the process holds open (`/proc/self/fd`), so its own lock is never
  taken for a node's. A sender does not lock the port it writes to, so nodes with the
  tool's host id in a separate IPC namespace (`network_mode: host` without `ipc: host`)
  are reported as well.
  Between two such nodes Fast DDS still picks SHM (same host id), but their ports are in
  different `/dev/shm` and every message is lost; the pair is `NONE` with
  `shm-ipc-namespace-split` when the tool can tell (see
  [Split IPC namespaces](#split-ipc-namespaces)).
- `shm-nearly-full` warns at 90 % usage or less than 16 MiB free.

The line is omitted where there is no `/dev/shm` (macOS). In JSON the same data is the
`shm` object (with `missing_ports`, the announced ports without a lock file here, and
`unknown_ports`, those whose lock could not be probed); `--watch` refreshes it every frame.

The per-participant evidence behind the visibility verdicts is the `participants` array
of the document ([#125](https://github.com/atinfinity/fastdds_transport_viz/issues/125)):
one entry per discovered participant with `guid_prefix`, `host_id`, `host`, `host_name`,
`own` (a participant of the tool's process), `shm_visibility` (`visible`, `not-visible`,
`unprobed`) and `shm_ports`, its announced SHM ports on the tool's host with the lock as
probed from the tool's IPC namespace (`held`, `own`, `absent` - no lock file, `stale` - a
free lock, `unknown` - unreadable, `unprobed` - no `/dev/shm` or another host),
`announced_by` (how many participants announce that number) and `proof` (whether a held
lock on it counts as evidence, see [Split IPC namespaces](#split-ipc-namespaces)). The
array is present when at least one participant was discovered, on every platform; the web
viewer shows the same in the endpoint panel's `shm` row.

### Split IPC namespaces

Fast DDS takes the same host id as proof that shared memory reaches the other
participant. With `network_mode: host`, containers share the host id, but each one without
`ipc: host` (or a common `ipc: service:…` / `ipc: container:…`) has its own `/dev/shm`.
A writer then writes into a port file that no reader listens on and the listener receives
nothing, with no error on either side. The tool reports such a pair as `NONE`, `certain`,
with the warning `shm-ipc-namespace-split`, when one of these holds:

- `shm-port-collision`: the writer's and the reader's participants announce the same SHM
  port number. Only one participant per IPC namespace can listen on a port, so equal
  numbers mean two namespaces. This is the usual case with one node per container, because
  each namespace numbers its ports from the same start (on Jazzy and newer the
  `ros_discovery_info` reader's port, 7000 for the first node). It does not depend on the
  tool's IPC namespace, but the tool needs the nodes' host id (the host network), because
  it gathers the SHM ports of the participants on its own host only.
- `shm-reader-port-not-visible` / `shm-writer-port-not-visible`: from the tool's IPC
  namespace, every SHM port of one participant is held, and one of them is announced by
  nobody else and by one of its endpoints other than the `ros_discovery_info` reader,
  while a port of the other has no lock file here or is one of the tool's own ports. It
  needs the tool in the IPC namespace of one side. A free lock (left by a node that just
  died) decides nothing. Two kinds of held port neither prove nor contradict it, and leave
  the decision to the participant's other ports: one whose number another participant
  announces too (with several IPC namespaces the lock can be either one's), and the
  `ros_discovery_info` reader's 7000+ port, whose number any Fast DDS participant of the
  namespace can take, whatever its domain
  ([#118](https://github.com/atinfinity/fastdds_transport_viz/issues/118)). On Humble each
  participant has a single SHM port, numbered per IPC namespace: when another node in the
  other namespace takes the same number as one side, and the two sides do not collide with
  each other, neither signal fires and the pair stays `SHM`.

Data-sharing endpoints (rule 3) fail the same way. Fast DDS pairs them on QoS alone; the
reader cannot open the writer's history in its own `/dev/shm` and rejects the writer, and
the writer sends it nothing through a transport, so no sample arrives. Such a pair is
`NONE`, `certain`, with `shm-ipc-namespace-split` on the evidence above when both sides
announce SHM, or on the data-sharing segments, which exist without the SHM transport too:

- `datasharing-reader-segment-not-visible` / `datasharing-writer-segment-not-visible`: the
  writer's history (`fast_datasharing_<writer guid>`) is in the tool's `/dev/shm` and the
  reader's notification segment (`fast_datasharing_<reader guid>`) is not, or the reverse.
  It needs the tool in the IPC namespace of one side ([#110](https://github.com/atinfinity/fastdds_transport_viz/issues/110)).
  What the tool saw is on each endpoint as `datasharing_segment_visibility` in JSON
  (`visible`, `not-visible`, or `unprobed` for an endpoint on another host, without
  data-sharing in its QoS, or where there is no `/dev/shm`), and in the web viewer's
  endpoint panel ([#163](https://github.com/atinfinity/fastdds_transport_viz/issues/163)).

For a pair reported this way, `--advise` gives the remedy: put both nodes in one IPC
namespace (`ipc: host`), or disable SHM on one side so that UDPv4 is selected, and
data-sharing as well for a data-sharing pair (`data_sharing` OFF in its QoS profile).

When neither can be told, for example with the tool in a third IPC namespace and
different port numbers on the two sides, the pair stays `SHM` (or `DATA_SHARING`), and
`shm-not-visible` on the shared-memory line is the only hint. The node names of both sides
are still shown: the tool reads them over UDP (see "Node names and the tool's own
footprint" above, [#112](https://github.com/atinfinity/fastdds_transport_viz/issues/112)).

With `--stats` the writer's SHM traffic on such a pair is expected (it writes into the
port file in its own `/dev/shm`; a data-sharing writer still sends heartbeats), so the pair
stays `NONE`. Only a proven delivery adds
`shm-ipc-namespace-split-but-delivered`, since it contradicts the split. So do non-SHM
packets during the observation when both endpoints announce SHM: Fast DDS sends same-host
traffic between them over SHM only, and the pair gets
`shm-ipc-namespace-split-but-non-shm-traffic` ([#111](https://github.com/atinfinity/fastdds_transport_viz/issues/111)).
Both warnings keep the verdict and ask for a report. A data-sharing pair without SHM on one
side is not flagged: the statistics are per participant, and the other endpoints of the two
nodes legitimately talk over UDP. The statistics
themselves are not lost across IPC namespaces: the tool's statistics readers announce the
participant's UDP (or TCP) locators but no SHM locator, so a same-host writer sends its
statistics over the network stack wherever the tool runs ([#106](https://github.com/atinfinity/fastdds_transport_viz/issues/106)).

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

## Comparing two snapshots

A common workflow is *change a profile or an environment variable, run again, see what
moved*. `transport_viz diff before.json after.json` (`ros2 transport diff`) runs the
comparison of watch mode on two saved `--json` documents, without a DDS participant:

```
ros2 transport list --json > before.json
# change the XML profile / FASTDDS_BUILTIN_TRANSPORTS / ..., restart the nodes
ros2 transport list --json > after.json
ros2 transport diff before.json after.json
```

The after snapshot is printed as a table with the mark column of `--watch`: `+` for a pair
that appeared, `~` for one whose transport, confidence, measured transport, selected
locator, measured locators or warnings changed, `-` for a pair that disappeared (a dimmed
ghost row with the labels it had before; a topic that disappeared entirely gets a ghost
topic row). The `changes:` summary line follows the table, and the view options of a
one-shot run apply to both documents before the comparison: `--topic`, `--node`, `--all`
(without it services and raw DDS topics are left out, as when observing), `-v`,
`--explain`, `--locators`, `--advise`, `--color`. `--changes-only` keeps only the topics with
a marked or removed pair.

**Matching pairs.** `--watch` matches a pair from one frame to the next by
`(topic, writer GUID, reader GUID)`. Between two runs the nodes are usually restarted, and
every restart gives their endpoints new GUIDs, so that key would report every pair as
removed and added again. `diff` therefore matches by `(topic, writer node, reader node)`
by default (`--key node`): a node with several writers or readers on one topic has them
matched in GUID order, and an endpoint without a ROS node name is matched by its GUID. For
the same reason the node key ignores the port numbers of the selected and measured
locators (a restart renumbers them: 7413, 7415, ... by participant id); their kinds and
addresses still count. `--key guid` gives the exact `--watch` semantics for two frames of
the same run.

**Exit status** follows `diff(1)`: 0 when nothing changed, 1 when something did, 2 on a
usage error, an unreadable file, a document of another `schema_version` or an invalid
one. A domain mismatch between the two documents is only a warning. One of the two
documents may be `-` (stdin); a JSON Lines file written by `--watch --json` counts by its
last document (a note on stderr says so).

**JSON.** With `--json` the output is the after document with the `changes` object of
`--watch --json` added: `added_pairs`, `removed_pairs` and `changed_pairs` (each pair key
carries `topic`, the GUIDs and `writer_node` / `reader_node`; `changed_pairs[].from`
also names the GUIDs the pair had in the before document), plus `key` (`node` or `guid`)
and `before` (`observed_at` and `domain` of the before document). `--changes-only` prunes
`topics` the same way as the table. The document validates against the schema, so
`jq .changes` extracts the bare comparison and the [web viewer](web-viewer.md#comparing-two-documents)
highlights it (it can also compare two documents itself).
