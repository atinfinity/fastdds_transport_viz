# Contributing

Thanks for considering a contribution to `fastdds_transport_viz`.

## Reporting bugs / requesting features

Use the issue templates ([bug report](.github/ISSUE_TEMPLATE/bug_report.yml),
[feature request](.github/ISSUE_TEMPLATE/feature_request.yml)) — they ask for the ROS 2
distro and OS/architecture up front, which is almost always the first thing needed to
reproduce a transport/verdict issue. Usage questions not covered by the
[docs](https://atinfinity.github.io/fastdds_transport_viz/) are welcome as a blank issue.

## Development environment

See [docs/development.md](docs/development.md) for the full picture: the
[Docker environment](docs/development.md#docker-environment),
[multi-container scenarios](docs/development.md#multi-container-scenarios),
[verification nodes](docs/development.md#verification-nodes),
[tests](docs/development.md#tests) and
[continuous integration](docs/development.md#continuous-integration); the package layout is
in [docs/architecture.md](docs/architecture.md#repository-layout). The short version:

```
docker compose build
docker compose run --rm dev bash       # shell in the dev container, repo mounted at /ws
colcon build --symlink-install
source build/$ROS_DISTRO/install/setup.bash
colcon test && colcon test-result --verbose
```

`ament_lint_auto` (cpplint, uncrustify, flake8, pep257, ...) is wired into the normal
`colcon test` run — there's no separate lint step or command to remember.

The web viewer has its own tests, unit tests under Node and headless-browser tests (Node >= 22
and Chrome), and the documentation site is built strictly; both run from the repository root:

```
node --test "web/test/*.test.js"
mkdocs build --strict
```

A change that alters behavior updates the documentation (`README*.md`, `docs/*.md`, `--help`
text) and `CHANGELOG.rst` in the same pull request, with the Japanese pages (`*.ja.md`) kept in
step with the English ones.

## Branches and pull requests

- Branch names follow `feature/<issue-number>-<slug>` for work tied to an issue, or
  `docs/<slug>` for docs-only changes.
- Open the PR against `main`; the [PR template](.github/PULL_REQUEST_TEMPLATE.md) will be
  applied automatically.
- Two aggregate checks are required to pass before merging: `CI result` of
  `.github/workflows/ci.yml` (`colcon build` and `colcon test` against Humble, Jazzy, Lyrical
  and Rolling, a coverage build and the web viewer tests) and `Docs result` of
  `.github/workflows/docs.yml` (`mkdocs build --strict` and an offline link check of every
  Markdown file with lychee). Rolling runs with `continue-on-error`: a Rolling failure does
  not block a pull request, and the weekly `.github/workflows/rolling.yml` run reports it.

## License

By contributing, you agree that your contributions are licensed under the project's
[Apache-2.0 license](LICENSE).
