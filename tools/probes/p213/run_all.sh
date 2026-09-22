#!/bin/bash
# Lyrical: every #213 case, one ROS_DOMAIN_ID each (62..). Results in out/<label>.*
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
W="$ROOT/build/probes/p213"; mkdir -p "$W/out"   # binaries in $W, logs in $W/out
d=62
run() { local label=$1; shift; env ROS_DOMAIN_ID=$d "$@" "$HERE/run_case.sh" $label; d=$((d+1)); }
run rename_struct   P206_EXT=final P213_STRUCT_NAME=other::String_ SIDES=a
run rename_member   P206_EXT=final P206_MEMBER_NAME=text SIDES=a
run hdr_same        P213_MODE=header P206_EXT=final P206_TYPE_NAME=std_msgs::msg::dds_::Header_ SIDES=a
run hdr_nested      P213_MODE=header P206_EXT=final P206_TYPE_NAME=std_msgs::msg::dds_::Header_ P213_NESTED_NAME=other::Time_ SIDES=a
run mb_same         P206_EXT=final P213_TP=minimal_bandwidth SIDES=a
run mb_extra        P206_EXT=final P213_TP=minimal_bandwidth SIDES=b
run mb_rename_struct P206_EXT=final P213_TP=minimal_bandwidth P213_STRUCT_NAME=other::String_ SIDES=a
run mb_appendable   P213_TP=minimal_bandwidth SIDES=a
run tp_disabled_extra P206_EXT=final P213_TP=disabled SIDES=b
run tp_regonly_extra  P206_EXT=final P213_TP=registration_only SIDES=b
run tp_enabled_extra  P206_EXT=final P213_TP=enabled SIDES=b
