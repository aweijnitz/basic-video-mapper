#!/bin/bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://localhost:8080}"
SERVER_SCRIPT="${SERVER_SCRIPT:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/server.sh}"
RENDERER_SCRIPT="${RENDERER_SCRIPT:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/renderer.sh}"
START_SERVER="${START_SERVER:-1}"

extract_id() {
  python3 - "$1" <<'PY'
import json, sys
payload = json.loads(sys.argv[1])
print(payload["id"])
PY
}

create_feed() {
  local name="$1"
  local path="$2"
  curl -sf -X POST "$BASE_URL/feeds" -H "Content-Type: application/json" --data-binary @- <<EOF
{"name":"$name","type":"VideoFile","configJson":{"filePath":"$path"}}
EOF
}

echo "Creating feeds..."
feed1_json=$(create_feed "Clip A" "data/assets/clipA.mp4")
feed1_id=$(extract_id "$feed1_json")
feed2_json=$(create_feed "Clip B" "data/assets/clipB.mp4")
feed2_id=$(extract_id "$feed2_json")
echo "  feed1=$feed1_id feed2=$feed2_id"

# Create a scene with two surfaces that reference the feeds and include all required fields
echo "Creating scene..."
scene_json=$(curl -sf -X POST "$BASE_URL/scenes" -H "Content-Type: application/json" --data-binary @- <<EOF
{"name":"Two Video Demo","description":"M4 walkthrough","surfaces":[{"id":"surface-a","name":"Left Quad","vertices":[{"x":-0.8,"y":-0.6},{"x":-0.1,"y":-0.5},{"x":-0.1,"y":0.2},{"x":-0.8,"y":0.1}],"feedId":"$feed1_id","opacity":1.0,"brightness":1.0,"blendMode":"Normal","zOrder":0},{"id":"surface-b","name":"Right Quad","vertices":[{"x":0.1,"y":-0.3},{"x":0.8,"y":-0.2},{"x":0.7,"y":0.5},{"x":0.0,"y":0.4}],"feedId":"$feed2_id","opacity":1.0,"brightness":1.0,"blendMode":"Normal","zOrder":1}]}
EOF
)
scene_id=$(extract_id "$scene_json")
echo "  scene=$scene_id"

# Send the full Scene + Feeds payload to the renderer
echo "Sending scene to renderer..."
curl -sf -X POST "$BASE_URL/renderer/loadScene" -H "Content-Type: application/json" -d "{\"sceneId\":\"$scene_id\"}"
echo -e "\nScene sent. Launching renderer in foreground..."

# Start renderer in foreground so the window is visible
RENDERER_ARGS="${RENDERER_ARGS:-}" RENDERER_PORT="${RENDERER_PORT:-5050}" "${RENDERER_SCRIPT}" --verbose
