# Type-rule probes (#193, #206, #210, #213)

Throwaway programs and scripts kept as the evidence behind the #193, #206, #210 and #213
verification rows in `docs/verification-log.md`. They are not part of the packages: colcon
never builds them (this directory has a `COLCON_IGNORE`, so a `colcon build` from the
repository root does not pick up the `p213_msgs` packages below) and CI never runs them.

Everything but p193 runs in the Lyrical dev container, with the repository mounted at `/ws`:

```bash
ROS_DISTRO=lyrical docker compose run --rm dev bash
colcon build --symlink-install     # the tool itself, into build/lyrical/ (the run scripts source it)
```

The non-ROS programs link straight against the image's `libfastdds` 3.x (no rclcpp, no
rmw, no ament). Binaries and logs never land in this directory: each probe writes to
`build/probes/<probe>/` (binaries) and `build/probes/<probe>/out/` (logs, JSON, tables),
relative to the repository root. Every script finds the repository from its own location,
so it can be started from any working directory. Give every case its own `ROS_DOMAIN_ID`,
as the verification did, so that leftovers of one case never show up in the next.

## p193: what type identity reaches the wire (#193)

A discovery-only participant (`type_probe.cpp`, in the style of
`src/fastdds_transport_viz/src/discovery_observer.cpp`) that prints, for every remote writer
and reader, whether it announces a `TypeIdentifier`, a `TypeObject` or a `TypeInformation`,
and its `USER_DATA`, next to a `demo_nodes_cpp` talker and listener. It showed that Humble
and Jazzy announce none of the three and Lyrical announces `TypeInformation`. It runs in
every distribution's image (`libfastrtps` on 2.x, `libfastdds` on 3.x):

```bash
ROS_DISTRO=jazzy docker compose run --rm dev bash    # or humble / lyrical
tools/probes/p193/build.sh                           # -> build/probes/p193/probe_$ROS_DISTRO
ROS_DOMAIN_ID=71 tools/probes/p193/run.sh 8          # talker/listener logs in build/probes/p193/out/
```

## p206: a non-ROS peer with a differing definition (#206)

A standalone Fast DDS 3.x application (`nonros_app.cpp`, a `DynamicType` registered with
`register_type_object_representation()`) with a writer and a reader per side on
`rt/chatter` under `example_interfaces::msg::dds_::String_`: side a
`struct { string data; }`, side b `struct { string data; int32 extra; }`, next to
`demo_nodes_cpp` talker and listener. It showed that such a peer announces no ROS 2 type
hash but a non-empty EK_COMPLETE `TypeInformation` hash, which the `type-information-mismatch`
rule of #206 uses.

Knobs (environment): `P206_SIDES=a|b|ab`, `P206_EXT=final|appendable|mutable`,
`P206_MEMBER_NAME`, `P206_TYPE_NAME`, `P206_TOPIC`, `P206_NO_TYPEOBJECT=1`.

```bash
tools/probes/p206/build.sh                                   # -> build/probes/p206/nonros_app
ROS_DOMAIN_ID=60 P206_SIDES=ab tools/probes/p206/run_tool.sh               # transport_viz --json --all
ROS_DOMAIN_ID=60 P206_SIDES=ab P206_EXT=final tools/probes/p206/run_tool_table.sh final.txt
ROS_DOMAIN_ID=60 P206_SIDES=a tools/probes/p206/run_tool_stats.sh          # with --stats
ROS_DOMAIN_ID=60 tools/probes/p206/run_probe.sh 14           # discovery listener probe
```

`run_tool.sh`, `run_tool_table.sh` and `run_tool_stats.sh` take the output file as their
first argument (default `build/probes/p206/out/p206.json` / `p206s.json`; a relative name
lands in `build/probes/p206/`); the app, talker, listener and tool logs go to
`build/probes/p206/out/`. `run_probe.sh` uses the #193 discovery probe
`build/probes/p193/probe_lyrical`: build it first with `tools/probes/p193/build.sh` in the Lyrical image.

## p210: what tells an unchecked peer from a fine one (#210)

The #206 app extended with per-writer DATA/DROP lines and end-of-run statuses
(`nonros210.cpp`, `P210_NO_WRITER=1`, `P210_NO_READER=1`), a Fast DDS 3.x
discovery-listener probe printing `product_version`, vendor, `type_information`,
`max_serialized_size`, representation and type consistency (`probe.cpp`), and an SPDP/SEDP
sniffer (`sniff_sedp.py`, AF_PACKET, UDP only). It showed that no signal tells a peer that
announces no `TypeInformation` with a differing definition from one that is fine.

```bash
tools/probes/p210/build.sh                                   # -> build/probes/p210/{nonros210,probe}
ROS_DOMAIN_ID=50 P206_NO_TYPEOBJECT=1 tools/probes/p210/run_case.sh noto_app 12
tools/probes/p210/run_sniff.sh                               # four configurations, domains 44..47
ROS_DOMAIN_ID=51 tools/probes/p210/run_stats.sh              # transport_viz --stats; TAG=, EARLY=1, NSEC=
```

