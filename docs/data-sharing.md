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

The verdict is `likely` (`DATA_SHARING?`) when both endpoints announce data-sharing with
intersecting domain ids. With `--stats` it becomes `certain` only when `HISTORY_LATENCY`
proves delivery while no packet at all reached the reader's locators. Reliable
data-sharing endpoints still exchange heartbeats over SHM and statistics are per
participant, so traffic on the link does not disprove zero-copy delivery; the reason code
`datasharing-ambiguous-participant-traffic` says exactly that.
