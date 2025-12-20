#!/usr/bin/env bash
set -euo pipefail

# Build server, renderer, command line client, and tests (requires openFrameworks).
# Usage:
#   ./scripts/build_all.sh                   # builds into ./build with RelWithDebInfo
#   ./scripts/build_all.sh --clean           # clean build directory before configuring
#   BUILD_DIR=/tmp/pmapper ./scripts/build_all.sh
#   BUILD_TYPE=Debug ./scripts/build_all.sh
# Environment:
#   OPENFRAMEWORKS_DIR=/path/to/of_v0.12.1_osx_release

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build"}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"
OPENFRAMEWORKS_DIR="${OPENFRAMEWORKS_DIR:-/Users/aweijnitz/openFrameworks/of_v0.12.1_osx_release}"

CLEAN=0
EXTRA_ARGS=()
for arg in "$@"; do
  case "$arg" in
    -c|--clean)
      CLEAN=1
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

CONFIG_ARGS=(-S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DOPENFRAMEWORKS_DIR="${OPENFRAMEWORKS_DIR}")
echo "Configuring with openFrameworks at ${OPENFRAMEWORKS_DIR}"

cmake "${CONFIG_ARGS[@]}"
TARGETS=(
  lumi_server
  renderer_default
  commandlineclient
  projection_core_tests
  lumi_server_tests
  renderer_default_tests
)

if ((${#EXTRA_ARGS[@]})); then
  cmake --build "${BUILD_DIR}" --target "${TARGETS[@]}" "${EXTRA_ARGS[@]}"
else
  cmake --build "${BUILD_DIR}" --target "${TARGETS[@]}"
fi

echo "Built server:    ${BUILD_DIR}/server/lumi_server"
echo "Built renderer:  ${BUILD_DIR}/renderer/renderer_default"
echo "Built client:    ${BUILD_DIR}/clients/commandlineclient/commandlineclient"
echo "Built tests:     ${BUILD_DIR}/core/projection_core_tests"
echo "Built tests:     ${BUILD_DIR}/server/lumi_server_tests"
echo "Built tests:     ${BUILD_DIR}/renderer/renderer_default_tests"
