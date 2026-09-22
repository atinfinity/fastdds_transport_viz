#!/bin/bash
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p213_ros"; mkdir -p "$W/out"   # ws_*/{build,install,log} and logs (out/) in $W
source /opt/ros/lyrical/setup.bash
cd "$HERE"
for w in ws_*; do
  mkdir -p "$W/$w"
  (cd $w && colcon --log-base "$W/$w/log" build --build-base "$W/$w/build" --install-base "$W/$w/install" --event-handlers console_direct- --cmake-args -DBUILD_TESTING=OFF > "$W/$w/build.log" 2>&1; echo "$w rc=$?")
done
