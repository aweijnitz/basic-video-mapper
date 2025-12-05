# AGENTS.md

> **Purpose:**
> This document explains the architecture, conventions, and rules for agents and humans contributing to this project.
> **Rule 0:** If you change architecture, folder structure, or protocols, update this file in the same PR.

---

## 1. Project Summary

**Working name:** `projection-mapper`

This project is a **projection/video mapping engine** that:

- Runs on **Raspberry Pi** and **macOS**.
- Renders multiple video/graphics feeds onto skewed surfaces (rectangles, quads, meshes) in real time on a **projector**.
- Is **interactive**, driven by **MIDI**, **audio analysis (FFT)**, LFOs, and remote control via **client/server APIs**.
- Persists scenes, cues, and configuration in an **embedded SQLite3** database file and stores media assets on the filesystem.

High-level goals:

- A **core C++ library** encapsulating domain logic (scenes, surfaces, feeds, cues, playback).
- A **C++ server** exposing a **remote API** (over TCP/IP) using the core library.
- One or more **C++ clients** (CLI, UI, etc.) using that API.
- A **C++ rendering engine** that connects to the projector and executes scenes in real time.

> **Language policy:**
> Where pragmatic, **use C++** for all components (core, server, renderer, baseline clients). Other languages may be added later, but C++ is the default and must remain a first-class citizen.

---

## 2. High-Level Architecture

The repo is structured as a C++-oriented monorepo with four main components:

1. **Core Library (`/core`)**
   - Language: **C++17+**.
   - Responsibility: Domain model & logic only. No networking, no rendering, no DB.
   - Knows about: Scenes, Surfaces, Feeds, Layers, Cues, Playback state.
   - Includes: JSON serialization helpers and validation utilities for IDs and the main entities.
   - Used by: Server and (optionally) headless tools/clients.

2. **Server (`/server`)**
   - Language: **C++17+**.
   - Uses the core library to:
     - Persist state in an **embedded SQLite3** database file (default path: `./data/db/projection.db`).
     - Manage asset metadata (files are stored on the host filesystem).
     - Expose a **remote API** over TCP/IP to clients.
     - Coordinate with the **Renderer** process to apply changes & control playback.
   - The **server machine is physically connected to the projector** (via the Renderer).
   - The server exposes a minimal **HTTP+JSON** API with `/feeds` and `/scenes` endpoints for CRUD-style interactions.
   - Persistence is handled through the DB module (e.g., `db::SqliteConnection`, `db::SchemaMigrations`) and repository layer.
   - HTTP transport is implemented with the vendored single-header **cpp-httplib** server (`server/third_party/httplib.h`) inside the `http::HttpServer` wrapper.

3. **Renderer (`/renderer`)**
   - Language: **C++** using **openFrameworks** + addons (e.g. `ofxPiMapper`, `ofxMidi`, `ofxFft`).
   - Responsibility:
     - Real-time rendering of scenes on the projector display.
     - Handle MIDI input, audio input, and apply LFOs.
     - Render multiple video feeds mapped to skewed rectangles/quads/meshes.
   - Communicates with the Server via a local **control protocol** (see §4.3).

4. **Clients (`/clients/...`)**
   - Primary implementations: **C++** (e.g. CLI via standard C++ + a networking library).
   - Responsibilities:
     - Connect to the Server over TCP/IP.
     - Manage scenes, feeds, surfaces, cues, and playback.
   - Must **never talk directly to the Renderer or DB**.

### 2.1 Data Flow Overview

- **Clients → Server**
  - CRUD operations on Scenes, Surfaces, Feeds, Cues.
  - Playback control: `play`, `pause`, `gotoCue`, etc.

- **Server → SQLite3 & Assets**
  - Persists domain state in an embedded SQLite3 database file on local disk.
  - Associates logical feeds with asset paths on disk.

- **Server → Renderer**
  - Sends high-level commands:
    - “Load this Scene definition”
    - “Update Surface #23 vertices”
    - “Switch Feed of Surface #10 to `feedId=xyz`”
    - “Play / Pause / Seek cue X”
  - Renderer processes commands and updates its internal representation.

- **Renderer → Projector**
  - Fullscreen rendering window on the projector display (server machine’s GPU output).

---

## 3. Technology & Tooling Conventions

### 3.1 Languages & Toolchain

- **C++**
  - Standard: **C++17** (minimum), C++20 features allowed if guarded or agreed.
  - Build system: **CMake** (top-level project) where possible.
  - openFrameworks has its own build system; we will integrate via:
    - Dedicated CMake targets, or
    - Using openFrameworks’ project generator and documenting the linkage.
  - Compilation targets:
    - macOS (desktop).
    - Raspberry Pi (Linux/ARM).

- **SQLite3**
  - Embedded, file-based database. Default location: `./data/db/projection.db` (relative to the server working directory).
  - Access library (C++): system `sqlite3` C API (no heavy ORM). Only the **server** component may include `sqlite3.h`.
  - All DB access must be encapsulated behind a clear repository/DAO layer in `/server`.

### 3.2 Testing

> **Mandatory:** Unit tests for **all classes or modules with logic**.

- Test framework: **Catch2** (default choice) for all C++ components.
- Test naming & layout:
  - Tests live under `<component>/tests`.
  - Test files end with `_test.cpp`.
  - For small modules, tests can also live in a `tests` subdirectory next to the code.

Examples:

```text
/core/src/scene/Scene.cpp
/core/tests/scene/Scene_test.cpp

/server/src/db/SqliteConnection.cpp
/server/tests/db/SqliteConnection_test.cpp
```

### 3.3 Tooling & dependencies

- **JSON**: The `/core` library uses [nlohmann::json](https://github.com/nlohmann/json) for serialization. It is vendored as a
  single header at `core/third_party/nlohmann/json.hpp` and can be included with `<nlohmann/json.hpp>`. The serialization module
  provides the JSON conversions for IDs, enums, and the Feed/Surface/Scene/Cue classes.
- **HTTP**: The server uses the single-header [cpp-httplib](https://github.com/yhirose/cpp-httplib) library vendored at
  `server/third_party/httplib.h`.

## 4. Non-Functional Requirements

- Server code must include tests covering the DB layer, repository layer, and HTTP API handlers.

### Build notes
- The root `CMakeLists.txt` enables testing and pulls in the `/core` and `/server` subdirectories as part of the default build.
- The core library target is named `projection_core`; it is built as a C++17 target from `core/src`.
- Catch2 is provided locally under `core/third_party/catch2` (lightweight harness plus `Catch2WithMain` target) for unit tests,
  and the test executable is registered with `add_test` for ctest integration.
- Server builds must link against the system SQLite3 library and use the embedded DB file; no external DB service/container is expected.

### Dockerization notes
- Single container for the server that includes SQLite3 client library and runtime. No separate DB service is required.
- Mount `/data/db` (or `./data/db` relative to the repo) as a volume so the SQLite3 file (default `projection.db`) persists across runs.
- Expose the server’s remote API port; renderer and clients connect over the network, but DB access stays in-process.

### Open questions / TODOs
- Schema evolution for SQLite3: migrations tracked via a schema version table under `/server`.
- Validate performance on Raspberry Pi with the embedded DB file (consider WAL mode if needed in the future).
