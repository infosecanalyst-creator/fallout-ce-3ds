#!/usr/bin/env bash
# Builds the pieces the devkitpro/devkitarm image does not ship, then fetches
# fpattern, so `make -f Makefile.ctr` can produce both fallout2-ce.3dsx and
# fallout2-ce.cia:
#
#   SDL2       3DS port, installed into $DEVKITPRO/portlibs/3ds (the image
#              only has SDL 1.2 in its 3DS portlibs)
#   bannertool builds the .smdh icon and the .bnr banner
#   makerom    packs the .cia
#   fpattern   third_party/fpattern-src (the same commit Makefile.ctr pins)
#
# Every source is pinned to a commit so a rebuild of an old tag uses the same
# code. SDL2/bannertool/makerom are what the GitHub Actions workflow built from
# their branch tips at its last run (Feb 2026). To update one, change its
# commit here and run a manual pipeline.
#
# Usage: ci/build-3ds-deps.sh   (inside the devkitarm image; needs DEVKITPRO)
set -euo pipefail

: "${DEVKITPRO:?DEVKITPRO is not set; run this inside the devkitpro/devkitarm image}"

SDL2_URL=https://github.com/libsdl-org/SDL.git
SDL2_COMMIT=c080cc8068eef29ab0151e9916a890620acad395        # SDL2 branch, 2026-02-06
BANNERTOOL_URL=https://github.com/diasurgical/bannertool.git
BANNERTOOL_COMMIT=16d8c5a0ce02a5e06e64ab42275132fca57c04a2  # 1.2.0 (master)
PROJECT_CTR_URL=https://github.com/3DSGuy/Project_CTR.git
PROJECT_CTR_COMMIT=e8f5f529c54ff9b22a2491a480ffa69206bf7b19 # makerom 0.19.0 (master)
FPATTERN_URL=https://github.com/alexbatalov/fpattern.git
FPATTERN_COMMIT=8523173ec252c3b796fcdfca0fcc6329642fbbe3    # = FPATTERN_TAG in Makefile.ctr

WORK=${DEPS_WORK_DIR:-/tmp/3ds-deps}
JOBS=${BUILD_JOBS:-$(nproc)}
TOOLS_BIN="$DEVKITPRO/tools/bin"

# Shallow fetch of exactly one commit (GitHub serves any commit by id).
fetch() {
  local url=$1 commit=$2 dir=$3
  git init -q "$dir"
  git -C "$dir" remote add origin "$url"
  git -C "$dir" fetch -q --depth 1 origin "$commit"
  git -C "$dir" -c advice.detachedHead=false checkout -q FETCH_HEAD
  [ "$(git -C "$dir" rev-parse HEAD)" = "$commit" ] || { echo "$url: got $(git -C "$dir" rev-parse HEAD), wanted $commit" >&2; exit 1; }
}

mkdir -p "$WORK"

echo "== SDL2 $SDL2_COMMIT"
fetch "$SDL2_URL" "$SDL2_COMMIT" "$WORK/SDL2"
cmake -S "$WORK/SDL2" -B "$WORK/SDL2/build" \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" \
  -DCMAKE_INSTALL_PREFIX="$DEVKITPRO/portlibs/3ds" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/SDL2/build" -j"$JOBS"
cmake --install "$WORK/SDL2/build"

echo "== bannertool $BANNERTOOL_COMMIT"
fetch "$BANNERTOOL_URL" "$BANNERTOOL_COMMIT" "$WORK/bannertool"
git -C "$WORK/bannertool" submodule update -q --init --recursive --depth 1
make -C "$WORK/bannertool" -j"$JOBS"
install -m755 "$WORK"/bannertool/output/*/bannertool "$TOOLS_BIN/bannertool"

echo "== makerom $PROJECT_CTR_COMMIT"
fetch "$PROJECT_CTR_URL" "$PROJECT_CTR_COMMIT" "$WORK/Project_CTR"
make -C "$WORK/Project_CTR/makerom" deps
make -C "$WORK/Project_CTR/makerom" -j"$JOBS"
install -m755 "$WORK/Project_CTR/makerom/bin/makerom" "$TOOLS_BIN/makerom"

echo "== fpattern $FPATTERN_COMMIT"
if [ ! -f third_party/fpattern-src/include/fpattern/fpattern.h ]; then
  fetch "$FPATTERN_URL" "$FPATTERN_COMMIT" third_party/fpattern-src
fi

"$TOOLS_BIN/bannertool" 2>&1 | head -n 1 || true
"$TOOLS_BIN/makerom" 2>&1 | head -n 1 || true
