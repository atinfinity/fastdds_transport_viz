#!/usr/bin/env bash
# Line coverage of the C++ package: builds with --coverage into
# build/<distro>/coverage/{build,install}, runs the package's tests and prints a per-file
# summary with gcovr.
# Run inside the dev container (docker compose run --rm dev bash scripts/coverage.sh).
set -eo pipefail   # no -u: the ROS setup scripts reference unset variables
cd "$(dirname "$0")/.."
cov="build/${ROS_DISTRO:?source a ROS setup first}/coverage"
command -v gcovr >/dev/null || { apt-get update -qq && apt-get install -y -qq gcovr; }
colcon build --symlink-install --build-base "$cov/build" --install-base "$cov/install" \
  --cmake-args -DCMAKE_CXX_FLAGS=--coverage -DCMAKE_EXE_LINKER_FLAGS=--coverage
# shellcheck disable=SC1091
source "$cov/install/setup.bash"
colcon test --build-base "$cov/build" --install-base "$cov/install" --packages-select fastdds_transport_viz
colcon test-result --test-result-base "$cov/build" --verbose | tail -1
gcovr -r src/fastdds_transport_viz --object-directory "$cov/build/fastdds_transport_viz" \
  --gcov-ignore-errors=no_working_dir_found --gcov-ignore-parse-errors \
  -e '.*CompilerId.*' -e '.*third_party.*' -e '.*/test/.*' -s "$@"
