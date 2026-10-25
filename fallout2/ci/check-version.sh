#!/bin/sh
# Release builds must carry the release's version in the startup banner the
# 3DS build writes to its debug log (src/win32.cc), which is the only version
# string this port has. Untagged 0.0.0-<sha> builds are not checked.
#
# Usage: ci/check-version.sh <version>     e.g. ci/check-version.sh 0.0.46
set -eu

VERSION=${1:?usage: ci/check-version.sh <version>}
case "$VERSION" in
  0.0.0-*) echo "untagged build; skipping version check"; exit 0 ;;
esac

BANNER=$(sed -n 's/.*"=== Fallout 2 CE 3DS v\([^ ]*\) ===".*/\1/p' src/win32.cc)
[ -n "$BANNER" ] || { echo "no '=== Fallout 2 CE 3DS vX.Y.Z ===' banner found in src/win32.cc"; exit 1; }
[ "$BANNER" = "$VERSION" ] || { echo "src/win32.cc says v$BANNER but this release is $VERSION: bump the banner (and CHANGELOG.md) first"; exit 1; }
echo "src/win32.cc banner matches $VERSION"
