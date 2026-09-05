# Getting started

This page takes you from a plain ROS 2 installation to the first `ros2 transport list`,
with statistics and the web viewer. Everything runs on Linux; the tool observes Fast DDS,
so the nodes you look at must use `rmw_fastrtps_cpp` (the default RMW of Jazzy, Kilted
and Rolling).

| ROS 2 distribution | Fast DDS | Notes |
|---|---|---|
| Jazzy (Ubuntu 24.04) | 2.14 | primary target |
| Kilted (Ubuntu 24.04) | 3.2 | |
| Rolling | 3.x | best effort (CI allows failures) |

## 1. Build it

Two packages live in `src/`: `fastdds_transport_viz` (the C++ tool) and `ros2transport`
(the `ros2 transport` command). Build them in a colcon workspace like any other ROS 2
package.

### Native (recommended)

Prerequisites: a ROS 2 desktop or base installation, `python3-colcon-common-extensions`
and `rosdep` (`sudo rosdep init && rosdep update` once).

```
mkdir -p ~/ws/src && cd ~/ws
git clone https://github.com/atinfinity/fastdds_transport_viz.git src/fastdds_transport_viz
source /opt/ros/jazzy/setup.bash              # or kilted / rolling
rosdep install --from-paths src --ignore-src -y
colcon build --symlink-install
source install/setup.bash
```

`rosdep` installs the build dependencies (`rclcpp`, Fast DDS headers, `nlohmann-json`),
the runtime ones (`rmw_fastrtps_cpp`, `demo_nodes_cpp` for the examples) and the test
dependencies. `source install/setup.bash` is needed in every shell that runs the tool;
it also registers the `ros2 transport` command.

### Docker (alternative)

The repository ships a `compose.yaml` with a development image (`ros:jazzy`, or
`ROS_DISTRO=kilted` / `rolling`), the repository mounted at `/ws`, and `ipc: host` so that
the tool sees the host's shared memory:

```
docker compose build
docker compose run --rm dev
colcon build --symlink-install && source install/setup.bash
```

