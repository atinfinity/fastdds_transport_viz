#!/bin/bash
# Builds the probe binaries of this directory into <repo>/build/probes/p213 (Lyrical image;
# no ROS packages, no ament: straight against libfastdds 3.x). The image really ships the
# fastcdr headers under "includefastcdr" ("include" + "fastcdr" concatenated).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p213"; mkdir -p "$W"
FLAGS="-I/opt/ros/lyrical/include/fastdds -I/opt/ros/lyrical/includefastcdr -L/opt/ros/lyrical/lib -L/opt/ros/lyrical/lib/aarch64-linux-gnu -lfastdds -lfastcdr -pthread"
g++ -std=c++17 -O1 "$HERE/nonros213.cpp" -o "$W/nonros213" $FLAGS
g++ -std=c++17 -O1 "$HERE/probe.cpp" -o "$W/probe" $FLAGS
