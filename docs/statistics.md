# Measured transports (`--stats`)

English | [日本語](statistics.ja.md)

Discovery data tells you what *should* happen. With `--stats` the tool also subscribes to
the [Fast DDS statistics module](https://fast-dds.docs.eprosima.com/en/2.14.x/fastdds/statistics/statistics.html)
topics and shows what *did* happen:

| Topic | Used for |
|---|---|
| `_fastdds_statistics_rtps_sent` | RTPS packets/bytes sent by each participant to each destination locator. Matched against the locators the reader announced, this gives the locator kind that actually carried packets (`measured=SHM 47pkt`). A disagreement with the prediction is flagged `!measured-transport-mismatch`. |
| `_fastdds_statistics_history2history_latency` | Proves that samples from a writer reached a specific reader. Used to confirm zero-copy data-sharing, which leaves no RTPS trace. |
| `_fastdds_statistics_physical_data` | Host name, user and process id per participant, shown instead of `local` / `host:<id>`. |
| `_fastdds_statistics_data_count` | DATA/DATA_FRAG submessages each writer sent through a transport. Zero-copy delivery does not touch it, so a growing count settles whether data-sharing was really used (see [data-sharing.md](data-sharing.md#confidence)). |

## Enabling statistics on the observed nodes

No code change is needed; Fast DDS reads an environment variable when the participant is
created:

```
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC"
```

Nodes started without it are reported with the warning `stats-not-enabled-on-writer`.

## Granularity

Statistics are per *participant* (one per ROS node), so a measurement applies to the
writer's node → reader's node link. The prediction from discovery is what tells the
individual pairs apart. `--stats` observes for the full `--timeout` (default 5 s, the
quiet-period early exit is disabled) so that counters can accumulate; idle topics show
`!no-traffic-observed`.

## Pitfall: the 10-instance limit

Fast DDS 2.14 creates the statistics DataWriters with the default resource limit of
10 instances. `RTPS_SENT` is keyed by destination locator, so a node that talks to more
than 10 locators (a handful of peers is enough: every peer has metatraffic, user-data and
SHM locators) silently stops reporting the extra ones. The tool flags this as
`!stats-writer-instance-limit-suspected`.

Lift the limit on the observed nodes with the shipped profile. Profile names must match
the aliases passed in `FASTDDS_STATISTICS`:

```
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/statistics.xml
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC"
```

Fast DDS reads a single profiles file; `datasharing_auto_stats.xml` is the merge of this
file with `datasharing_auto.xml` for observing data-sharing with `--stats`. (The tool's
own statistics readers already use unlimited instances.)

## Implementation notes

- `RTPS_SENT` reports the *participant* GUID as source, and `byte_count` is the plain
  cumulative byte total (`byte_magnitude_order` is only `floor(log10(byte_count))`).
- The generated type-support code for the statistics topics is vendored (Apache-2.0)
  because the ROS distributions ship the compiled types in the Fast DDS library but
  neither their headers nor `fastddsgen`:
  `src/fastdds_transport_viz/third_party/fastdds_statistics_types/` (Fast DDS 2.14.6,
  Jazzy) and `.../fastdds_statistics_types_v3/` (Fast DDS 3.2.4, Kilted / Rolling);
  CMake picks one by the Fast DDS major version. Replace the matching directory when
  targeting another Fast DDS version.
