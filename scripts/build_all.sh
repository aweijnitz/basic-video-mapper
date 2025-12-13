#!/usr/bin/env bash
set -euo pipefail

# Build both the projection_server and renderer_app binaries.
# Usage:
#   ./scripts/build_all.sh            # builds into ./build with RelWithDebInfo
#   BUILD_DIR=/tmp/pmapper ./scripts/build_all.sh
#   BUILD_TYPE=Debug ./scripts/build_all.sh

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build"}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" --target projection_server renderer_app "$@"

echo "Built server:    ${BUILD_DIR}/server/projection_server"
echo "Built renderer:  ${BUILD_DIR}/renderer/renderer_app"
