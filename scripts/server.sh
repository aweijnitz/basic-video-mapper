#!/usr/bin/env bash
set -euo pipefail

# Starts/stops the lumi_server in the background. Renderer is handled separately.
#
# Usage:
#   ./scripts/server.sh start
#   ./scripts/server.sh stop
# Environment overrides:
#   BUILD_DIR     Build directory containing server/ (default: <repo>/build)
#   SERVER_BIN    Path to lumi_server (default: ${BUILD_DIR}/server/lumi_server)
#   SERVER_PORT   HTTP port (default: 8080)
#   RENDERER_PORT Renderer listen port (default: 5050)
#   SERVER_DB     SQLite DB path (default: <repo>/data/db/projection.db)
#   SERVER_ARGS   Extra args for server (e.g., "--verbose")
#   PID_FILE      File to record process ID (default: /tmp/lumi-server.pid)
#   SERVER_LOG    Log file for server stdout/stderr (default: /tmp/lumi_server.log)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build"}"
SERVER_BIN="${SERVER_BIN:-${BUILD_DIR}/server/lumi_server}"
PID_FILE="${PID_FILE:-/tmp/lumi-server.pid}"
SERVER_LOG="${SERVER_LOG:-/tmp/lumi_server.log}"
SERVER_PORT="${SERVER_PORT:-8080}"
RENDERER_PORT="${RENDERER_PORT:-5050}"
SERVER_DB="${SERVER_DB:-${ROOT_DIR}/data/db/projection.db}"

require_binary() {
  local bin="$1"
  local name="$2"
  if [[ ! -x "$bin" ]]; then
    echo "Missing $name binary at: $bin"
    echo "Build it first (e.g., ./scripts/build_all.sh)."
    exit 1
  fi
}

stop_pid() {
  local pid="$1"
  local name="$2"

  if [[ -z "$pid" ]]; then
    return
  fi

  if kill -0 "$pid" 2>/dev/null; then
    echo "Stopping $name (pid $pid)..."
    kill "$pid" 2>/dev/null || true
    for _ in {1..5}; do
      if kill -0 "$pid" 2>/dev/null; then
        sleep 1
      else
        break
      fi
    done
    if kill -0 "$pid" 2>/dev/null; then
      echo "$name still running; sending SIGKILL..."
      kill -9 "$pid" 2>/dev/null || true
    fi
  else
    echo "$name not running (pid $pid)"
  fi
}

start() {
  require_binary "$SERVER_BIN" "server"

  if [[ -f "$PID_FILE" ]]; then
    local pid
    pid="$(cat "$PID_FILE")"
    if [[ -n "${pid:-}" && "$(printf '%s' "$pid" | tr -d '[:space:]')" != "" ]] && kill -0 "$pid" 2>/dev/null; then
      echo "Server already running (pid $pid). Stop it first with $0 stop."
      exit 1
    fi
    rm -f "$PID_FILE"
  fi

  local server_cmd=(
    "$SERVER_BIN"
    --db "$SERVER_DB"
    --port "$SERVER_PORT"
    --renderer-port "$RENDERER_PORT"
  )
  if [[ -n "${SERVER_ARGS:-}" ]]; then
    # shellcheck disable=SC2206
    extra_args=(${SERVER_ARGS})
    server_cmd+=("${extra_args[@]}")
  fi

  echo "Starting server (HTTP ${SERVER_PORT}, renderer listen ${RENDERER_PORT}) -> $SERVER_LOG"
  "${server_cmd[@]}" >"$SERVER_LOG" 2>&1 &
  local server_pid=$!
  sleep 0.3
  if ! kill -0 "$server_pid" 2>/dev/null; then
    echo "server failed to start (pid $server_pid not running). Log:"
    if [[ -f "$SERVER_LOG" ]]; then
      tail -n 20 "$SERVER_LOG"
    fi
    exit 1
  fi

  echo "$server_pid" > "$PID_FILE"
  echo "Server PID: $server_pid"
  echo "PID file:   $PID_FILE"
}

stop() {
  if [[ ! -f "$PID_FILE" ]]; then
    echo "No PID file found at $PID_FILE (nothing to stop)."
    return 0
  fi

  local server_pid
  server_pid="$(cat "$PID_FILE")"
  stop_pid "$server_pid" "server"

  rm -f "$PID_FILE"
  echo "Stopped. PID file removed."
}

usage() {
  cat <<EOF
Usage: $0 {start|stop}
EOF
}

main() {
  local cmd="${1:-}"
  case "$cmd" in
    start) start ;;
    stop) stop ;;
    *) usage; exit 1 ;;
  esac
}

main "$@"
