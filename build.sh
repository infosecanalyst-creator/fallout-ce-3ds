#!/usr/bin/env bash
# Build fallout-ce.3dsx (Fallout 1) and fallout2-ce.3dsx (Fallout 2) for Nintendo 3DS.
# Requires Docker. Uses the pinned devkitARM image and builds SDL2 for 3DS on first run.
#   ./build.sh            # both games
#   ./build.sh fallout2   # one game
set -euo pipefail
cd "$(dirname "$0")"
IMAGE=devkitpro/devkitarm:20251231
TC=fallout-ce-3ds-toolchain
SDL2_COMMIT=c080cc8068eef29ab0151e9916a890620acad395   # SDL2 branch, 2026-02-06

if ! docker image inspect "$TC" >/dev/null 2>&1; then
  echo "== building toolchain image ($TC) with SDL2 for 3DS"
  docker rm -f "$TC-build" >/dev/null 2>&1 || true
  docker run --name "$TC-build" "$IMAGE" bash -c "set -e
    git init -q /tmp/SDL2 && cd /tmp/SDL2
    git remote add origin https://github.com/libsdl-org/SDL.git
    git fetch -q --depth 1 origin $SDL2_COMMIT && git checkout -q FETCH_HEAD
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=\$DEVKITPRO/cmake/3DS.cmake \
      -DCMAKE_INSTALL_PREFIX=\$DEVKITPRO/portlibs/3ds -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j\$(nproc) && cmake --install build"
  docker commit "$TC-build" "$TC" >/dev/null
  docker rm "$TC-build" >/dev/null
fi

games=("${@:-fallout1 fallout2}")
mkdir -p out
for g in ${games[@]}; do
  echo "== $g"
  docker run --rm -v "$PWD:/src" -w "/src/$g" "$TC" make -f Makefile.ctr -j"$(nproc)"
  cp "$g"/*.3dsx out/
done
ls -la out/
