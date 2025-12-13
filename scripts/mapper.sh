#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"${ROOT_DIR}/build"}"
SERVER_BIN="${SERVER_BIN:-${BUILD_DIR}/server/projection_server}"
RENDERER_BIN="${RENDERER_BIN:-${BUILD_DIR}/renderer/renderer_app}"
PID_FILE="${PID_FILE:-/tmp/projection-mapper.pids}"
SERVER_LOG="${SERVER_LOG:-/tmp/projection_server.log}"
RENDERER_LOG="${RENDERER_LOG:-/tmp/renderer_app.log}"
SERVER_PORT="${SERVER_PORT:-8080}"
RENDERER_PORT="${RENDERER_PORT:-5050}"
RENDERER_HOST="${RENDERER_HOST:-127.0.0.1}"
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

ensure_running() {
  local pid="$1"
  local name="$2"
  local log_file="$3"

  sleep 0.2
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "$name failed to start (pid $pid not running). Log:"
    if [[ -f "$log_file" ]]; then
      tail -n 20 "$log_file"
    fi
    exit 1
  fi
}

start() {
  require_binary "$SERVER_BIN" "server"
  require_binary "$RENDERER_BIN" "renderer"

  if [[ -f "$PID_FILE" ]]; then
    while IFS='=' read -r name pid; do
      if [[ -n "${pid:-}" && "$(printf '%s' "$pid" | tr -d '[:space:]')" != "" ]] && kill -0 "$pid" 2>/dev/null; then
        echo "A $name process is already running (pid $pid). Stop it first with $0 stop."
        exit 1
      fi
    done < "$PID_FILE"
    rm -f "$PID_FILE"
  fi

  echo "Starting renderer (port ${RENDERER_PORT}) -> $RENDERER_LOG"
  RENDERER_PORT="${RENDERER_PORT}" "$RENDERER_BIN" ${RENDERER_ARGS:-} >"$RENDERER_LOG" 2>&1 &
  local renderer_pid=$!
  ensure_running "$renderer_pid" "renderer" "$RENDERER_LOG"
  sleep 0.3
  if ! kill -0 "$renderer_pid" 2>/dev/null; then
    echo "renderer exited early. Log:"
    if [[ -f "$RENDERER_LOG" ]]; then
      tail -n 20 "$RENDERER_LOG"
    fi
    exit 1
  fi

  local server_cmd=(
    "$SERVER_BIN"
    --db "$SERVER_DB"
    --port "$SERVER_PORT"
    --renderer-host "$RENDERER_HOST"
    --renderer-port "$RENDERER_PORT"
  )
  if [[ -n "${SERVER_ARGS:-}" ]]; then
    # shellcheck disable=SC2206
    extra_args=(${SERVER_ARGS})
    server_cmd+=("${extra_args[@]}")
  fi

  echo "Starting server (HTTP ${SERVER_PORT}, renderer ${RENDERER_HOST}:${RENDERER_PORT}) -> $SERVER_LOG"
  "${server_cmd[@]}" >"$SERVER_LOG" 2>&1 &
  local server_pid=$!
  sleep 0.2
  if ! kill -0 "$server_pid" 2>/dev/null; then
    echo "server failed to start (pid $server_pid not running). Log:"
    if [[ -f "$SERVER_LOG" ]]; then
      tail -n 20 "$SERVER_LOG"
    fi
    kill "$renderer_pid" 2>/dev/null || true
    exit 1
  fi
  sleep 0.3
  if ! kill -0 "$server_pid" 2>/dev/null; then
    echo "server exited early. Log:"
    if [[ -f "$SERVER_LOG" ]]; then
      tail -n 20 "$SERVER_LOG"
    fi
    kill "$renderer_pid" 2>/dev/null || true
    exit 1
  fi

  {
    echo "renderer=${renderer_pid}"
    echo "server=${server_pid}"
  } > "$PID_FILE"

  echo "Renderer PID: $renderer_pid"
  echo "Server PID:   $server_pid"
  echo "PID file:     $PID_FILE"
}

stop() {
  if [[ ! -f "$PID_FILE" ]]; then
    echo "No PID file found at $PID_FILE (nothing to stop)."
    return 0
  fi

  local renderer_pid=""
  local server_pid=""
  while IFS='=' read -r name pid; do
    if [[ "$name" == "renderer" ]]; then
      renderer_pid="$pid"
    elif [[ "$name" == "server" ]]; then
      server_pid="$pid"
    fi
  done < "$PID_FILE"

  stop_pid "$server_pid" "server"
  stop_pid "$renderer_pid" "renderer"

  rm -f "$PID_FILE"
  echo "Stopped. PID file removed."
}

usage() {
  cat <<EOF
Usage: $0 {start|stop}

Environment overrides:
  BUILD_DIR     Build directory containing server/ and renderer/ (default: ${BUILD_DIR})
  SERVER_BIN    Path to projection_server (default: ${SERVER_BIN})
  RENDERER_BIN  Path to renderer_app (default: ${RENDERER_BIN})
  SERVER_PORT   HTTP port for the server (default: ${SERVER_PORT})
  RENDERER_PORT TCP port for renderer (default: ${RENDERER_PORT})
  RENDERER_HOST Hostname/IP for renderer as seen by server (default: ${RENDERER_HOST})
  SERVER_DB     Path to SQLite database (default: ${SERVER_DB})
  SERVER_ARGS   Extra args for server (e.g., "--port 8080 --db ./data/db/projection.db")
  RENDERER_ARGS Extra args for renderer (e.g., "--port 5050")
  PID_FILE      File to record process IDs (default: ${PID_FILE})
  SERVER_LOG    Log file for server stdout/stderr (default: ${SERVER_LOG})
  RENDERER_LOG  Log file for renderer stdout/stderr (default: ${RENDERER_LOG})
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
