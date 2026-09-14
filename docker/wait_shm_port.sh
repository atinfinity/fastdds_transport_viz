#!/usr/bin/env bash
# wait_shm_port.sh <port>...: wait until a Fast DDS participant of this IPC namespace listens
# on each SHM port, that is, holds the lock of its fastrtps_port<N>_el / fastdds_port<N>_el
# file. A free lock file (a sender's short zombie check) does not count, and the file is
# opened without creating it, so no lock file is left behind for Fast DDS to take as a zombie.
held() {
  local f fd
  for f in /dev/shm/fastrtps_port"$1"_el /dev/shm/fastdds_port"$1"_el; do
    if { exec {fd}<"$f"; } 2>/dev/null; then
      if flock -n "$fd"; then
        flock -u "$fd"
        exec {fd}<&-
      else
        exec {fd}<&-
        return 0
      fi
    fi
  done
  return 1
}

for port in "$@"; do
  until held "$port"; do sleep 0.2; done
done
