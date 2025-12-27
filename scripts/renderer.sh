#!/usr/bin/env bash
set -euo pipefail

# Starts the renderer_default in the foreground (requires openFrameworks; window must stay attached).
#
# Usage:
#   ./scripts/renderer.sh                 # uses defaults
#   ./scripts/renderer.sh --verbose       # enable verbose logging
# Environment overrides:
#   BUILD_DIR       Build dir containing renderer/ (default: <repo>/build)
#   RENDERER_BIN    Path to renderer_default (default: ${BUILD_DIR}/renderer/renderer_default)
#   RENDERER_HOST   Server host (default: 127.0.0.1)
#   RENDERER_PORT   Server port (default: 5050)
#   RENDERER_NAME   Renderer name (default: renderer-<pid>)
#   RENDERER_ARGS   Extra args (e.g., "--verbose")

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build"}"
RENDERER_BIN="${RENDERER_BIN:-${BUILD_DIR}/renderer/renderer_default}"
RENDERER_HOST="${RENDERER_HOST:-127.0.0.1}"
RENDERER_PORT="${RENDERER_PORT:-5050}"
RENDERER_NAME="${RENDERER_NAME:-renderer-$$}"

require_binary() {
  local bin="$1"
  local name="$2"
  if [[ ! -x "$bin" ]]; then
    echo "Missing $name binary at: $bin"
    echo "Build it first (e.g., ./scripts/build_all.sh)."
    exit 1
  fi
}

ARGS=()
for arg in "$@"; do
  case "$arg" in
    --verbose) ARGS+=("--verbose") ;;
    *) ARGS+=("$arg") ;;
  esac
done

EXTRA_RENDERER_ARGS=()
if [[ -n "${RENDERER_ARGS:-}" ]]; then
  # shellcheck disable=SC2206
  EXTRA_RENDERER_ARGS=(${RENDERER_ARGS})
fi

CMD_ARGS=("${ARGS[@]}")
if [[ ${#EXTRA_RENDERER_ARGS[@]} -gt 0 ]]; then
  CMD_ARGS+=("${EXTRA_RENDERER_ARGS[@]}")
fi

require_binary "$RENDERER_BIN" "renderer"
echo "Starting renderer (server ${RENDERER_HOST}:${RENDERER_PORT}, name ${RENDERER_NAME})..."
RENDERER_HOST="${RENDERER_HOST}" RENDERER_PORT="${RENDERER_PORT}" RENDERER_NAME="${RENDERER_NAME}" \
  exec "$RENDERER_BIN" "${CMD_ARGS[@]}"
