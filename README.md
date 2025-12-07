# projection-mapper

A modular, open-source **projection/video mapping engine** written primarily in **C++**, designed to run on **Raspberry Pi** and **macOS**.

- Render multiple real-time **video feeds** onto **skewed rectangles/quads/meshes**.
- Control playback and parameters via **MIDI**, **audio analysis (FFT)**, LFOs, and **remote clients**.
- Persist **scenes, surfaces, feeds, cues** and configuration in an **embedded SQLite3** database file.
- Use a **client–server model** so the machine connected to the projector can be controlled from other devices.

> 📌 Architecture & conventions for agents and humans are documented in [`AGENTS.md`](./AGENTS.md).  
> If you change anything important in the architecture, update `AGENTS.md` in the same PR.

---

## High-Level Architecture

The project is a C++-centric monorepo with four main components:

- **`/core` – Core Library**
  - Pure C++ domain model and logic.
  - Knows about: Scenes, Surfaces, Feeds, Cues, Layers, Playback state.
  - Provides JSON serialization (via nlohmann::json) and validation helpers for the domain entities.
  - No rendering, DB, or networking dependencies.

- **`/server` – Server**
  - C++ server built on top of the core library.
  - Persists state to **SQLite3 (embedded, file-based)** and manages asset metadata.
  - Exposes a **remote API** over TCP/IP for clients.
  - Talks to the Renderer via a local **control protocol** (JSON over TCP in v0).

- **`/renderer` – Renderer**
  - C++ application using **openFrameworks**, `ofxPiMapper`, `ofxMidi`, `ofxFft`.
  - Runs on the machine that is physically connected to the **projector**.
  - Receives commands from the server and renders scenes in real time.

- **`/clients` – Clients**
  - C++ CLI and/or GUI tools.
  - Talk only to the **server** via its remote API.
  - Used to manage scenes, feeds, surfaces, cues, and playback.

Assets (images, video files, etc.) are stored on the filesystem (e.g. `./data/assets`), while structured state lives in an embedded SQLite3 database file under `./data/db`.

### Core library capabilities

- Domain classes for IDs/enums plus Feed, Surface, Scene, and Cue.
- JSON serialization/deserialization for the main entities and helper types.
- Validation helpers to confirm references between surfaces, feeds, scenes, and cues.

---

## Core domain overview

The core library models a few key entities that the server, renderer, and clients share:

- **Feed** – a source of pixels (video file, camera, generated content) with configuration metadata.
- **Surface** – a quad/mesh on the projector mapped to a particular Feed with blend/opacity controls.
- **Scene** – a collection of Surfaces configured together for playback.
- **Cue** – references a Scene and optionally overrides surface parameters (opacity/brightness) when triggered.

```mermaid
graph TD
  Feed1[Feed]
  Surface1[Surface]
  Surface2[Surface]
  Scene1[Scene]
  Cue1[Cue]

  Surface1 --> Feed1
  Surface2 --> Feed1
  Scene1 --> Surface1
  Scene1 --> Surface2
  Cue1 --> Scene1
  Cue1 --> Surface1
  Cue1 --> Surface2
```

---

## Build & runtime prerequisites

- **SQLite3** headers and library on the host (e.g., `libsqlite3-dev` on Debian/Ubuntu or Homebrew `sqlite` on macOS).
- No external database service is required; the server reads/writes a local file-backed DB at `./data/db/projection.db` by default.

## Build & Run

```bash
# Configure and build from the repository root
cmake -S . -B .
cmake --build .

# Start the server with explicit configuration
./server/projection_server --db ./data/db/projection.db --port 8080
```

- The server uses an embedded **SQLite3** database file and will create the DB on first run if it does not already exist.
- Configuration is provided via command-line flags: `--db <path>` for the SQLite file location and `--port <port>` for the HTTP listener.

Example API calls (HTTP+JSON):

```bash
curl -X POST http://localhost:8080/feeds \
  -H "Content-Type: application/json" \
  -d '{"id":"feed-1","name":"Camera","type":"Camera","configJson":"{}"}'

curl http://localhost:8080/feeds

curl -X POST http://localhost:8080/scenes \
  -H "Content-Type: application/json" \
  -d '{"id":"scene-1","name":"Main","description":"Example scene","surfaces":[]}'

curl http://localhost:8080/scenes
```

### Renderer integration (Milestone 3)

Two long-running processes now work together: the **server** (`projection_server`) and the **renderer** (`projection_renderer`).

