# WIL Constellation Wallpaper — Architecture & Design

## Overview

A live GPU-rendered constellation wallpaper that responds to WIL agent activity in real-time via Unix socket API. Built on NeoWall's shader infrastructure.

## Project Relationship

```
WIL ──(Unix socket JSON)──> NeoWall ──(EGL/OpenGL)──> Desktop
                              │
                              └── constellation.glsl (new shader)
```

- **WIL** runs as AI agent system, sends agent state updates via socket
- **NeoWall** runs as wallpaper daemon, receives socket messages, updates shader uniforms
- **constellation.glsl** renders stars, constellation lines, mouse interaction

---

## Decisions (Locked)

### 1. Communication: Unix Socket + JSON
- NeoWall opens a Unix socket server
- WIL connects as client and sends JSON messages
- Non-blocking, event-driven via `poll()`

### 2. Agent Properties Per-Star
| Property | Type | Description |
|----------|------|-------------|
| `brightness` | float (0.0-1.0) | Star luminosity |
| `color` | float[3] (RGB) | Star tint |
| `active` | bool | Show constellation lines to/from this star |
| `pulse` | bool | Trigger pulse animation (future) |

### 3. Star Positions: Hardcoded, Future Hybrid
- Fixed positions in shader based on real constellation geometry
- Space reserved for dynamic positioning when subagents spawn moons/planets
- Mouse interaction: stars near cursor magnify + attract, others repel

### 4. Global Controls
| Property | Type | Description |
|----------|------|-------------|
| `theme` | string | Visual theme (default, deep_space, aurora, etc.) |
| `animation_speed` | float | Global animation multiplier |
| `visibility` | float (0.0-1.0) | Overall brightness multiplier |
| `background_color` | float[3] | Sky background tint |

### 5. Dual-Mode System
- NeoWall runs in either **normal mode** (existing shaders) or **constellation mode**
- Config option or CLI flag to enable constellation mode
- When enabled, loads `constellation.glsl` instead of user shader

### 6. Dynamic Agent Count
- Max capacity: `MAX_AGENTS 32`
- Initial load: 8 WIL agents (WIL, Dubhe, Merak, Phecda, Megrez, Alioth, Alkaid, Alcor)
- Easily expandable for future agents

---

## Future Features (Backlog)

### Medium Priority
- [ ] Shooting star animation when agent completes a task
- [ ] Pulse effect on `pulse: true`
- [ ] Moons/planets for subagents (hybrid positioning)
- [ ] Multiple constellation groups (different regions of sky)

### Nice to Have
- [ ] Aurora/nebula background effects per theme
- [ ] Star trails (temporal feedback)
- [ ] Buffer pass for bloom/glow optimization
- [ ] Twinkle frequency per star

---

## Agent Roster (Initial)

| Agent | Star Name | Constellation | Role |
|-------|-----------|---------------|------|
| WIL | Alioth | Big Dipper | Orchestrator |
| Dubhe | Dubhe | Big Dipper | Observer |
| Merak | Merak | Big Dipper | Memory |
| Phecda | Phecda | Big Dipper | Planner |
| Megrez | Megrez | Big Dipper | Context |
| Alioth | Alioth | Big Dipper | Executor |
| Alkaid | Alkaid | Big Dipper | Coding |
| Alcor | Alcor | Big Dipper | Social |

*Note: Alcor is a double star with Mizar — historically significant for eyesight tests*

---

## API Specification

### Socket Path
`$XDG_RUNTIME_DIR/neowall/neowall.sock`

### Message Types

#### 1. Agent Update
```json
{
  "type": "agent_update",
  "agent": "WIL",
  "brightness": 0.85,
  "color": [0.2, 0.5, 1.0],
  "active": true,
  "pulse": false
}
```

#### 2. Global Update
```json
{
  "type": "global",
  "theme": "deep_space",
  "animation_speed": 1.0,
  "visibility": 0.9,
  "background_color": [0.02, 0.02, 0.08]
}
```

#### 3. Bulk Agent Update
```json
{
  "type": "agents_bulk",
  "agents": [
    {"name": "WIL", "brightness": 0.8, "color": [0.2, 0.5, 1.0], "active": true},
    {"name": "Dubhe", "brightness": 0.6, "color": [0.3, 0.4, 0.9], "active": false}
  ]
}
```

#### 4. Ping/Pong (health check)
```json
// Client -> Server
{"type": "ping"}
// Server -> Client
{"type": "pong", "version": "0.1.0"}
```

### Response (Server -> Client)
```json
{
  "type": "ack",
  "status": "ok"
}
```
or
```json
{
  "type": "error",
  "message": "unknown agent 'X'"
}
```

---

## Shader Architecture

### Single Pass (MVP)
- `constellation.glsl` — Image pass only
- Renders stars as point sprites with glow
- Constellation lines when `active: true`
- Mouse proximity affects positions (attract/repel)

