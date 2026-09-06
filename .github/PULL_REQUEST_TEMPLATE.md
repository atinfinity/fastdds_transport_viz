## Summary

<!-- What changed and why. Link the issue it addresses, if any (e.g. "Fixes #NN" or "Part of #NN"). -->

## Test plan

<!-- How you verified this — commands run, scenarios checked, what passed. -->

## Checklist

- [ ] `colcon test` passes locally for the affected package(s) (CI also runs this across Humble/Jazzy/Kilted/Rolling, but a local run is faster to iterate on)
- [ ] Docs updated if user-visible behavior changed (`README*.md`, `docs/*.md`, `--help` text)
- [ ] New/changed behavior is covered by a test (gtest, pytest, or a `launch_test` scenario)
