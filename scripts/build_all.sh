#!/usr/bin/env bash
set -euo pipefail

# Build both the projection_server and renderer_app binaries.
# Usage:
#   ./scripts/build_all.sh                   # builds into ./build with RelWithDebInfo
#   ./scripts/build_all.sh --clean           # clean build directory before configuring
#   ./scripts/build_all.sh --with-of         # build renderer against openFrameworks
#   BUILD_DIR=/tmp/pmapper ./scripts/build_all.sh
#   BUILD_TYPE=Debug ./scripts/build_all.sh
# Environment:
#   OPENFRAMEWORKS_DIR=/path/to/of_v0.12.1_osx_release (used when --with-of is set)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build"}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
OPENFRAMEWORKS_DIR="${OPENFRAMEWORKS_DIR:-/Users/aweijnitz/openFrameworks/of_v0.12.1_osx_release}"

CLEAN=0
USE_OF=0
EXTRA_ARGS=()
for arg in "$@"; do
  case "$arg" in
    -c|--clean)
      CLEAN=1
      ;;
    --with-of)
      USE_OF=1
      ;;
    *)
      EXTRA_ARGS+=("$arg")
      ;;
  esac
done

safe_remove_build_dir() {
  local dir="$1"
  if [[ -z "${dir}" || "${dir}" == "/" || "${dir}" == "${ROOT_DIR}" ]]; then
    echo "Refusing to remove suspicious build directory: '${dir}'" >&2
    exit 1
  fi
  rm -rf "${dir}"
}

if [[ "${CLEAN}" -eq 1 ]]; then
  echo "Cleaning build directory: ${BUILD_DIR}"
  safe_remove_build_dir "${BUILD_DIR}"
else
  CACHE_FILE="${BUILD_DIR}/CMakeCache.txt"
  if [[ -f "${CACHE_FILE}" ]]; then
    CACHE_SOURCE="$(grep -E '^CMAKE_HOME_DIRECTORY:INTERNAL=' "${CACHE_FILE}" | cut -d= -f2- || true)"
    if [[ -n "${CACHE_SOURCE}" && "${CACHE_SOURCE}" != "${ROOT_DIR}" ]]; then
      echo "Removing stale CMake cache generated for ${CACHE_SOURCE} (expected ${ROOT_DIR})"
      safe_remove_build_dir "${BUILD_DIR}"
    fi
  fi
fi

CONFIG_ARGS=(-S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}")
if [[ "${USE_OF}" -eq 1 ]]; then
  CONFIG_ARGS+=(-DUSE_OPENFRAMEWORKS=ON -DOPENFRAMEWORKS_DIR="${OPENFRAMEWORKS_DIR}")
  echo "Configuring with openFrameworks at ${OPENFRAMEWORKS_DIR}"
fi

cmake "${CONFIG_ARGS[@]}"
if ((${#EXTRA_ARGS[@]})); then
  cmake --build "${BUILD_DIR}" --target projection_server renderer_app "${EXTRA_ARGS[@]}"
else
  cmake --build "${BUILD_DIR}" --target projection_server renderer_app
fi

echo "Built server:    ${BUILD_DIR}/server/projection_server"
echo "Built renderer:  ${BUILD_DIR}/renderer/renderer_app"
