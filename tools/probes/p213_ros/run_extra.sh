#!/bin/bash
# rclpy sides and rmw_fastrtps_dynamic_cpp sides
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p213_ros"; mkdir -p "$W/out"   # ws_*/{build,install,log} and logs (out/) in $W
ROS_DOMAIN_ID=73 "$HERE/run_case.sh" py_cpp2py_base   base base cpp py
ROS_DOMAIN_ID=74 "$HERE/run_case.sh" py_py2cpp_base   base base py cpp
ROS_DOMAIN_ID=75 "$HERE/run_case.sh" py_py2py_a_type  base a_type py py
ROS_DOMAIN_ID=76 "$HERE/run_case.sh" py_py2py_b_default base b_default py py
ROS_DOMAIN_ID=77 PUB_RMW=rmw_fastrtps_dynamic_cpp SUB_RMW=rmw_fastrtps_dynamic_cpp "$HERE/run_case.sh" dyn_base base base
ROS_DOMAIN_ID=78 PUB_RMW=rmw_fastrtps_cpp SUB_RMW=rmw_fastrtps_dynamic_cpp "$HERE/run_case.sh" mix_base base base
ROS_DOMAIN_ID=79 PUB_RMW=rmw_fastrtps_dynamic_cpp SUB_RMW=rmw_fastrtps_dynamic_cpp "$HERE/run_case.sh" dyn_a_type base a_type
ROS_DOMAIN_ID=73 PUB_RMW=rmw_fastrtps_dynamic_cpp SUB_RMW=rmw_fastrtps_dynamic_cpp "$HERE/run_case.sh" dyn_b_default base b_default
ROS_DOMAIN_ID=74 PUB_RMW=rmw_fastrtps_dynamic_cpp SUB_RMW=rmw_fastrtps_dynamic_cpp "$HERE/run_case.sh" dyn_c_const base c_const
ROS_DOMAIN_ID=75 PUB_RMW=rmw_fastrtps_dynamic_cpp SUB_RMW=rmw_fastrtps_dynamic_cpp "$HERE/run_case.sh" dyn_d_rename base d_rename
ROS_DOMAIN_ID=76 PUB_RMW=rmw_fastrtps_dynamic_cpp SUB_RMW=rmw_fastrtps_dynamic_cpp "$HERE/run_case.sh" dyn_e_nodef base e_nodef
