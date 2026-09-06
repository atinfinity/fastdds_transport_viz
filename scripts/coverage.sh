#!/usr/bin/env bash
# Line coverage of the C++ package: builds with --coverage into build_cov/ and
# install_cov/, runs the package's tests and prints a per-file summary with gcovr.
# Run inside the dev container (docker compose run --rm dev bash scripts/coverage.sh).
set -eo pipefail   # no -u: the ROS setup scripts reference unset variables
cd "$(dirname "$0")/.."
command -v gcovr >/dev/null || { apt-get update -qq && apt-get install -y -qq gcovr; }
colcon build --symlink-install --build-base build_cov --install-base install_cov \
  --cmake-args -DCMAKE_CXX_FLAGS=--coverage -DCMAKE_EXE_LINKER_FLAGS=--coverage
# shellcheck disable=SC1091
source install_cov/setup.bash
colcon test --build-base build_cov --install-base install_cov --packages-select fastdds_transport_viz
colcon test-result --test-result-base build_cov --verbose | tail -1
gcovr -r src/fastdds_transport_viz --object-directory build_cov/fastdds_transport_viz \
  --gcov-ignore-errors=no_working_dir_found --gcov-ignore-parse-errors \
  -e '.*CompilerId.*' -e '.*third_party.*' -e '.*/test/.*' -s "$@"
