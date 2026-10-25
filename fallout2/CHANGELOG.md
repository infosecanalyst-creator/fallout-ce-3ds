# Changelog

Nintendo 3DS port of Fallout 2 Community Edition. Releases are tagged `vX.Y.Z`;
the version is also printed in the startup banner (`src/win32.cc`).

## 0.0.47 — 2026-09-25

- CI: pipelines run on merge requests (tests/Linux builds; Windows/macOS manual;
  nothing published).

## 0.0.46 — 2026-09-24

- CI/CD moved to GitLab (`.gitlab-ci.yml`), because GitHub Actions is now disabled. On `v*` tags and manual
  runs it builds `fallout2-ce.3dsx` and `fallout2-ce.cia` in the pinned
  `devkitpro/devkitarm:20251231` image. On tags it publishes them, with
  `SHA256SUMS`, to the package registry and a GitLab release.
- SDL2, bannertool, makerom and fpattern are now built from pinned commits
  (`ci/build-3ds-deps.sh`) rather than from each branch's latest commit, so
  rebuilds of a tag are reproducible.
- A release gate (`ci/check-version.sh`) refuses a tag whose version does not
  match the startup banner.
- No gameplay or engine changes.
