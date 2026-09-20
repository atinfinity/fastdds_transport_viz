# Measured transports (`--stats`)

Discovery data tells you what *should* happen. With `--stats` the tool also subscribes to
the [Fast DDS statistics module](https://fast-dds.docs.eprosima.com/en/2.14.x/fastdds/statistics/statistics.html)
topics and shows what *did* happen:

| Topic | Used for |
|---|---|
| `_fastdds_statistics_rtps_sent` | RTPS packets/bytes sent by each participant to each destination locator. Matched against the locators the reader announced, this gives the locator kind that actually carried packets (`measured=SHM 47pkt`) and the locators themselves (`--locators`, JSON `measured.locators[]`). A disagreement with the prediction is flagged `!measured-transport-mismatch` for the kind, and `!measured-locator-mismatch` when the kind agrees but the locator the prediction selected carried nothing at all. |
| `_fastdds_statistics_history2history_latency` | Write-to-notification latency of each writer → reader pair, shown as `LATENCY` (mean and max over the observation; JSON `measured.latency_s`, topic `latency_s` = slowest pair), the delivered rate `HZ` (one sample per delivered sample, counted per pair; JSON `measured.delivered_per_s`, see [below](#the-hz-column-delivered-samples-per-second)) and, by its mere presence, the proof that samples reached that reader (used to confirm zero-copy data-sharing, which leaves no RTPS trace). Across hosts it includes the clock offset. |
| `_fastdds_statistics_physical_data` | Host name, user and process id per participant, shown instead of `local` / `host:<id>`. |
| `_fastdds_statistics_rtps_lost` | RTPS packets a participant missed (sequence-number gaps), per sending participant and per its own locator the sender addressed. Published by the receiving participant: the loss of a pair is what the reader's participant reports from the writer's participant on the reader's unicast locators. It gives the `lost` part of the `LOSS` column and the warning `rtps-packets-lost` (see [RTPS_LOST](#rtps_lost) for what it covers). |
| `_fastdds_statistics_resent_datas`, `_fastdds_statistics_heartbeat_count`, `_fastdds_statistics_gap_count` | Per writer: DATA submessages resent, HEARTBEATs and GAPs sent. `resent` is the other part of the `LOSS` column; all three are in JSON `measured.reliability`. |
| `_fastdds_statistics_acknack_count`, `_fastdds_statistics_nackfrag_count` | Per reader: ACKNACKs and NACKFRAGs sent (how often the reader asked for missing data or fragments); JSON `measured.reliability`. |
| `_fastdds_statistics_data_count` | DATA/DATA_FRAG submessages each writer sent through a transport. Zero-copy delivery does not touch it, so a growing count settles whether data-sharing was really used (see [data-sharing.md](data-sharing.md#confidence)). |

## The `HZ` column: delivered samples per second

`HZ` is the rate at which samples of a writer reached a reader, counted by the tool from the
`HISTORY_LATENCY` samples of the pair ([#143](https://github.com/atinfinity/fastdds_transport_viz/issues/143)): Fast DDS publishes one for every sample accepted
into the reader's history, on every delivery path (RTPS over SHM or UDP, intraprocess inside
one participant, zero-copy data-sharing), and the tool counts them per writer → reader pair.
With `n` samples whose source timestamps span `t`, the rate is `(n − 1) / t`; below two samples
there is none and the cell stays blank, as it does without `--stats`. The column is on the
pair rows only: a topic has no single rate to show, and a writer's rate is the same number on
each of its reader rows, never their sum. Values from 100 are printed whole, below with one
decimal (`120`, `9.9`). JSON: `measured.delivered_per_s` (`null` without one),
`delivered_per_s_lower_bound` and `delivered_per_s_window_s`.

The window is the whole observation one-shot (`delivered_per_s_window_s` =
`observation_seconds`) and the last 5 s of source timestamps under `--watch`, so a frame
shows the current rate rather than the average since the start.

This is a delivered rate, not a publish rate: a sample the reader's history refused, or one
the writer never sent because nothing matched, is not in it. What it counts is bounded by what
the tool's own `HISTORY_LATENCY` reader can hold between two drains (100 per instance every
50 ms, see [Reader QoS](#reader-qos)): 1000 samples/s per pair are counted to within 0.1 %,
verified for SHM between two processes, intraprocess and data-sharing pairs at 10, 100 and
1000 Hz on Jazzy and Lyrical (`scripts/integration_test.sh rate_stats`, tolerance ±3 %, see
[development.md](development.md#verification-log)). When samples of that topic were lost on
the way to the tool, the rate is a lower bound and is printed as `≥120`
(`delivered_per_s_lower_bound: true`). The statistics writer that publishes a pair's
`HISTORY_LATENCY` belongs to the reader's participant and numbers the samples of all its pairs
in one sequence, so a gap can be attributed to that participant, never to a pair: every pair
whose reader lives there gets the `≥`. A sample overwritten inside the tool's reader before
the drain leaves that same gap and nothing else - `stats.samples_lost_latency` does not count
it, since Fast DDS reports no lost sample for an overwrite in a best-effort reader.

`_fastdds_statistics_publication_throughput` is still not subscribed
([#137](https://github.com/atinfinity/fastdds_transport_viz/issues/137)): it looks like a rate and is not one - Fast DDS publishes one sample
per `write()` whose value is *that sample's* payload divided by the interval since the same
writer's previous `write()`, which says how fast the writer was during one inter-write
interval and never how much a topic carries per second. `measured.throughput_bytes_per_s`,
`topics[].throughput_bytes_per_s` and `stats.throughput` stay in the JSON so that documents
written earlier keep validating, fixed to `null`, `null` and `{}`.

The JSON also carries cumulative counters and the length of the observation, so a rate on the
wire is well defined too:

| Rate | Take | Divide by |
|---|---|---|
| DATA submessages per second of a writer | `stats.data_count[<writer guid>].last` − `.first` | `observation_seconds` |
| RTPS packets or bytes per second a participant sent to a locator | `stats.traffic[].packets` − `.packets_first`, `.bytes` − `.bytes_first` | `observation_seconds` |

`observation_seconds` is how long the tool observed (cumulative under `--watch`), and `first`
is the first value the tool saw rather than zero, so the difference is what happened during
the observation. `DATA_COUNT` does not move for intraprocess or data-sharing delivery (that is
exactly why it confirms data-sharing, see [data-sharing.md](data-sharing.md#confidence)); it
counts once per destination locator and again per fragment and per retransmission; and
`RTPS_SENT` counts whole RTPS packets, headers included.

## Enabling statistics on the observed nodes

No code change is needed; Fast DDS reads an environment variable when the participant is
created:

```
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
```

A pair judged `qos-incompatible` is not measured; if `HISTORY_LATENCY` nevertheless proves
delivery, the warning `qos-incompatible-but-delivered` flags a gap in the matching rules.
A pair judged `type-name-mismatch` is treated the same way, with
`type-name-mismatch-but-delivered` ([#85](https://github.com/atinfinity/fastdds_transport_viz/issues/85)).
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
rather than losing its measured transport. When `HISTORY_LATENCY` proved delivery during
that observation the pair is not idle, its `RTPS_SENT` samples did not arrive: it shows
`measured=SHM (unmeasured, delivered)` with `!delivered-without-measured-traffic` instead
([#149](https://github.com/atinfinity/fastdds_transport_viz/issues/149)). Other values of
the cell: `n/a` (the writer's participant publishes no statistics), `none` (statistics, but
no packet to any locator of the reader) and `none(delivered)` (the same, while
`HISTORY_LATENCY` proved delivery).
A participant has statistics when the tool received a statistics sample it published:
that set is `stats.participants_with_stats` in the JSON and the N of the footer's
"statistics from N participant(s)". A participant only named in another one's sample, such
as the remote writer of a reader's `HISTORY_LATENCY` report, does not count.

## Granularity

Statistics are per *participant* (one per ROS node), so a measurement applies to the
writer's node → reader's node link. The prediction from discovery is what tells the
individual pairs apart. `--stats` observes for at least 5 s so that counters can accumulate,
and goes on until discovery has been quiet for `--quiet` seconds, every `RTPS_SENT` writer
(one per participant with statistics) the tool's reader matched has delivered a first sample,
*and* the number of `RTPS_SENT` entries with measured packets towards a discovered reader's
unicast port has stopped growing for `--quiet` seconds (at least 3 s) - or until `--timeout`
(default 30 s with `--stats`), whichever comes first. Only entries to a reader port count
([#179](https://github.com/atinfinity/fastdds_transport_viz/issues/179)): the entries to
multicast metatraffic and to the tool's own port move within seconds of any participant, and
a run that counted them settled at 8 s with 47 such entries and no pair measured. The last two conditions are what a fixed window cut short
([#168](https://github.com/atinfinity/fastdds_transport_viz/issues/168)): a transient-local
counter writer hands its whole history to a late-joining reader as one batch, the writers do
it one process at a time with pauses of up to 2 s in between, and at 20 processes with 100
pairs each the last writer was first heard from after 16-20 s and the entries kept coming
until about 25 s, so a 5 s run measured a fraction of the pairs on one run and none on the
next. `--json` records the rule in `stats.writers_announced` (`RTPS_SENT` writers matched),
`stats.writers_heard`, `stats.measured_instances` (entries measured towards a reader port),
`stats.settled` and `stats.settled_at_s` (`null` when the run hit `--timeout` first, in which
case one stderr line names the writers still not heard from, or says that no entry towards a
reader was measured), and
`discovery.stopped_on` reads `settled`. `--watch --stats` does not wait for that rule: its
first frame comes once discovery is quiet and 5 s have passed (`--timeout` still caps the
wait), and the later frames carry what the handoff brings
([#177](https://github.com/atinfinity/fastdds_transport_viz/issues/177)). Idle topics show
`!no-traffic-observed`. When `HISTORY_LATENCY` proves delivery but `RTPS_SENT` has no entry
for any of the reader's locators, or only entries from before the observation
(`measured=SHM (unmeasured, delivered)`), the warning is
`!delivered-without-measured-traffic` instead: the samples arrived, the statistics just did
not attribute the packets (seen on
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
- Multicast destinations are left out, and [#130](https://github.com/atinfinity/fastdds_transport_viz/issues/130)
  measured why. Fast DDS writes the sequence number inside the transport's per-socket
  `send()`, from a counter kept per destination locator, and one multicast send goes out on
  the any-address socket as well as on one socket per interface. A sender with N interfaces
  therefore spends N+1 numbers on a message a receiver elsewhere sees once, and reports the
  N it never had as lost: measured `RTPS_LOST` / `RTPS_SENT` of 0.91-1.03 on one interface,
  1.63-2.00 on two and 2.29-2.98 on three over twelve readings each on Fast DDS 2.14.6 and
  3.6.2, against 0.00 in every reading when sender and receiver share a network namespace
  and every copy arrives. Nothing is lost on the wire.
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
profile below for the instance limit - they need it for another reason, see
[the next pitfall](#pitfall-stalled-counters-on-fast-dds-36) - and a tool built with 3.5 or
later never shows the warning. A pair without traffic to its reader gets `delivered-without-measured-traffic` or
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

## Pitfall: stalled counters on Fast DDS 3.6

On Fast DDS 3.6 (Lyrical, Rolling) the statistics DataWriters of the counters deliver almost
only on their periodic heartbeat, which defaults to 3 s: the samples reach a reader in bursts
with stalls of many seconds, none is reported lost, and with many `RTPS_SENT` instances most
of them have not arrived after 30 s. Every pair then reads `measured=none(delivered)` with
`delivered-without-measured-traffic`, already at 20 processes and 500 topics
([#152](https://github.com/atinfinity/fastdds_transport_viz/issues/152); Jazzy's 2.14 is not
affected). A reader cannot ask for heartbeats, so the tool cannot fix this on its side.

The shipped profiles set `heartbeat_period` to 500 ms on the counter writers (every profile
but `HISTORY_LATENCY_TOPIC`) when the package is built with Fast DDS 3.x, which restores
continuous delivery. Fast DDS 2.x spells the element `heartbeatPeriod` and drops a whole
profile it cannot parse, so CMake generates the installed `statistics.xml` and
`datasharing_auto_stats.xml` from `config/*.xml.in` for the Fast DDS of the build (the writer
profiles themselves live once in `config/statistics_writers.xml.in`): use the
installed files, on the machine of the observed nodes. 500 ms is the longest of 100 ms,
250 ms, 500 ms and 1 s that kept the `--watch` coverage at 1.0 on the medium rung (1 s reads
0.21). Use the profile on the observed nodes of every distro, with the two lines of
[the previous section](#pitfall-the-10-instance-limit).

## What the shipped profiles change on the writers

A profile named after a statistics alias replaces the writer's QoS **entirely**: Fast DDS
does not merge it with the QoS it builds itself (`DataWriterQos.cpp` of the statistics
module, the same on 2.14 and 3.6). What Fast DDS builds is reliable, transient-local,
keep-last 10, asynchronous on `FastDDSStatisticsFlowControllerDefault` - a sender thread of
its own, so the statistics never queue behind user data on an asynchronous writer - and the
property `fastdds.push_mode=false`, *pull mode*: a reliable remote reader is sent nothing
until it answers a heartbeat with an ACKNACK, so the samples flow at the heartbeat period.

The shipped profiles keep everything but the property
([#154](https://github.com/atinfinity/fastdds_transport_viz/issues/154)): the writers name
the statistics flow controller and run in **push mode**, sending each sample as it is
written. `HISTORY_LATENCY` alone is keep-last **100** instead of 10
([#170](https://github.com/atinfinity/fastdds_transport_viz/issues/170)): it emits one sample
per delivered message, and with 10 a sender thread that falls 10 ms behind at 1000 Hz
overwrites unsent samples, which the tool then reports as a lower bound on the rate; 100 is
the depth of the tool's own reader, and the periodic counters keep 10. Pull mode was measured on the medium rung (20 processes, 500 topics) and rejected:
on Jazzy, whose heartbeat stays at the 3 s default (no `heartbeat_period` on 2.x, see above),
the tool's `--watch` saw no `HISTORY_LATENCY` proof at all over 60 s and 2000-3300 counter
samples were reported lost per run - keep-last 10 overwrites an instance's samples before
the reader gets to ask for them; on Lyrical, with the 500 ms heartbeat, pull mode measured
every pair and cost the tool a third less CPU (0.49 instead of 0.7 cores), but one delivery
mechanism for both distros was preferred over a second version split. The numbers are in
[development.md](development.md#verification-results).

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
| `_fastdds_statistics_history2history_latency` | best-effort | volatile | 100 | by far the loudest topic, nothing it carries is cumulative, and it is the only one that never reads `first`; its samples are counted per pair for `HZ`, and what fits between two drains bounds the rate the tool can count: 10 saturated near 200 samples/s per pair, 100 counts 1000/s ([#143](https://github.com/atinfinity/fastdds_transport_viz/issues/143)) |

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

- **Up to about 10 processes and 500 pairs** everything keeps up: every pair is measured within 5 s, and the default `--stats` one-shot ends a few seconds after that.
- **At 20 processes and 2400 pairs** the default `--stats` one-shot settles after 17-23 s and measures 95-100 % of the pairs, and the one-shot table shows all of them. A 5 s run measured none ([#168](https://github.com/atinfinity/fastdds_transport_viz/issues/168): the statistics writers hand their history to the tool's readers one process at a time), and before [#141](https://github.com/atinfinity/fastdds_transport_viz/issues/141) a quarter of the pairs had no measurement and the table showed one.
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
number the pair still shows, and turns the `HZ` of every pair read by the participant that lost
them into a lower bound (`≥`, see [above](#the-hz-column-delivered-samples-per-second)). The
table names them apart (`..., 42 sample(s) lost, 1000 latency sample(s) lost`), the web viewer
does too, and the `stats-samples-lost` warning leaves them out.

When the warning is there, `no-traffic-observed` on a pair can also mean "not measured": the
counters are shared by all ten statistics readers, so a loss cannot be attributed to one
pair. Narrow the view instead:
- enable statistics only on the nodes you care about;
- keep `FASTDDS_STATISTICS` to the aliases you need, for example `RTPS_SENT_TOPIC;RTPS_LOST_TOPIC`.

A longer `--timeout` does not reduce the loss - it collects more of it - but it does give every
instance more chances to be sampled twice, which is exactly what the coverage figures above
compare: what the default one-shot measured against what is measured at 30 s.

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
