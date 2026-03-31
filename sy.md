# NeoWall + WIL Constellation Integration

## Overview

NeoWall is a GPU-accelerated live wallpaper daemon for Wayland and X11, written in pure C11. It renders Shadertoy-compatible shaders directly on the desktop using EGL/OpenGL 3.3, achieving ~2% CPU usage.

This document describes the NeoWall architecture and the constellation wallpaper system added for WIL integration.

---

## NeoWall Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                         main.c                                │
│  - CLI parsing                                              │
│  - Daemonization                                           │
│  - Signal handling (signalfd for race-free operation)       │
│  - Initialization sequence                                   │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                       eventloop.c                             │
│  - Core event loop using poll()                             │
│  - File descriptors: compositor, timerfd, eventfd, signalfd │
│  - Timer-driven wallpaper cycling                            │
│  - Output redraw management                                  │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    compositor/                               │
│  - compositor_registry.c    - Backend auto-detection          │
│  - compositor_surface.c     - Surface abstraction            │
│  - backends/wayland/        - Wayland implementations        │
│  - backends/x11/           - X11 implementations            │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    shader_lib/                               │
│  - shader_core.c          - Single-pass shader loading      │
│  - shader_multipass.c     - Multi-pass (Buffer A-D → Image) │
│  - adaptive_scale.c        - Resolution scaling              │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                       render/                                │
│  - Full scene rendering                                     │
│  - Transition effects (fade, slide, glitch, pixelate)        │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                    EGL / OpenGL 3.3                          │
│  - Desktop OpenGL rendering                                 │
│  - Shader compilation and execution                          │
└─────────────────────────────────────────────────────────────┘
```

### Event Loop

The event loop (`eventloop.c:event_loop_run`) polls on multiple file descriptors:

| FD Index | Source | Purpose |
|----------|--------|---------|
| 0 | Compositor | Wayland/X11 events |
| 1 | timerfd | Wallpaper cycling timers |
| 2 | eventfd | Internal wakeup events |
| 3 | signalfd | Signal handling (SIGTERM, SIGUSR1, etc.) |
| 4+ | frame timer fds | Shader frame pacing (vsync-off mode) |

### Shader System

NeoWall supports both single-pass and multipass shaders:

```
Single Pass:     Image pass only
Multipass:       Buffer A → Buffer B → Buffer C → Buffer D → Image
```

Shadertoy uniforms are supported:
- `iResolution`, `iTime`, `iMouse`, `iFrame`, `iTimeDelta`, `iFrameRate`
- `iSampleRate`, `iDate`
- `iChannelTime[4]`, `iChannelResolution[4]`
- `iChannel0`, `iChannel1`, `iChannel2`, `iChannel3`

---

## Constellation System (WIL Integration)

### Purpose

The constellation system provides a live wallpaper that responds to WIL agent activity in real-time via Unix socket IPC. When agents work, their corresponding stars brighten and constellation lines appear.

### Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                         WIL                                  │
│  - Agents: WIL, Dubhe, Merak, Phecda, Megrez, Alioth,       │
│            Alkaid, Alcor (Big Dipper + Alcor)               │
│  - Events: agent activity, task completion                  │
└──────────────────────────────────────────────────────────────┘
                              ↓ Unix socket JSON
┌──────────────────────────────────────────────────────────────┐
│              constellation_api.c                             │
│  - Unix socket server at $XDG_RUNTIME_DIR/neowall.sock     │
│  - Non-blocking, event-driven via poll()                    │
│  - JSON message parsing (agent_update, global, ping)       │
└──────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────┐
│           constellation_state.c                              │
│  - Agent state: name, brightness, color, active, pulse      │
│  - Brightness algorithm: base + burst - decay               │
│  - Color palette by domain                                  │
│  - Uniform packing for shader                                │
└──────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────┐
│              constellation.glsl                              │
│  - 8 stars in Big Dipper formation                          │
│  - Glow, rays, constellation lines                           │
│  - Mouse interaction (magnify, attract/repel)                │
│  - Themes: minimal, deep_space                              │
└──────────────────────────────────────────────────────────────┘
```

### Agent Roster

| Agent | Star | Constellation | Domain | Color |
|-------|------|---------------|--------|-------|
| WIL | Alioth | Big Dipper | Orchestrator | Cyan |
| Dubhe | Dubhe | Big Dipper | Observer | Blue |
| Merak | Merak | Big Dipper | Memory | Purple |
| Phecda | Phecda | Big Dipper | Planner | Green |
| Megrez | Megrez | Big Dipper | Context | Yellow |
| Alioth | Alioth | Big Dipper | Executor | Orange |
| Alkaid | Alkaid | Big Dipper | Coding | Teal |
| Alcor | Alcor | Big Dipper | Social | Pink |

