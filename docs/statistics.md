# Measured transports (`--stats`)

Discovery data tells you what *should* happen. With `--stats` the tool also subscribes to
the [Fast DDS statistics module](https://fast-dds.docs.eprosima.com/en/2.14.x/fastdds/statistics/statistics.html)
topics and shows what *did* happen:

| Topic | Used for |
|---|---|
| `_fastdds_statistics_rtps_sent` | RTPS packets/bytes sent by each participant to each destination locator. Matched against the locators the reader announced, this gives the locator kind that actually carried packets (`measured=SHM 47pkt`). A disagreement with the prediction is flagged `!measured-transport-mismatch`. |
| `_fastdds_statistics_history2history_latency` | Write-to-notification latency of each writer → reader pair, shown as `LATENCY` (mean and max over the observation; JSON `measured.latency_s`, topic `latency_s` = slowest pair) and, by its mere presence, the proof that samples reached that reader (used to confirm zero-copy data-sharing, which leaves no RTPS trace). Across hosts it includes the clock offset. |
| `_fastdds_statistics_physical_data` | Host name, user and process id per participant, shown instead of `local` / `host:<id>`. |
| `_fastdds_statistics_publication_throughput` | Payload bytes per second of each writer; shown as `RATE` (per topic: sum of its writers) and, in JSON, `measured.throughput_bytes_per_s` per pair and `topics[].throughput_bytes_per_s` per topic. Independent of the transport, so it also quantifies zero-copy data-sharing. |
| `_fastdds_statistics_rtps_lost` | RTPS packets the reader's participant missed from each source locator (sequence-number gaps). Matched to the writer's locators like `RTPS_SENT`, it gives the `lost` part of the `LOSS` column and the warning `rtps-packets-lost`. |
| `_fastdds_statistics_resent_datas`, `_fastdds_statistics_heartbeat_count`, `_fastdds_statistics_gap_count` | Per writer: DATA submessages resent, HEARTBEATs and GAPs sent. `resent` is the other part of the `LOSS` column; all three are in JSON `measured.reliability`. |
| `_fastdds_statistics_acknack_count`, `_fastdds_statistics_nackfrag_count` | Per reader: ACKNACKs and NACKFRAGs sent (how often the reader asked for missing data or fragments); JSON `measured.reliability`. |
| `_fastdds_statistics_data_count` | DATA/DATA_FRAG submessages each writer sent through a transport. Zero-copy delivery does not touch it, so a growing count settles whether data-sharing was really used (see [data-sharing.md](data-sharing.md#confidence)). |

## Enabling statistics on the observed nodes

No code change is needed; Fast DDS reads an environment variable when the participant is
created:

```
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
```

A pair judged `qos-incompatible` is not measured; if `HISTORY_LATENCY` nevertheless proves
delivery, the warning `qos-incompatible-but-delivered` flags a gap in the matching rules.
Pairs whose *writer* was started without it are reported with the warning
`stats-not-enabled-on-writer` (a reader without statistics is not flagged).

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

## Pitfall: the 10-instance limit

Fast DDS 2.14 creates the statistics DataWriters with the default resource limit of
10 instances. `RTPS_SENT` is keyed by destination locator, so a node that talks to more
than 10 locators (a handful of peers is enough: every peer has metatraffic, user-data and
SHM locators) silently stops reporting the extra ones. The tool flags this as
`!stats-writer-instance-limit-suspected`.

Lift the limit on the observed nodes with the shipped profile. Fast DDS applies a
`data_writer` profile whose name is the alias passed in `FASTDDS_STATISTICS`; the file has
one for every keyed topic (`PHYSICAL_DATA` has a single instance and needs none):

```
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/statistics.xml
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;RTPS_LOST_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC;RESENT_DATAS_TOPIC;HEARTBEAT_COUNT_TOPIC;ACKNACK_COUNT_TOPIC;NACKFRAG_COUNT_TOPIC;GAP_COUNT_TOPIC"
```

Fast DDS reads a single profiles file; `datasharing_auto_stats.xml` is the merge of this
file with `datasharing_auto.xml` for observing data-sharing with `--stats`. (The tool's
own statistics readers already use unlimited instances.)

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
  Jazzy) and `.../fastdds_statistics_types_v3/` (Fast DDS 3.2.4, Kilted / Rolling);
  CMake picks one by the Fast DDS major version. Replace the matching directory when
  targeting another Fast DDS version.
