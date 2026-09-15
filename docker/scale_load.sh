#!/bin/bash
# Starts processes SCALE_FIRST..SCALE_LAST of a scale_load system of SCALE_PROCESSES processes
# (#74, scripts/scale_test.sh). Every process runs the binary directly, not through `ros2 run`,
# so that the Python launcher does not take its memory per process.
set -euo pipefail
processes="${SCALE_PROCESSES:?}"
first="${SCALE_FIRST:-0}"
last="${SCALE_LAST:-$((processes - 1))}"
bin="$(ros2 pkg prefix fastdds_transport_viz)/lib/fastdds_transport_viz/scale_load"
for ((i = first; i <= last; ++i)); do
  "$bin" --index "$i" --processes "$processes" --nodes "${SCALE_NODES:-1}" \
    --topics "${SCALE_TOPICS:?}" --readers "${SCALE_READERS:-4}" --seed "${SCALE_SEED:-74}" &
done
echo "scale_load: started processes $first..$last of $processes" >&2
wait
