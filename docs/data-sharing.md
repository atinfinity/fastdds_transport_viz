# Data-sharing (zero-copy) delivery

Fast DDS can bypass every transport when writer and reader are on the same host and the
type is bounded: the reader maps the writer's history directly (data-sharing delivery).
The tool reports it as `DATA_SHARING`, distinct from the `SHM` transport.

## What ROS 2 nodes announce

`rmw_fastrtps_cpp` announces data-sharing `OFF` for every endpoint by default, so ROS 2
topics normally show `SHM`, never `DATA_SHARING`. Fast DDS resolves `AUTO` against type
boundedness and the history memory policy *before* discovery, so what the tool sees in the
announced QoS is the effective setting (a `std_msgs/String` writer announces `OFF` even
with `AUTO` requested).

To enable it for bounded types (e.g. `std_msgs/Int32`):

```
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/datasharing_auto.xml
export RMW_FASTRTPS_USE_QOS_FROM_XML=1
ros2 run fastdds_transport_viz bounded_pub &
ros2 run fastdds_transport_viz bounded_sub &
ros2 transport list -v                              # /bounded -> DATA_SHARING? (likely)
```

Use `AUTO`, not `ON`, in the default profile: `ON` makes writer creation fail for unbounded
types such as `/rosout`.

## Confidence

The verdict is `likely` (`DATA_SHARING?`) when both endpoints announce data-sharing and
their domain ids intersect (or at least one side announces none). With `--stats` it becomes
`certain` in two ways:

- `HISTORY_LATENCY` proves delivery while no packet at all reached the reader's locators
  (`datasharing-confirmed-no-traffic`).
- `HISTORY_LATENCY` proves delivery and the writer's `DATA_COUNT` did not grow during the
  observation (`datasharing-confirmed-no-data-submessages`). Reliable data-sharing
  endpoints still exchange heartbeats over SHM, but zero-copy delivery never produces a
  DATA submessage, so this counter separates the two. It needs `DATA_COUNT_TOPIC` in
  `FASTDDS_STATISTICS` on the observed nodes:

  ```
  export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/datasharing_auto_stats.xml
  export RMW_FASTRTPS_USE_QOS_FROM_XML=1
  export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC"
  ros2 run fastdds_transport_viz bounded_pub &
  ros2 run fastdds_transport_viz bounded_sub &
  ros2 transport list -v --stats                      # /bounded -> DATA_SHARING (certain)
  ```

If the writer also serves readers without data-sharing its `DATA_COUNT` mixes both paths
and the verdict stays `likely` (`datasharing-ambiguous-mixed-readers`). If every reader
uses data-sharing and the count still grows, Fast DDS did not use zero-copy: the verdict
becomes the measured transport (`SHM?` when no packet to the reader was attributed) with
the reason `datasharing-data-submessages-sent` and the warning `datasharing-not-used`.
Without `DATA_COUNT_TOPIC`, or while `HISTORY_LATENCY` has not proven delivery yet, traffic
on the link leaves the verdict `likely` (`datasharing-ambiguous-participant-traffic`); no
traffic and no delivery proof gives `datasharing-no-delivery-observed`. The JSON `measured` object carries
`data_submessages` (the `DATA_COUNT` delta, `null` when unavailable) and
`delivered_samples`.

## History size

A data-sharing writer keeps its history in a file `fast_datasharing_<writer guid>` under
`/dev/shm`. When that file is visible from the tool (same IPC namespace), its size is
reported on the writer as `datasharing_history_bytes` in JSON and in the web viewer's
endpoint details; the environment's shared-memory line counts every such history (see
[how-it-works.md](how-it-works.md#shared-memory-of-the-environment)). Fast DDS does not
remove the file when the writer is killed, so histories of finished writers show up as
*unmatched* until `fastdds shm clean` runs.
