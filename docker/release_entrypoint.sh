#!/bin/bash
# Entrypoint of the published image (the `release` stage, #78): the ROS 2 installation and
# the packages built into /opt/fastdds_transport_viz, then the command (default
# `ros2 transport list`).
set -e
source "/opt/ros/${ROS_DISTRO}/setup.bash"
source /opt/fastdds_transport_viz/setup.bash
exec "$@"
