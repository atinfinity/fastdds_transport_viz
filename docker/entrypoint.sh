#!/bin/bash
set -e
source "/opt/ros/${ROS_DISTRO:-jazzy}/setup.bash"
# The workspace of this distribution only (build/<distro>/, see the Dockerfile): a top-level
# install/ may come from another distribution or the former layout.
ws_setup="/ws/build/${ROS_DISTRO:-jazzy}/install/setup.bash"
if [ -f "$ws_setup" ]; then
  source "$ws_setup"
elif [ -f /ws/install/setup.bash ]; then
  echo "entrypoint: /ws/install is no longer sourced; run colcon build (it builds into build/${ROS_DISTRO:-jazzy}/)" >&2
fi
exec "$@"
