#!/bin/bash
# same rich definition on both sides: C (rclpy) vs C++ (rclcpp) introspection, cpp vs dynamic rmw
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p213_ros"; mkdir -p "$W/out"   # ws_*/{build,install,log} and logs (out/) in $W
source /opt/ros/lyrical/setup.bash
mkdir -p "$W/ws_rich"
cd "$HERE/ws_rich" && colcon --log-base "$W/ws_rich/log" build --build-base "$W/ws_rich/build" --install-base "$W/ws_rich/install" --event-handlers console_direct- --cmake-args -DBUILD_TESTING=OFF > "$W/ws_rich/build.log" 2>&1 || { echo build failed; exit 1; }
ROS_DOMAIN_ID=77 "$HERE/run_case.sh" rich_cpp2py rich rich cpp py
ROS_DOMAIN_ID=78 "$HERE/run_case.sh" rich_py2cpp rich rich py cpp
ROS_DOMAIN_ID=79 PUB_RMW=rmw_fastrtps_cpp SUB_RMW=rmw_fastrtps_dynamic_cpp "$HERE/run_case.sh" rich_cpp2dyn rich rich cpp cpp
ROS_DOMAIN_ID=73 PUB_RMW=rmw_fastrtps_dynamic_cpp SUB_RMW=rmw_fastrtps_cpp "$HERE/run_case.sh" rich_dynpy2cpp rich rich py cpp