- **Default ports**: HTTP API on **8080**; renderer TCP control on **5050**.
- **Start the renderer** (in a separate terminal):

  ```bash
  ./renderer/projection_renderer --port 5050
  ```

- **Start the server** and point it at the renderer:

  ```bash
  ./server/projection_server \
    --db ./data/db/projection.db \
    --port 8080 \
    --renderer-host 127.0.0.1 \
    --renderer-port 5050
  ```

Example renderer control calls:

```bash
# Ping the renderer (server sends hello/ack handshake)
curl -X POST http://localhost:8080/renderer/ping

# Load a scene that already exists in the server DB
curl -X POST http://localhost:8080/renderer/loadScene \
  -H "Content-Type: application/json" \
  -d '{"sceneId":"1"}'
```

For now, the renderer displays a simple visual and shows the last `sceneId` it was instructed to load via the control protocol.

### Milestone 4 demo (two videos + MIDI/audio)

Follow this minimal recipe to see the full end-to-end chain (server + renderer + control protocol + MIDI/audio input):

1. **Build both binaries**
   ```bash
   cmake -S . -B .
   cmake --build .
   ```

2. **Prepare demo assets** – place two small MP4 clips at `./data/assets/clipA.mp4` and `./data/assets/clipB.mp4`.

3. **Start the renderer** (listens for control protocol commands on TCP port 5050 by default):
   ```bash
   ./renderer/projection_renderer --port 5050
   ```

4. **Start the server** (HTTP API on 8080; configured to talk to the renderer at 127.0.0.1:5050):
   ```bash
   ./server/projection_server \
     --db ./data/db/projection.db \
     --port 8080 \
     --renderer-host 127.0.0.1 \
     --renderer-port 5050
   ```

5. **Seed feeds and a scene (two ways):**
   - **Manual calls**
     ```bash
     # Create two VideoFile feeds pointing at the prepared assets
     curl -X POST http://localhost:8080/feeds -H "Content-Type: application/json" \
       -d '{"name":"Clip A","type":"VideoFile","configJson":"{\\"filePath\\":\\"data/assets/clipA.mp4\\"}"}'
     curl -X POST http://localhost:8080/feeds -H "Content-Type: application/json" \
       -d '{"name":"Clip B","type":"VideoFile","configJson":"{\\"filePath\\":\\"data/assets/clipB.mp4\\"}"}'

     # Create a scene with two surfaces that reference the feeds and include quad vertices
     curl -X POST http://localhost:8080/scenes -H "Content-Type: application/json" \
       -d '{"name":"Two Video Demo","description":"M4 walkthrough","surfaces":[{"id":"surface-a","name":"Left Quad","vertices":[{"x":-0.8,"y":-0.6},{"x":-0.1,"y":-0.5},{"x":-0.1,"y":0.2},{"x":-0.8,"y":0.1}],"feedId":"1"},{"id":"surface-b","name":"Right Quad","vertices":[{"x":0.1,"y":-0.3},{"x":0.8,"y":-0.2},{"x":0.7,"y":0.5},{"x":0.0,"y":0.4}],"feedId":"2"}]}'

     # Send the full Scene + Feeds payload to the renderer
     curl -X POST http://localhost:8080/renderer/loadScene -H "Content-Type: application/json" -d '{"sceneId":"1"}'
     ```

   - **Demo helper endpoint** (auto-creates feeds/surfaces/scene and sends LoadSceneDefinition):
     ```bash
     curl -X POST http://localhost:8080/demo/two-video-test
     ```
     The JSON response includes the created feed and scene IDs.

6. **Observe on the projector/render window:**
   - Two separate videos should appear, each pinned to its own quad.
   - Turning MIDI CC #1 (a knob) modulates brightness.
   - Playing audio into the renderer’s input modulates the scale via FFT amplitude.

---

## Repository Layout (Initial)

```text
/AGENTS.md                 # Architecture & rules for agents and humans
/README.md                 # This file
/CMakeLists.txt            # Top-level CMake

/core/                     # Core C++ library (domain + protocol)
/core/src/...
/core/tests/...

/server/                   # C++ server using core + embedded SQLite3
/server/src/...
/server/tests/...
/server/CMakeLists.txt
/server/Dockerfile         # Server Docker image

/renderer/                 # C++ openFrameworks renderer
/renderer/src/...
/renderer/tests/...
/renderer/addons.make...   # or openFrameworks project files

/clients/                  # Client apps (CLI, GUI, etc. in C++)
/clients/cli/...
/clients/gui/...

/data/                     # Default data dir (for local dev)
/data/assets/              # Media assets
/data/tmp/                 # Temp/cache files
