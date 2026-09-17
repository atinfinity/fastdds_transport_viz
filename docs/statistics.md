# Measured transports (`--stats`)

Discovery data tells you what *should* happen. With `--stats` the tool also subscribes to
the [Fast DDS statistics module](https://fast-dds.docs.eprosima.com/en/2.14.x/fastdds/statistics/statistics.html)
topics and shows what *did* happen:

| Topic | Used for |
|---|---|
| `_fastdds_statistics_rtps_sent` | RTPS packets/bytes sent by each participant to each destination locator. Matched against the locators the reader announced, this gives the locator kind that actually carried packets (`measured=SHM 47pkt`) and the locators themselves (`--locators`, JSON `measured.locators[]`). A disagreement with the prediction is flagged `!measured-transport-mismatch` for the kind, and `!measured-locator-mismatch` when the kind agrees but the locator the prediction selected carried nothing at all. |
| `_fastdds_statistics_history2history_latency` | Write-to-notification latency of each writer → reader pair, shown as `LATENCY` (mean and max over the observation; JSON `measured.latency_s`, topic `latency_s` = slowest pair) and, by its mere presence, the proof that samples reached that reader (used to confirm zero-copy data-sharing, which leaves no RTPS trace). Across hosts it includes the clock offset. |
| `_fastdds_statistics_physical_data` | Host name, user and process id per participant, shown instead of `local` / `host:<id>`. |
| `_fastdds_statistics_rtps_lost` | RTPS packets a participant missed (sequence-number gaps), per sending participant and per its own locator the sender addressed. Published by the receiving participant: the loss of a pair is what the reader's participant reports from the writer's participant on the reader's unicast locators. It gives the `lost` part of the `LOSS` column and the warning `rtps-packets-lost` (see [RTPS_LOST](#rtps_lost) for what it covers). |
| `_fastdds_statistics_resent_datas`, `_fastdds_statistics_heartbeat_count`, `_fastdds_statistics_gap_count` | Per writer: DATA submessages resent, HEARTBEATs and GAPs sent. `resent` is the other part of the `LOSS` column; all three are in JSON `measured.reliability`. |
| `_fastdds_statistics_acknack_count`, `_fastdds_statistics_nackfrag_count` | Per reader: ACKNACKs and NACKFRAGs sent (how often the reader asked for missing data or fragments); JSON `measured.reliability`. |
| `_fastdds_statistics_data_count` | DATA/DATA_FRAG submessages each writer sent through a transport. Zero-copy delivery does not touch it, so a growing count settles whether data-sharing was really used (see [data-sharing.md](data-sharing.md#confidence)). |

## No rate column, and what to do instead

`_fastdds_statistics_publication_throughput` is deliberately missing from the list above: the
tool does not subscribe to it and reports no publish rate ([#137](https://github.com/atinfinity/fastdds_transport_viz/issues/137)). The statistic
looks like a rate and is not one - Fast DDS publishes one sample per `write()` whose value is
*that sample's* payload divided by the interval since the same writer's previous `write()`. It
says how fast the writer was during one inter-write interval, never how much a topic carries
per second, and for a writer that bursts and then falls silent the two differ by orders of
magnitude. Nothing downstream can repair it: the tool's statistics readers keep one
sample per instance of a counter topic and are drained every 50 ms, so everything but the
newest value is gone before the tool can see it.

`measured.throughput_bytes_per_s`, `topics[].throughput_bytes_per_s` and `stats.throughput`
stay in the JSON so that documents written earlier keep validating, fixed to `null`, `null`
and `{}`.

What the JSON does carry is cumulative counters and the length of the observation, so a rate
you compute yourself is well defined:

| Rate | Take | Divide by |
|---|---|---|
| DATA submessages per second of a writer | `stats.data_count[<writer guid>].last` − `.first` | `observation_seconds` |
| RTPS packets or bytes per second a participant sent to a locator | `stats.traffic[].packets` − `.packets_first`, `.bytes` − `.bytes_first` | `observation_seconds` |

`observation_seconds` is how long the tool observed (cumulative under `--watch`), and `first`
is the first value the tool saw rather than zero, so the difference is what happened during
the observation.

This is a rate on the wire, not an application publish rate. `DATA_COUNT` does not move for
intraprocess or data-sharing delivery (that is exactly why it confirms data-sharing, see
[data-sharing.md](data-sharing.md#confidence)); it counts once per destination locator and
again per fragment and per retransmission; and `RTPS_SENT` counts whole RTPS packets, headers
included. [#143](https://github.com/atinfinity/fastdds_transport_viz/issues/143) tracks a rate the tool could show itself.

## Enabling statistics on the observed nodes

No code change is needed; Fast DDS reads an environment variable when the participant is
created:

```
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
```

A pair judged `qos-incompatible` is not measured; if `HISTORY_LATENCY` nevertheless proves
delivery, the warning `qos-incompatible-but-delivered` flags a gap in the matching rules.
A pair with `shm-ipc-namespace-split` stays `NONE` too, although the writer's SHM traffic
(for a data-sharing pair, its heartbeats) is measured; a proven delivery adds `shm-ipc-namespace-split-but-delivered`, and
non-SHM packets during the observation between endpoints that both announce SHM add
`shm-ipc-namespace-split-but-non-shm-traffic` (see
[Split IPC namespaces](how-it-works.md#split-ipc-namespaces)).
Pairs whose *writer* was started without it are reported with the warning
`stats-not-enabled-on-writer` (a reader without statistics is not flagged). The tool's
statistics readers announce UDP (or TCP) locators but no SHM locator, so a same-host
writer in another IPC namespace still gets its statistics through. A writer whose
participant has the SHM transport only shares no transport with them and shows the same
warning.

## What the counters cover

`RTPS_SENT` counters are cumulative since the writer's participant started. The tool
polls the statistics readers during the whole observation and reports the *difference*
between the first and the last sample as `packets` / `bytes` (`measured=SHM 148pkt
7.63 MB`); the cumulative values are kept as `packets_total` / `bytes_total` in JSON.
The transport kinds in `measured` are taken from every packet ever reported, so a pair
that was active before but silent during the observation shows `measured=SHM (idle)`
rather than losing its measured transport. Other values of the cell: `n/a` (the writer's
participant publishes no statistics), `none` (statistics, but no packet to any locator of
the reader) and `none(delivered)` (the same, while `HISTORY_LATENCY` proved delivery).
A participant has statistics when the tool received a statistics sample it published:
that set is `stats.participants_with_stats` in the JSON and the N of the footer's
"statistics from N participant(s)". A participant only named in another one's sample, such
as the remote writer of a reader's `HISTORY_LATENCY` report, does not count.

## Granularity

Statistics are per *participant* (one per ROS node), so a measurement applies to the
writer's node → reader's node link. The prediction from discovery is what tells the
individual pairs apart. `--stats` observes for the full `--timeout` (default 5 s, the
quiet-period early exit is disabled) so that counters can accumulate; idle topics show
`!no-traffic-observed`. When `HISTORY_LATENCY` proves delivery but `RTPS_SENT` has no entry
for any of the reader's locators, the warning is `!delivered-without-measured-traffic`
instead: the samples arrived, the statistics just did not attribute the packets (seen on
slow machines with 2 MB samples over SHM and the default 512 KB segment; a larger
`segment_size` in the SHM transport descriptor helps).

The per-entity counters (`HISTORY_LATENCY`, `DATA_COUNT`, `RESENT_DATAS`,
`HEARTBEAT_COUNT`, `GAP_COUNT`, `ACKNACK_COUNT`, `NACKFRAG_COUNT`) of a writer or reader
include those of its native-buffer
companion on `<topic>/_buf_cpu` (`rmw_fastrtps_cpp` on Lyrical and later), which carries
the samples of types with an unbounded `uint8[]` field; see
[Native-buffer companion topics](how-it-works.md#native-buffer-companion-topics).

## RTPS_LOST

`RTPS_LOST` is published by the *receiving* participant when it sees a gap in the sequence
numbers a sending participant stamps on its RTPS packets. Fast DDS numbers the packets per
sending participant and per destination locator, so an entry (JSON `stats.lost[]`) names
the reporter (`reporter_participant_guid_prefix`), the sender
(`src_participant_guid_prefix`) and the reporter's own locator the sender addressed
(`dst_locator`). The `lost` of a pair is what the reader's participant reports from the
writer's participant on the reader's unicast locators, as the difference to the first
sample of the observation.

- It belongs to the participant pair, not to the writer: every packet between the two
  counts (other topics, heartbeats, discovery traffic to those locators), and every pair
  between the same two participants shows the same number. The topic's `lost_packets`
  counts each entry once.
- Multicast destinations are left out: Fast DDS may number one multicast send once per
  socket, which a remote receiver would report as lost packets.
- A packet that arrives late lowers the count again; a window that ends below its first
  sample shows 0.
- Only UDP and TCP packets carry the numbers: SHM and data-sharing never report a loss.
- The sender needs no runtime setting, only a Fast DDS built with statistics. The reader's
  participant needs `RTPS_LOST_TOPIC` in `FASTDDS_STATISTICS`; without it the loss is
  unknown: `- lost` in the `LOSS` column and `lost_packets: null` in JSON, while the other
  reliability counters stay.

## Pitfall: the 10-instance limit

Before 3.5, Fast DDS creates the statistics DataWriters with the default resource limit of
10 instances (Jazzy's 2.14; Humble's 2.6 binary has no statistics module). `RTPS_SENT` is
keyed by destination locator, so a node that talks to more than 10 locators (a handful of
peers is enough: every peer has metatraffic, user-data and SHM locators) silently stops
reporting the extra ones. The tool flags this as `!stats-writer-instance-limit-suspected`.

Fast DDS 3.5 made the limit unlimited by default: Lyrical and Rolling (3.6) do not need the
profile below (it does no harm there), and a tool built with 3.5 or later never shows the
warning. A pair without traffic to its reader gets `delivered-without-measured-traffic` or
`no-traffic-observed` instead. The tool goes by the Fast DDS it is built with, so it misses
the limit when a Lyrical build observes Jazzy nodes, or when a profile of your own sets
`max_instances` again.

Lift the limit on the observed nodes with the shipped profile. Fast DDS applies a
`data_writer` profile whose name is the alias passed in `FASTDDS_STATISTICS`; the file has
one for every keyed topic (`PHYSICAL_DATA` has a single instance and needs none):

```
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/statistics.xml
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
```

Fast DDS 2.x reads only `FASTRTPS_DEFAULT_PROFILES_FILE` and Fast DDS 3.x only
`FASTDDS_DEFAULT_PROFILES_FILE`. On Lyrical and Rolling, rmw_fastrtps reads
`FASTRTPS_DEFAULT_PROFILES_FILE` as well (with a deprecation warning), so the line above works
for ROS 2 nodes on every supported distro; a Fast DDS 3.x application that does not go
through the rmw needs `FASTDDS_DEFAULT_PROFILES_FILE`.

Fast DDS reads a single profiles file; `datasharing_auto_stats.xml` is the merge of this
file with `datasharing_auto.xml` for observing data-sharing with `--stats`. (The tool's
own statistics readers already use unlimited instances.)

## Reader QoS

The tool's ten statistics readers took the Fast DDS default for these topics until
[#141](https://github.com/atinfinity/fastdds_transport_viz/issues/141): reliable,
transient-local, keep-last 1 and - less visibly - at most ten instances. The instance limit is
the one this page warns about on the writer side, and it applies just as much to a reader: the
statistics topics are keyed, so a reader stops receiving a participant's samples entirely once
ten `(source, locator)` instances exist. The readers now leave the instance count unlimited,
and differ per topic in what else they ask for:

| Topic | Reliability | Durability | Depth | Why |
|---|---|---|---|---|
| the eight counter topics (`rtps_sent`, `rtps_lost`, `data_count`, `resent_datas`, `heartbeat_count`, `acknack_count`, `nackfrag_count`, `gap_count`) | reliable | transient-local | 1 | the tool reports `last - first`, and `first` is the sample from before the observation began: losing it costs the measurement of a whole entity, not one sample |
| `_fastdds_statistics_physical_data` | reliable | transient-local | 1 | one sample per participant, published once at discovery; a second one would say the same thing |
| `_fastdds_statistics_history2history_latency` | best-effort | volatile | 10 | by far the loudest topic, nothing it carries is cumulative, and it is the only one that never reads `first` |

One sample per instance is enough for a cumulative counter: only its newest value says anything
the tool needs, and the observer's own thread takes it within 50 ms of its arrival. Keeping ten
was measured and rejected - it bought no extra measured pair and cost a fifth of a core and
twice the `--watch` frame p95.

`max_samples` stays unlimited and only `max_samples_per_instance` is bounded. A total cap would
start refusing samples once enough instances exist, and a refusal is counted as
`samples_rejected` - exactly the loss the limit is meant to prevent.

Receiving `HISTORY_LATENCY` reliably is not affordable either: measured at 20 processes and
2400 pairs, its acknacks and retransmissions took the tool below the coverage it had before
#141 (0.746 against 0.947) and its `--watch` frame p95 to 20 s. Best-effort makes the losses on
that topic visible instead - see `stats.samples_lost_latency` below - which is a report, not
a regression.

The readers are drained by a thread the observer owns, every 50 ms, in both modes and while the
display is paused - not once per `--interval` as before. What bounds the loss on these topics is
the time between two takes, so the cadence matters more than the depth.

## Large systems

Measured on an 8-CPU Docker VM with Jazzy (Fast DDS 2.14.6), statistics on every node, the
tool next to the nodes (details: [development.md](development.md#scale-results)):

- **Up to about 10 processes and 500 pairs** everything keeps up: every pair is measured within the default 5 s.
- **At 20 processes and 2400 pairs** every pair is measured within 5 s as well, and the one-shot table shows all of them. Before [#141](https://github.com/atinfinity/fastdds_transport_viz/issues/141) a quarter of them had no measurement after 5 s and the table showed one.
- **At 40 processes and 5600 pairs** the tool measures most or all of the pairs: the coverage at 5 s was 0.62, 0.97 and 1.0 over three runs of the same build, where before #141 it was 0.0 in all three. The spread is the host rather than the tool - at this size the load alone takes 6.7 to 7.5 of the 8 cores before the tool starts - and a `--watch --stats` frame still takes 1.6 s here (0.44 s without `--stats`; at 20 processes 0.15 s, within the 250 ms budget since [#135](https://github.com/atinfinity/fastdds_transport_viz/issues/135)).

The tool receives all statistics over UDP on one Fast DDS receive thread, and next to Nav2 (4
participants, 1195 pairs) that thread already takes a whole core. What bounds the loss is how
often the tool takes from its readers, which is what the 50 ms drain above is for; past that,
the writers' keep-last history overwrites samples before they arrive.

The tool says what it lost. `stats.samples_lost` in the JSON document counts the statistics
samples that never reached it and the table's `statistics:` line repeats the number. When the
loss also cost a measurement (below), the document carries the warning code
`stats-samples-lost` and a one-shot run adds one line on stderr:

```
warning: 682142 of 690671 statistics samples were lost (the tool could not keep up) and 37 of 2400 pairs with proven deliveries show no measured packet - enable statistics on fewer nodes, or keep FASTDDS_STATISTICS to the aliases you need (e.g. RTPS_SENT_TOPIC;RTPS_LOST_TOPIC)
```

`--watch` does not print the line: the count only grows from frame to frame, and the
`statistics:` footer already carries it. The counters are cumulative for the whole run, so a
frame that loses nothing does not bring back the measurements the earlier ones missed.

Losing samples is not the same as losing measurements: a 40-process run lost 925130 of
1040228 samples and measured every `/scale` pair. The statistics counters are cumulative and
the tool reports `last - first` over the observation window, so a sample it never receives
between two it did changes nothing. A loss costs a measurement only when the `RTPS_SENT`
samples of a pair's locators did not arrive at all, so that there is nothing to subtract. The
instance itself cannot tell: one sample and no difference is also what a locator looks like
that nothing was sent to since the tool started, and at 20 processes that is one in ten of
them in a run that lost no sample. What tells the two apart is a delivery proof. Since
[#147](https://github.com/atinfinity/fastdds_transport_viz/issues/147) the document counts the pairs whose deliveries `HISTORY_LATENCY` proves
(`stats.pairs_delivered`) and those of them without a measured packet
(`stats.pairs_delivered_unmeasured`), and the warning needs both: counter samples lost or
rejected **and** at least one such pair. A loss alone is reported as a number and warns
nobody; an unmeasured pair without a loss keeps its own `delivered-without-measured-traffic`.
Data-sharing pairs, `qos-incompatible` and IPC-split pairs and
`stats-writer-instance-limit-suspected` pairs are in neither number, and a pair without a
delivery proof stays the ambiguity `no-traffic-observed` describes. The numbers follow the
frame, so under `--watch` the warning goes away once the measurements are back.

Not every unmeasured pair is the tool's loss. Since
[#152](https://github.com/atinfinity/fastdds_transport_viz/issues/152) the document also counts
the unmeasured pairs that no lost sample explains (`stats.pairs_delivered_absent`): pairs the
tool never saw an `RTPS_SENT` instance for (`packets_total` 0), and every unmeasured pair of a
run that lost no counter sample. They raise their own document-level warning,
`rtps-sent-absent` (one stderr line in a one-shot run, the `statistics:` footer, the web
viewer), and `stats-samples-lost` counts only the rest, so both can appear in one run. The
usual cause is Fast DDS 3.6 (ROS 2 Lyrical): its statistics writers deliver almost only on
their periodic heartbeat, 3 s by default, so the counters arrive in bursts with stalls of
several seconds and nothing is reported lost. The tool's readers cannot change that. Start the
observed nodes with the installed `config/statistics.xml` of this package
(`FASTDDS_DEFAULT_PROFILES_FILE`, or `FASTRTPS_DEFAULT_PROFILES_FILE`), which shortens that
heartbeat period on Fast DDS 3. The warning judges what it observes, not the Fast DDS version.

`stats.samples_lost_latency` is counted apart, as a part of `stats.samples_lost` rather than
beside it, and nothing warns about it. `HISTORY_LATENCY` is received best-effort by design
(see [Reader QoS](#reader-qos)), so its reader reports every sequence gap in the loudest topic
there is: 1.7 to 1.9 million of them in a 60 s run at 40 processes. Those samples are
independent observations that the tool reduces to a mean and a max, so losing them coarsens a
number the pair still shows. The table names them apart (`..., 42 sample(s) lost, 1000 latency
sample(s) lost`), the web viewer does too, and the `stats-samples-lost` warning leaves them out.

When the warning is there, `no-traffic-observed` on a pair can also mean "not measured": the
counters are shared by all ten statistics readers, so a loss cannot be attributed to one
pair. Narrow the view instead:
- enable statistics only on the nodes you care about;
- keep `FASTDDS_STATISTICS` to the aliases you need, for example `RTPS_SENT_TOPIC;RTPS_LOST_TOPIC`.

A longer `--timeout` does not reduce the loss - it collects more of it - but it does give every
instance more chances to be sampled twice, which is exactly what the coverage figures above
compare: what is measured at 5 s against what is measured at 30 s.

`stats.samples_lost_at_start` is counted apart and never warns. Every reader is told about the
samples a writer's keep-last history had already dropped when it matched, which says nothing
about the tool keeping up - and it is told late, because a writer announces what it dropped with
one of its next heartbeats rather than at the match: measured on a quiet five-node system, the
notifications arrived up to 1.2 seconds (Jazzy) and 3.9 seconds (Lyrical) afterwards. Every new
statistics writer brings such a burst, so a loss counts as late-join while a writer matched
within the last five seconds - and before the first match of all, which the grace period has
nothing to measure from. A node that starts mid-run therefore excuses five seconds of loss,
never the losses that keep coming after it.

## Implementation notes

- The tool removes `FASTDDS_STATISTICS` from its own environment before creating its
  participants: with the variable set, Fast DDS 2.14 adds statistics writers to the
  participant that hosts the tool's statistics readers and can deadlock inside
  `on_rtps_sent()` while a reader sends an acknack. Statistics about the tool itself are
  never needed (its endpoints are filtered out anyway).
- `RTPS_SENT` reports the *participant* GUID as source, and `byte_count` is the plain
  cumulative byte total (`byte_magnitude_order` is only `floor(log10(byte_count))`).
- Fast DDS shows the tool the locators of participants on its own host as `127.0.0.1`
  (its localhost transformation), while a writer on another host reports `RTPS_SENT`
  traffic to that participant's real address. The overlay therefore treats a loopback
  reader locator as equal to any address of the tool's host with the same port; without
  this, a cross-host pair whose reader sits next to the tool showed
  `delivered-without-measured-traffic`.
- The generated type-support code for the statistics topics is vendored (Apache-2.0)
  because the ROS distributions ship the compiled types in the Fast DDS library but
  neither their headers nor `fastddsgen`:
  `src/fastdds_transport_viz/third_party/fastdds_statistics_types/` (Fast DDS 2.14.6,
  Jazzy) and `.../fastdds_statistics_types_v3/` (generated from Fast DDS 3.2.4, used on Lyrical / Rolling);
  CMake picks one by the Fast DDS major version. Replace the matching directory when
  targeting another Fast DDS version.