The rest of this page works the same inside that shell. The container has its own
network namespace, so it can observe nodes running in the same container but not nodes
on the Docker host; [development.md](development.md#docker-environment) explains the
`hostnet` service for that.

## 2. First run

Start two demo nodes and look at them:

```
ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 transport list -v --explain
```

```
TOPIC     TYPE                 PUBS  SUBS  TRANSPORT  RATE  REASON
/chatter  std_msgs/msg/String  1     1     SHM x1     -     same-host-guid,datasharing-disabled-writer,both-shm-locators
    /talker@local -> /listener@local  SHM  -  same-host-guid,datasharing-disabled-writer,both-shm-locators

shared memory: /dev/shm 2.19 MB used of 16.7 GB (16.7 GB free) | Fast DDS 2.19 MB in 3 segment(s), 6 port(s), 0 data-sharing histories

Reason codes:
  both-shm-locators
      Both endpoints announce a shared-memory locator ...
```

What you are looking at:

- one row per topic, then (with `-v`) one row per writer → reader pair with the
  predicted transport and the reason codes that led to it (`--explain` prints their
  descriptions; `ros2 transport codes` lists every code);
- the `shared memory:` line describes the `/dev/shm` of the environment the tool runs
  in ([how-it-works.md](how-it-works.md#shared-memory-of-the-environment)).

Make a second listener that cannot use shared memory and watch the verdict change:

```
FASTDDS_BUILTIN_TRANSPORTS=UDPv4 ros2 run demo_nodes_cpp listener &
ros2 transport list -v                 # second pair: UDPv4, reader-no-shm-locator
```

The tool needs nothing from the observed nodes for this: the verdicts come from the
discovery data every Fast DDS participant announces anyway.

!!! note "Run it where the nodes run"
    Discovery data is only visible from the same DDS domain, and shared memory only
    from the same IPC namespace. Run the tool with the same environment variables
    (`ROS_DOMAIN_ID`, `FASTDDS_BUILTIN_TRANSPORTS`, XML profile, Discovery Server
    settings) and in the same network/IPC namespace as the nodes you observe. See
    [how-it-works.md](how-it-works.md#run-it-where-the-nodes-run).

## 3. Measure instead of predict (`--stats`)

Predictions can be confirmed with the Fast DDS statistics module, which the observed
nodes have to enable through an environment variable before they start:

```
export FASTDDS_STATISTICS="RTPS_SENT_TOPIC;HISTORY_LATENCY_TOPIC;PHYSICAL_DATA_TOPIC;DATA_COUNT_TOPIC;PUBLICATION_THROUGHPUT_TOPIC"
export FASTRTPS_DEFAULT_PROFILES_FILE=$(ros2 pkg prefix fastdds_transport_viz)/share/fastdds_transport_viz/config/statistics.xml
ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener &
ros2 transport list -v --stats
```

The pair row now carries `measured=SHM 47pkt 3.2 kB`, the `RATE` column shows the
payload throughput, and hosts are shown by name with process ids. The profile file
lifts a resource limit of the statistics writers; [statistics.md](statistics.md) explains
the topics and the pitfall it avoids.

## 4. Keep watching

```
ros2 transport list --watch --stats --interval 2
```

re-observes every two seconds and marks what changed since the previous frame (`+`
appeared, `~` changed, `-` disappeared). Keys: `q` quit, `p` pause, `v` pairs, `e`
legend, `a` all topics.

## 5. See it in the browser

```
ros2 run fastdds_transport_viz transport_viz_web --stats --interval 1
# transport_viz_web: listening on http://127.0.0.1:8765/
```

Open the URL: hosts are columns, nodes are boxes, pairs are arrows colored by transport,
updated live. `ros2 transport list --json > snapshot.json` produces a document that
the same page (`web/index.html`) can open offline. See [web-viewer.md](web-viewer.md).

## 6. Command reference

```
ros2 transport list [--domain N] [--timeout S] [--quiet S] [--topic REGEX] [--node REGEX]
                    [--all] [-v] [--explain] [--stats] [--json] [--color auto|always|never]
                    [--watch [--interval S]]
ros2 transport codes
```

`ros2 transport` execs the `transport_viz` binary of `fastdds_transport_viz`; the binary
can be run directly as `ros2 run fastdds_transport_viz transport_viz` with the same options
plus `--list-codes`. Exit codes: 0 on success, 2 on a usage error.

## First checks when something is off

| Symptom | Check |
|---|---|
| `ros2: error: argument Call ... invalid choice: 'transport'` | `source install/setup.bash` in this shell; `ros2transport` must be built in the same workspace. |
| no topics at all | same `ROS_DOMAIN_ID` as the nodes? `ROS_AUTOMATIC_DISCOVERY_RANGE=OFF` limits every participant to itself. With a Discovery Server the tool needs the same `ROS_DISCOVERY_SERVER` (it becomes a SUPER_CLIENT automatically). |
| nodes on another machine are missing | the other machine must be reachable by multicast, or listed in `ROS_STATIC_PEERS`, or both sides use a Discovery Server; see [development.md](development.md#two-physical-hosts). |
| `!stats-not-enabled-on-writer` with `--stats` | the node was started without `FASTDDS_STATISTICS`; the variable must be set before the node starts. |
| `!shm-not-visible` | the nodes use another `/dev/shm` (another container or host); the shared-memory line describes the tool's environment only. |
| `!shm-stale-files` | crashed processes left segments behind; `fastdds shm clean` removes them. |
| the tool shows up as a node | it does not: its own node `/_transport_viz_<pid>` and participant are filtered out. If you see it, please open an issue with `--json` output. |

Next: [How it works](how-it-works.md) for the decision rules, or
[Architecture](architecture.md) if you want to change the tool.
