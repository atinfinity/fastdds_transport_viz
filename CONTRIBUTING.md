# Contributing

Thanks for considering a contribution to `fastdds_transport_viz`.

## Reporting bugs / requesting features

Use the issue templates ([bug report](.github/ISSUE_TEMPLATE/bug_report.yml),
[feature request](.github/ISSUE_TEMPLATE/feature_request.yml)) — they ask for the ROS 2
distro and OS/architecture up front, which is almost always the first thing needed to
reproduce a transport/verdict issue. Usage questions not covered by the
[docs](https://atinfinity.github.io/fastdds_transport_viz/) are welcome as a blank issue.

## Development environment

See [docs/development.md](docs/development.md) for the full picture (Docker environment,
packages, verification nodes, multi-container scenarios, existing tests). The short version:

```
docker compose build
docker compose run --rm dev bash       # shell in the dev container, repo mounted at /ws
colcon build --symlink-install
source install/setup.bash
colcon test && colcon test-result --verbose
```

`ament_lint_auto` (cpplint, uncrustify, flake8, pep257, ...) is wired into the normal
`colcon test` run — there's no separate lint step or command to remember.

## Branches and pull requests

- Branch names follow `feature/<issue-number>-<slug>` for work tied to an issue, or
  `docs/<slug>` for docs-only changes.
- Open the PR against `main`; the [PR template](.github/PULL_REQUEST_TEMPLATE.md) will be
  applied automatically.
- CI (`.github/workflows/ci.yml`) builds and tests against Humble, Jazzy, Kilted and Rolling
  and is required to pass before merging.

## License

By contributing, you agree that your contributions are licensed under the project's
[Apache-2.0 license](LICENSE).
