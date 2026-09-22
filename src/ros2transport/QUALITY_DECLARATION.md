This document is a declaration of software quality for the `ros2transport` package, based on the guidelines in [REP-2004](https://www.ros.org/reps/rep-2004.html).

# `ros2transport` Quality Declaration

The package `ros2transport` claims to be in the **Quality Level 3** category, with one exception to the platform support requirement: Windows 10, a tier 1 platform of [REP-2000](https://www.ros.org/reps/rep-2000.html), is not supported (see [Platform Support](#platform-support-6)).

`ros2transport` is a thin `ros2cli` extension: `ros2 transport list`, `ros2 transport diff` and `ros2 transport codes` run the `transport_viz` binary of [`fastdds_transport_viz`](../fastdds_transport_viz/QUALITY_DECLARATION.md), which is declared at the same level and does the work.

Below are the rationales, notes and caveats for this claim, organized by each requirement listed in the [Package Requirements for Quality Level 3 in REP-2004](https://www.ros.org/reps/rep-2004.html).

## Version Policy [1]

### Version Scheme [1.i]

`ros2transport` uses `semver` according to the recommendation for ROS Core packages in the [ROS 2 Developer Guide](https://docs.ros.org/en/rolling/The-ROS2-Project/Contributing/Developer-Guide.html#versioning).
It is released together with `fastdds_transport_viz`, with the same version: the `<version>` of [`package.xml`](package.xml), a `vX.Y.Z` tag in the repository and [`CHANGELOG.rst`](CHANGELOG.rst).

### Version Stability [1.ii]

`ros2transport` is at a stable version, i.e. `>= 1.0.0`.

### Public API Declaration [1.iii]

The public API is the `ros2 transport` command line: the verbs `list`, `diff` and `codes`, their options and exit codes.
Each option is passed to `transport_viz` and means what the [`fastdds_transport_viz` quality declaration](../fastdds_transport_viz/QUALITY_DECLARATION.md#public-api-declaration-1iii) declares for it, including the `--json` document and the `--csv` columns.

The Python modules of the package are not part of the public API.

### API Stability Policy [1.iv]

Changes follow `semver` as for `fastdds_transport_viz`: removing or renaming a verb or an option, or changing the meaning of an exit code, is a major change; adding one is a minor change.

### ABI Stability Policy [1.v]

`ros2transport` is a Python package with no compiled code, so there is no ABI.

### ABI and ABI Stability Within a Released ROS Distribution [1.vi]

Not guaranteed, as for `fastdds_transport_viz`: one `main` branch serves every distribution, and a major release reaches all of them at once.

## Change Control Process [2]

`ros2transport` lives in the same repository and follows the same process as `fastdds_transport_viz`: see its [Change Control Process](../fastdds_transport_viz/QUALITY_DECLARATION.md#change-control-process-2).
All changes go through pull requests against `main`, whose required checks build and test this package on Humble, Jazzy and Lyrical in CI; Rolling is built and tested too but, being `continue-on-error`, does not block a pull request (the weekly Rolling workflow reports its breakages).

## Documentation [3]

### Feature Documentation [3.i]

Not required at Level 3. `ros2 transport` is documented in the [README](../../README.md) and the [documentation site](https://atinfinity.github.io/fastdds_transport_viz/), and by `ros2 transport <verb> --help`.

### Public API Documentation [3.ii]

Not required at Level 3; see above.

### License [3.iii]

The license for `ros2transport` is Apache License 2.0, declared in [`package.xml`](package.xml) and in the repository's [`LICENSE`](../../LICENSE) file.

### Copyright Statements [3.iv]

The package's source files carry a `Copyright 2026 atinfinity` line and an `SPDX-License-Identifier: Apache-2.0` header.

### Quality Declaration [3.v]

This document is the quality declaration. It is linked from the Quality Declaration section of the repository's [README](../../README.md#quality-declaration).
No centralized list of Level 3 packages exists, and the claim has not been through an external peer review.

## Testing [4]

Level 3 has no testing requirement. The package has, run by `colcon test` in CI:

- pytest tests of the argument parsing and of the command line passed to `transport_viz` for each verb;
- a live test that runs `ros2 transport list --json` against the `demo_nodes_cpp` talker and listener and expects SHM;
- `flake8` and `pep257`.

## Dependencies [5]

Levels are those declared by each package's quality declaration on 2026-09-22; they are the same on Humble, Jazzy and Lyrical.

### Direct Runtime ROS Dependencies [5.i]

| Dependency | Quality level | Used for |
|---|---|---|
| `fastdds_transport_viz` | [3](../fastdds_transport_viz/QUALITY_DECLARATION.md) | the `transport_viz` binary that does the work |
| `ros2cli` | none declared (5) | the `ros2` command this package extends |
| `ament_index_python` | [4](https://github.com/ament/ament_index/blob/jazzy/ament_index_python/QUALITY_DECLARATION.md) | finding the `transport_viz` binary |

`ros2cli` has no quality declaration and `ament_index_python` declares Level 4. Both are what every `ros2` command extension is built on, so there is no alternative for this package.

### Optional Direct Runtime ROS Dependencies [5.ii]

None.

### Direct Runtime non-ROS Dependencies [5.iii]

None beyond the Python standard library.

## Platform Support [6]

`ros2transport` runs wherever `transport_viz` does, so it shares its platform support and its exception: Linux only, **Windows 10 (amd64), a tier 1 platform of REP-2000, is not supported**. The supported platforms and how each one is verified are listed in the [`fastdds_transport_viz` quality declaration](../fastdds_transport_viz/QUALITY_DECLARATION.md#platform-support-6).

## Security [7]

### Vulnerability Disclosure Policy [7.i]

This package follows the repository's [security policy](../../SECURITY.md): vulnerabilities are reported privately through GitHub's private vulnerability reporting, acknowledged within 7 days, and fixed or published as a GitHub Security Advisory within 90 days. Fixes go to the latest release and `main`.