Logs: `build/probes/p210/out/<label>.*` (`run_case.sh`), `sniff_<config>.*`
(`run_sniff.sh`), `<TAG>.*` (default `stats`, `run_stats.sh`). The scripts cover the
Lyrical side only; the Jazzy and Humble peers of the mixed run are not scripted here.

The matching and statistics paths were read in the upstream sources, which are not kept
here. To look at the same code:

- Fast DDS 3.6.2 and 2.14.6: `git clone --depth 1 --branch v3.6.2 https://github.com/eProsima/Fast-DDS.git`
  (and `--branch v2.14.6`). Files read: `src/cpp/rtps/builtin/discovery/endpoint/EDP.cpp`
  (`EDP::valid_matching`, `is_same_type`),
  `src/cpp/fastdds/domain/DomainParticipantImpl.cpp` (`fill_type_information`),
  `src/cpp/fastdds/xtypes/dynamic_types/DynamicPubSubType.cpp` (3.x) and
  `src/cpp/dynamic-types/DynamicPubSubType.cpp` (2.x) (`deserialize`),
  `src/cpp/rtps/builtin/data/WriterProxyData.cpp` (`PID_TYPE_MAX_SIZE_SERIALIZED`),
  `src/cpp/fastdds/subscriber/DataReaderImpl.cpp`, `src/cpp/fastdds/publisher/DataWriterImpl.cpp`,
  `src/cpp/rtps/reader/StatefulReader.cpp` / `StatelessReader.cpp` (`on_data_notify`) and
  `src/cpp/statistics/rtps/reader/StatisticsReaderImpl.cpp` (HISTORY_LATENCY).
- rmw_fastrtps (the Lyrical image carries 9.4.10, tag `9.4.10` of
  `https://github.com/ros2/rmw_fastrtps.git`; the ref the files were fetched from was not
  recorded): `rmw_fastrtps_shared_cpp/src/TypeSupport_impl.cpp` and
  `rmw_fastrtps_cpp/src/type_support_common.cpp`.

## p213: the type rule against `EDP::valid_matching` / `is_same_type` (#213)

The #210 app extended with `P213_STRUCT_NAME` (registered struct name), `P213_MODE=header`
(`std_msgs/msg/Header` with `P213_NESTED_NAME`, against `ros2 topic pub/echo`) and
`P213_TP=enabled|minimal_bandwidth|disabled|registration_only`
(`fastdds.type_propagation`), next to ROS 2 endpoints on `rt/chatter`, plus the #210 probe.
It showed that Fast DDS 3.x compares the complete or the minimal type identifier and
ignores the names when both sides announce a `TypeInformation`; the #213 fix mirrors that.

```bash
tools/probes/p213/build.sh                                   # -> build/probes/p213/{nonros213,probe}
tools/probes/p213/run_all.sh                                 # all eleven cases, domains 62..72
ROS_DOMAIN_ID=62 P206_EXT=final P213_STRUCT_NAME=other::String_ SIDES=a \
  tools/probes/p213/run_case.sh rename_struct                # one case
tools/probes/p213/summarize.py [label ...]                   # probe hashes, app, listener, tool verdict
```

Logs: `build/probes/p213/out/<label>.{nonros,talker,listener,probe}.log`, `<label>.json`,
`<label>.table.txt`, `<label>.tool.err`. The #206 baselines of the #213 row (default,
`P206_EXT=final`, `P206_NO_TYPEOBJECT=1`) are `tools/probes/p206/run_tool.sh` with
`P206_SIDES=ab`, run with the fixed tool.

## p213_ros: ROS 2 versions of one message (#213)

Eleven colcon workspaces of one package `p213_msgs` (`ws_<variant>/src/p213_msgs`), each
with a different `msg/Foo.msg` (member type changed, member added, default changed,
constant added, member renamed, comment, bound changed, constant renamed, default
removed, and a rich definition) and the same `foo_pub` / `foo_sub` nodes (`common/` is the
template the workspaces were made from). Published and subscribed across rclcpp, rclpy and
`rmw_fastrtps_dynamic_cpp`, they showed that the REP-2011 hash and the `TypeInformation`
differ together in every variant and that the fixed tool agrees with delivery in 25 of 25
cases. Build p213 first: `run_case.sh` uses its probe.

```bash
tools/probes/p213/build.sh
tools/probes/p213_ros/build_all.sh      # every ws_* -> build/probes/p213_ros/ws_*/{build,install,log}
tools/probes/p213_ros/run_all.sh        # rclcpp -> rclcpp, base against each variant (domains 73..79)
tools/probes/p213_ros/run_extra.sh      # rclpy and rmw_fastrtps_dynamic_cpp sides
tools/probes/p213_ros/run_rich.sh       # builds ws_rich, same rich definition across the typesupports
tools/probes/p213_ros/summarize.py [label ...]
```

`run_case.sh <label> <pub_ws> <sub_ws> [cpp|py] [cpp|py]` runs one case (`PUB_RMW`,
`SUB_RMW` pick the rmw of each side). Logs: `build/probes/p213_ros/out/<label>.*`;
colcon logs: `build/probes/p213_ros/ws_*/build.log`.