### Future: Multipass
```
Buffer A ──> Image
  │           │
  └──(glow)──┘
```
- Buffer A: Star field generation at lower resolution
- Image: Final composite with glow

---

## Implementation Phases

### Phase 1: Core Infrastructure (COMPLETE)
- [x] Add Unix socket server to NeoWall event loop
- [x] JSON message parsing
- [x] Shader uniform structure for agent data
- [x] Basic constellation.glsl (stars, glow, constellation lines, mouse interaction)

### Phase 2: Full Rendering (MVP DONE)
- [x] Star glow/rays
- [x] Constellation lines (active agents)
- [x] Mouse proximity interaction (attract/repel/magnify)
- [x] Theme system (minimal, deep_space)

### Phase 3: WIL Integration
- [ ] WIL client library for sending updates
- [ ] Hook into event system (`events.ts`)
- [ ] Agent activity -> brightness mapping

### Phase 4: Polish
- [ ] Shooting stars on task completion
- [ ] Pulse effects
- [ ] Performance optimization
- [ ] Multiple constellation support (subagent moons/planets)

---

## File Structure (New Files)

```
neowall/
  src/
    constellation/
      constellation_api.c      # Unix socket server, JSON parsing
      constellation_state.c    # Agent state management
    shaders/
      constellation.glsl       # Main shader
  include/
    constellation.h            # Public API / types
  examples/
    shaders/
      constellation.glsl       # Example with themes

# In WIL (future integration)
packages/
  constellation/              # WIL-side client
    src/
      client.ts              # Socket client for neowall
      index.ts
```

---

## Technical Notes

### NeoWall Event Loop Integration
- Socket FD added to `poll()` alongside timerfd, signalfd, compositor_fd
- Non-blocking accept() on socket for clients
- Per-frame: read pending socket data, parse JSON, update shader uniforms

### Shader Uniforms
```glsl
#define MAX_AGENTS 32

uniform int agent_count;
uniform float agent_brightness[MAX_AGENTS];
uniform vec3 agent_color[MAX_AGENTS];
uniform bool agent_active[MAX_AGENTS];
uniform bool agent_pulse[MAX_AGENTS];

uniform vec2 agent_position[MAX_AGENTS];  // Future: dynamic positions

uniform float global_time;
uniform float animation_speed;
uniform float visibility;
uniform vec3 background_color;
uniform int theme_id;

uniform vec2 mouse_pos;
uniform float mouse_active;
```

### Mouse Interaction Math
```glsl
// For each star
vec2 to_star = star_pos - mouse_pos;
float dist = length(to_star);

// Magnify nearby star
float magnification = 1.0 + exp(-dist * 5.0) * 2.0;

// Repel distant stars
float repulsion = exp(-dist * 2.0) * 0.1;
vec2 repel_dir = normalize(to_star);
star_pos += repel_dir * repulsion;

// Attract nearby star
vec2 attraction = -repel_dir * exp(-dist * 8.0) * 0.05;
star_pos += attraction;
```

---

## Brightness Algorithm (Locked)

**Base + Token Burst + Decay Model:**
```
brightness(t) = base + (cap - base) * token_factor(t)

token_factor(t) = min(1.0, tokens_recent / token_threshold)

decay: every frame without tokens, brightness approaches base
       at rate: brightness -= (brightness - base) * decay_rate
```

- `base`: default brightness (e.g., 0.3)
- `cap`: maximum brightness (e.g., 1.0)
- `token_threshold`: tokens needed for full brightness (tunable)
- `decay_rate`: how fast brightness returns to base (e.g., 0.02 per frame)

**Theme: Minimal (MVP)**
- White/light gray stars on black background
- Simple point rendering with soft glow
- Constellation lines when agent is active
- Theme ID: 0

**Theme: Deep Space (v2)**
- Colored stars with subtle blue/purple tint on dark background
- Slight background gradient
- Theme ID: 1

---

## Color Palette by Domain (Locked)

| Domain | Color (RGB) | Hex |
|--------|-------------|-----|
| Orchestrator | Cyan (0.0, 0.7, 0.9) | #00b3e6 |
| Observer | Blue (0.2, 0.4, 0.9) | #3366e6 |
| Memory | Purple (0.6, 0.3, 0.8) | #994db3 |
| Planner | Green (0.2, 0.7, 0.4) | #33b366 |
| Context | Yellow (0.9, 0.7, 0.2) | #e6b333 |
| Executor | Orange (0.9, 0.4, 0.2) | #e66633 |
| Coding | Teal (0.2, 0.8, 0.7) | #33cca6 |
| Social | Pink (0.9, 0.3, 0.6) | #e64d99 |

---

## Coordinate System (Locked)

- All star positions in shader as normalized `[0.0, 1.0]` coordinates
- `uv = fragCoord / iResolution.xy` for rendering
- `star_pos = vec2(x_norm, y_norm)` in shader
- Works at any resolution

---

## Open Questions
