#!/bin/bash
# usage: build.sh  -- builds type_probe into <repo>/build/probes/p193/probe_$ROS_DISTRO inside
# the distribution's image (no ROS packages, no ament: straight against the installed Fast DDS,
# libfastrtps on 2.x, libfastdds on 3.x).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p193"; mkdir -p "$W"
P=/opt/ros/$ROS_DISTRO
ARCH_LIB="$P/lib/$(uname -m)-linux-gnu"
if [ -d "$P/include/fastdds/fastdds/rtps" ] || [ -f "$P/lib/libfastdds.so" ]; then
  FAST=fastdds
else
  FAST=fastrtps
fi
INC=""
for d in "$P/include" "$P/include/fastdds" "$P/include/fastrtps" "$P/include/fastcdr" "$P/includefastcdr"; do
  [ -d "$d" ] && INC="$INC -I$d"
done
g++ -std=c++17 -O1 "$HERE/type_probe.cpp" -o "$W/probe_$ROS_DISTRO" $INC \
  -L"$P/lib" -L"$ARCH_LIB" -Wl,-rpath,"$P/lib" -l$FAST -lfastcdr -pthread
echo "built $W/probe_$ROS_DISTRO against lib$FAST"