### Socket API

**Socket path:** `$XDG_RUNTIME_DIR/neowall.sock`

#### Message: Agent Update
```json
{
  "type": "agent_update",
  "agent": "WIL",
  "brightness": 0.9,
  "color": [0.0, 0.7, 0.9],
  "active": true,
  "pulse": false
}
```

#### Message: Global Settings
```json
{
  "type": "global",
  "theme": "minimal",
  "animation_speed": 1.0,
  "visibility": 0.9,
  "background_color": [0.02, 0.02, 0.08]
}
```

#### Message: Ping
```json
{"type": "ping"}
```
Response: `{"type":"pong","version":"0.1.0"}`

### Brightness Algorithm

```
brightness(t) = base + (cap - base) * token_factor(t)

token_factor(t) = min(1.0, tokens_recent / token_threshold)

decay: brightness -= (brightness - base) * decay_rate per frame

Constants:
  base = 0.3
  cap = 1.0
  rise_speed = 0.1
  decay_rate = 0.02
```

### Mouse Interaction

When cursor is near a star:
- **Magnification**: star grows 1.0 + exp(-dist * 8.0) * 0.5
- **Attraction**: nearby stars pulled toward cursor
- **Repulsion**: distant stars pushed away from cursor

---

## Files Added

### `include/constellation.h`
Public API for constellation system:
- `struct constellation_agent` — per-agent state
- `struct constellation_state` — global state
- Function declarations for init, cleanup, socket, setters, updates

### `src/constellation/constellation_state.c`
Agent state management (~170 lines):
- Global state with 8 default agents
- Domain-based color palette
- Brightness decay algorithm
- Uniform packing for shader

### `src/constellation/constellation_api.c`
Unix socket server (~285 lines):
- Socket creation, binding, listening
- Non-blocking accept via poll()
- JSON message parsing
- Response handling

### `examples/shaders/constellation.glsl`
MVP shader (~130 lines):
- 8 hardcoded stars in Big Dipper positions
- Glow, rays, constellation lines
- Mouse interaction
- Minimal and deep_space themes

### `config/constellation.vibe`
Config file for constellation shader:
```
default {
  shader examples/shaders/constellation.glsl
  shader_speed 1.0
}
```

---

## Files Modified

### `include/neowall.h`
Added constellation state pointer and enabled flag:
```c
struct constellation_state *constellation;
atomic_bool_t constellation_enabled;
```

### `src/main.c`
- Added `#include "constellation.h"`
- Added `constellation_state_init()` call
- Added `constellation_init(&state)` call after compositor init
- Sets `constellation_enabled` atomic flag

### `src/eventloop.c`
- Added `#include "constellation.h"`
- Added socket FD at `CONSTELLATION_FD_INDEX` (4) to poll array
- Calls `constellation_handle_socket_events()` when POLLIN on socket
- Guards with `constellation_active` atomic check

### `meson.build`
- Added `constellation_sources` file list
- Added to `all_sources`

---

## Running

### Build
```bash
meson setup build
ninja -C build
```

### Run with Constellation
```bash
./build/neowall -f -v -c config/constellation.vibe
```

### Test Socket
```bash
echo '{"type":"agent_update","agent":"WIL","brightness":0.9,"active":true}' | \
  socat - UNIX-CONNECT:/run/user/1000/neowall.sock
```

---

## Current Limitations (MVP)

1. **Shader has hardcoded agent data** — positions, colors, brightness are baked into GLSL
2. **No actual uniform passing** — C-side state exists but isn't connected to shader uniforms
3. **JSON parsing is primitive** — uses strstr/string searching, not a proper parser
4. **Single message per connection** — client connects, sends one message, disconnects

---

## Future Work

1. **Wire up uniforms** — Connect `constellation_update_uniforms()` to multipass shader system
2. **Proper JSON parser** — Use jsmn or similar for robust parsing
3. **Data texture** — Pass agent state via iChannel0 texture instead of individual uniforms
4. **Subagent support** — Moons/planets orbiting parent stars
5. **Multiple constellations** — Different sky regions for different agent groups
6. **Shooting stars** — On task completion events
7. **Pulse effects** — Visual feedback on agent activity spikes
