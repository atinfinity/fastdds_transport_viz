This document is a declaration of software quality for the `fastdds_transport_viz` package, based on the guidelines in [REP-2004](https://www.ros.org/reps/rep-2004.html).

# `fastdds_transport_viz` Quality Declaration

The package `fastdds_transport_viz` claims to be in the **Quality Level 3** category, with one exception to the platform support requirement: Windows 10, a tier 1 platform of [REP-2000](https://www.ros.org/reps/rep-2000.html), is not supported (see [Platform Support](#platform-support-6)).

It is a development and introspection tool - it shows which Fast DDS transport each ROS 2 topic is communicated over, and why - and is not meant to run inside an embedded product.

Below are the rationales, notes and caveats for this claim, organized by each requirement listed in the [Package Requirements for Quality Level 3 in REP-2004](https://www.ros.org/reps/rep-2004.html).

## Version Policy [1]

### Version Scheme [1.i]

`fastdds_transport_viz` uses `semver` according to the recommendation for ROS Core packages in the [ROS 2 Developer Guide](https://docs.ros.org/en/rolling/The-ROS2-Project/Contributing/Developer-Guide.html#versioning).
The current version is the `<version>` of [`package.xml`](package.xml); every release is tagged `vX.Y.Z` in the repository and described in [`CHANGELOG.rst`](CHANGELOG.rst).
From 2.1.0 on, every release is also published as a container image, `ghcr.io/atinfinity/fastdds_transport_viz:<X.Y.Z>-<distro>` (Humble, Jazzy, Lyrical; x86_64 and arm64), built from the tag by [`release.yml`](../../.github/workflows/release.yml) and pushed only after its smoke test passes.

### Version Stability [1.ii]

`fastdds_transport_viz` is at a stable version, i.e. `>= 1.0.0` (1.0.0 was released on 2026-09-06).

### Public API Declaration [1.iii]

Level 3 does not require a declared public API; one is declared so that the version numbers mean something.
The public API of this package is what a user or a script relies on:

- the `transport_viz` command line: its sub-commands (`transport_viz`, `transport_viz diff`), options, exit codes, and the reason and warning codes printed by `--list-codes`;
- the `--json` document, as specified by the JSON Schema in [`schema/`](../../schema/) and versioned by its `schema_version` field;
- the `--csv` columns;
- the `transport_viz_web` command line and HTTP endpoints: its options, `/events`, `/latest.json`, the series names and labels of `/metrics`, and the JSON Lines format written by `--record`;
- the URL parameters of the web viewer (`?src=`, `&diff=`, `&frame=`, `?live=1`, `?history=`, `&key=guid`).

Not part of the public API, and free to change in any release:

- the look of the human-readable table (columns, symbols, colors, wording) and the text printed on stderr;
- the C++ libraries and headers installed by the package (they are not exported with `ament_export_*` and exist for the package's own executables and tests);
- the verification nodes installed next to the tool (`bounded_pub`, `scale_load`, `rate_load`, ...);
- the layout and controls of the web viewer.

### API Stability Policy [1.iv]

Changes to the public API follow `semver`:

- **major**: removing or renaming an option, a CSV column, a `/metrics` series or label, an HTTP endpoint or a URL parameter; bumping `schema_version`; removing, renaming or redefining a reason or warning code; changing the meaning of an exit code;
- **minor**: adding an option, a JSON key, a CSV column, a reason or warning code, a `/metrics` series; refining how a pair is predicted or measured (its transport, confidence or the warnings it carries) as detection improves;
- **patch**: bug fixes that leave the API as it is.

### ABI Stability Policy [1.v]

`fastdds_transport_viz` provides no library for other packages, so there is no ABI to keep stable.

### ABI and ABI Stability Within a Released ROS Distribution [1.vi]

Level 3 does not require API or ABI stability within a released ROS distribution, and none is guaranteed: one `main` branch serves Humble, Jazzy, Lyrical and Rolling, so a major release reaches every distribution at the same time.
Breaking changes are kept to major releases and are listed in [`CHANGELOG.rst`](CHANGELOG.rst).

## Change Control Process [2]

`fastdds_transport_viz` follows the process described in [`CONTRIBUTING.md`](../../CONTRIBUTING.md).

### Change Requests [2.i]

All changes occur through pull requests against `main`.
Branch protection on `main` applies to administrators too, so nothing is pushed to `main` directly.
GitHub Actions dependencies are updated by Dependabot pull requests.

### Contributor Origin [2.ii]

Not required at Level 3. Contributions are licensed under Apache-2.0 as stated in [`CONTRIBUTING.md`](../../CONTRIBUTING.md); there is no DCO or CLA.

### Peer Review Policy [2.iii]

Not required at Level 3. The project has one maintainer, and pull requests are not required to have an approving review.

### Continuous Integration [2.iv]

Every pull request runs [`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) and [`.github/workflows/docs.yml`](../../.github/workflows/docs.yml), whose aggregate checks `CI result` and `Docs result` are required before merging.
`CI result` covers:

- `colcon build` and `colcon test` on Humble, Jazzy, Lyrical and Rolling (x86_64), Jazzy and Lyrical on arm64, and the released distributions again with `rmw_fastrtps_dynamic_cpp`; Rolling runs with `continue-on-error` and does not block a pull request;
- a coverage build (Jazzy, x86_64) reported to [Coveralls](https://coveralls.io/github/atinfinity/fastdds_transport_viz);
- the web viewer's unit and headless-browser tests (Node).

`Docs result` covers `mkdocs build --strict` of the documentation site and an offline link check of every Markdown file (lychee).

Merges to `main` also run the multi-container integration scenarios (`scripts/integration_test.sh`) on x86_64 and arm64, and [`.github/workflows/rolling.yml`](../../.github/workflows/rolling.yml) builds and tests Rolling every week on both architectures, opening an issue when it breaks; that is where a Rolling breakage is reported.

### Documentation Policy [2.v]

Not required at Level 3. In practice a pull request that changes behavior updates the documentation and `CHANGELOG.rst` in the same change, with the Japanese pages (`*.ja.md`) kept in step with the English ones.

## Documentation [3]

### Feature Documentation [3.i]

Not required at Level 3. The features are documented in the [README](../../README.md) and on the [documentation site](https://atinfinity.github.io/fastdds_transport_viz/) (English and Japanese), built from [`docs/`](../../docs/).

### Public API Documentation [3.ii]

Not required at Level 3. The command line is documented by `transport_viz --help` and the README, the JSON document by its [JSON Schema](../../schema/), and `transport_viz_web`, `/metrics` and the web viewer in [`docs/web-viewer.md`](../../docs/web-viewer.md).

### License [3.iii]

The license for `fastdds_transport_viz` is Apache License 2.0, declared in [`package.xml`](package.xml) and in the repository's [`LICENSE`](../../LICENSE) file.

Vendored third-party code keeps its own license:

- `third_party/fastdds_statistics_types*/`: types generated from the Fast DDS statistics IDL, Apache-2.0, with eProsima's `LICENSE` and `NOTICE`;
- `web/vendor/d3.v7.min.js`: d3, ISC license, in `web/vendor/d3.LICENSE`.

### Copyright Statements [3.iv]

The package's own source files carry a `Copyright 2026 atinfinity` line and an `SPDX-License-Identifier: Apache-2.0` header.
`ament_copyright` is disabled because it expects the full license boilerplate rather than the SPDX form; the vendored files keep their upstream copyright statements.

### Quality Declaration [3.v]

This document is the quality declaration. It is linked from the Quality Declaration section of the repository's [README](../../README.md#quality-declaration).
No centralized list of Level 3 packages exists, and the claim has not been through an external peer review.

## Testing [4]

Level 3 has no testing requirement. The package has, all run by `colcon test` in CI:

- gtest unit tests of the decision logic and the renderers, which build endpoints by hand without a DDS participant; of the shared-memory inspection, against a fake `/dev/shm` directory; and of the statistics, discovery and `ros_discovery_info` observers, with real Fast DDS participants on private domains;
- `launch_testing` tests that start real ROS 2 nodes in one host and check the predicted and measured transports (SHM, UDPv4, UDPv6, TCPv4 for large data, data-sharing, Discovery Server, Easy Mode, type mismatches, services and actions, `--watch`, `--json` against the schema, `transport_viz_web`), under `rmw_fastrtps_cpp` and `rmw_fastrtps_dynamic_cpp`;
- pytest tests of the command line and of `transport_viz_web`;
- the linters of `ament_lint_common` (cpplint, uncrustify, cppcheck, flake8, pep257, lint_cmake, xmllint), with `ament_copyright` disabled as explained above.

Coverage is tracked (line coverage of the C++ code, 92 % at the time of writing) and reported for every pull request, with no gate.
Multi-container scenarios, the web viewer's browser tests and a scale ladder (up to thousands of pairs, with fixed budgets judged at the `medium` rung) are described in [`docs/development.md`](../../docs/development.md).

## Dependencies [5]

Level 3 allows direct runtime ROS dependencies below Level 3 when they are documented here.
Levels are those declared by each package's quality declaration on 2026-09-22; they are the same on Humble, Jazzy and Lyrical.

### Direct Runtime ROS Dependencies [5.i]

| Dependency | Quality level | Used for |
|---|---|---|
| `rclcpp` | [1](https://github.com/ros2/rclcpp/blob/jazzy/rclcpp/QUALITY_DECLARATION.md) | the tool's node and the graph queries |
| `rmw` | [1](https://github.com/ros2/rmw/blob/jazzy/rmw/QUALITY_DECLARATION.md) | the RMW check |
| `rmw_dds_common` | [1](https://github.com/ros2/rmw_dds_common/blob/jazzy/rmw_dds_common/QUALITY_DECLARATION.md) | reading `ros_discovery_info` |
| `rosidl_typesupport_fastrtps_cpp` | [2](https://github.com/ros2/rosidl_typesupport_fastrtps/blob/jazzy/rosidl_typesupport_fastrtps_cpp/QUALITY_DECLARATION.md) | the type support of `ros_discovery_info` |
| `std_msgs` | [1](https://github.com/ros2/common_interfaces/blob/jazzy/std_msgs/QUALITY_DECLARATION.md) | the verification nodes |
| `rmw_fastrtps_cpp` | [2](https://github.com/ros2/rmw_fastrtps/blob/jazzy/rmw_fastrtps_cpp/QUALITY_DECLARATION.md) | the RMW the tool runs on |
| `demo_nodes_cpp` | none declared (5) | the talker and listener of the [getting started](../../docs/getting-started.md) guide; not used by the tool itself |

`demo_nodes_cpp` is an example package and has no quality declaration. It stays a runtime dependency so that the first steps of the documentation work on a `ros-base` installation; the tool does not load it.

### Optional Direct Runtime ROS Dependencies [5.ii]

None.

### Direct Runtime non-ROS Dependencies [5.iii]

| Dependency | Quality level | Notes |
|---|---|---|
| Fast DDS (`fastrtps` on Humble and Jazzy, `fastdds` from Lyrical on) | [1](https://github.com/eProsima/Fast-DDS/blob/master/QUALITY.md) | discovery data, the statistics module, the shared-memory transport; 2.6 on Humble, 2.14 on Jazzy, 3.x from Lyrical on |
| Fast CDR (`fastcdr`) | [1](https://github.com/eProsima/Fast-CDR/blob/master/QUALITY.md) | serialization of the statistics types |
| nlohmann/json (`nlohmann-json-dev`) | not declared | header-only, MIT license, the JSON library most C++ projects use; `--json`, `--csv` and `transport_viz diff` |

The web viewer and `transport_viz_web` depend only on the Python standard library and the vendored d3.

## Platform Support [6]

`fastdds_transport_viz` supports Linux only: it reads the shared-memory segments and ports of Fast DDS in `/dev/shm` and inspects them with POSIX calls, which have no Windows or macOS counterpart.
This is the exception to this requirement: **Windows 10 (amd64), a tier 1 platform of REP-2000, is not supported**. RHEL (tier 2) and macOS (tier 3) are not supported either.

The tier 1 Ubuntu platforms are supported:

| Distribution | Platform | Verified by |
|---|---|---|
| Humble | Ubuntu 22.04 amd64 | CI |
| Humble | Ubuntu 22.04 arm64 | tested by hand in the Docker environment on an arm64 host (e.g. the `record_flip` scenario of [#82](https://github.com/atinfinity/fastdds_transport_viz/issues/82)), not in CI |
| Jazzy | Ubuntu 24.04 amd64 and arm64 | CI |
| Lyrical | Ubuntu 26.04 amd64 and arm64 | CI; REP-2000 has no Lyrical section yet, so the target is the Ubuntu of the `ros:lyrical` image |
| Rolling | Ubuntu 26.04 amd64 (arm64 weekly) | CI and the weekly Rolling workflow |

Only the Fast DDS RMWs are supported (`rmw_fastrtps_cpp` and `rmw_fastrtps_dynamic_cpp`); the tool refuses to start on another RMW.

## Security [7]

### Vulnerability Disclosure Policy [7.i]

This package follows the repository's [security policy](../../SECURITY.md): vulnerabilities are reported privately through GitHub's private vulnerability reporting, acknowledged within 7 days, and fixed or published as a GitHub Security Advisory within 90 days. Fixes go to the latest release and `main`.
