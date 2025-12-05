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
- Persists scenes, cues, and configuration in a **PostgreSQL** database and stores media assets on the filesystem.

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
   - Used by: Server and (optionally) headless tools/clients.

2. **Server (`/server`)**
   - Language: **C++17+**.
   - Uses the core library to:
     - Persist state in **PostgreSQL**.
     - Manage asset metadata (files are stored on the host filesystem).
     - Expose a **remote API** over TCP/IP to clients.
     - Coordinate with the **Renderer** process to apply changes & control playback.
   - The **server machine is physically connected to the projector** (via the Renderer).

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

- **Server → PostgreSQL & Assets**
  - Persists domain state in PostgreSQL.
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

- **PostgreSQL**
  - Version: **PostgreSQL 14+** (exact version will be pinned in deployment configs).
  - Access library (C++): **TBD** (candidates: `libpqxx`, custom wrapper over `libpq`, etc.).
  - All DB access must be encapsulated behind a clear repository/DAO layer.

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

/server/src/db/PostgresConnection.cpp
/server/tests/db/PostgresConnection_test.cpp
