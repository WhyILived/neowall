# NeoWall Agent Guidelines

## Project Overview

NeoWall is a GPU-accelerated live wallpaper daemon for Wayland and X11, written in pure C11. It renders Shadertoy shaders directly on the desktop using EGL/OpenGL 3.3.

## Build Commands

### Setup and Build
```bash
# Initial setup (release mode with optimizations)
meson setup build

# Debug build (for development)
meson setup build --buildtype=debug

# Build the project
ninja -C build

# Full rebuild after clean
rm -rf build && meson setup build && ninja -C build
```

### Running
```bash
# Run the daemon
./build/neowall

# Run with verbose logging
./build/neowall -f -v

# Run with capability debugging
./build/neowall -f -v --debug-capabilities

# Run target via Meson
meson compile -C build run
```

### Code Quality Tools
```bash
# Format code (requires clang-format)
meson compile -C build format

# Static analysis (requires cppcheck)
meson compile -C build analyze

# Reconfigure build
meson setup build --wipe
```

### Backend-Specific Build Options
```bash
# Wayland only
meson setup build -Dx11_backend=disabled

# X11 only
meson setup build -Dwayland_backend=disabled
```

### Installation
```bash
# Install to system (requires root)
sudo ninja -C build install

# Install to custom DESTDIR
DESTDIR=/tmp/neowall-test ninja -C build install
```

## Code Style Guidelines

### Language Standard
- C11 (`c_std=c11` in meson.build)
- No C++ features or extensions

### Formatting
- 4-space indentation, no tabs
- Line length: aim for <100 characters, soft limit at 120
- Braces on separate lines for functions, same line for control statements
- No blank lines inside functions (except to group related logic)

### Includes
- Headers organized by category:
  1. Standard library (`<stdio.h>`, `<stdlib.h>`, etc.)
  2. External dependencies (`<EGL/egl.h>`, `<GL/gl.h>`, etc.)
  3. Project internal headers (`"neowall.h"`, `"config_access.h"`, etc.)
- Use relative paths from `include/` directory for project headers
- Source files use relative paths like `"../../include/neowall.h"`
- Include guard naming: `_<PROJECT>_<PATH>_H` (e.g., `NEOWALL_H`, `COMPOSITOR_H`)

### Naming Conventions
- **Functions**: `snake_case` (e.g., `egl_core_init`, `compositor_surface_create`)
- **Types**: `snake_case_t` suffix (e.g., `compositor_surface_config_t`, `atomic_bool_t`)
- **Constants/Macros**: `UPPER_SNAKE_CASE` (e.g., `MAX_PATH_LENGTH`, `LOG_LEVEL_ERROR`)
- **Enums**: Same as types, enum values `UPPER_SNAKE_CASE` or `SCREAMING_SNAKE_CASE`
- **Variables**: `snake_case` (e.g., `egl_display`, `output_count`)
- **Global state**: Prefix with `global_` or `state->` (e.g., `global_state`)

### Error Handling
- Functions return `bool` for success/failure where possible
- Use `log_error()`, `log_warn()`, `log_info()`, `log_debug()` for logging
- Always check return values and handle errors explicitly
- NULL checks required before dereferencing pointers
- Use `errno` and `strerror()` for system error messages

### Thread Safety
- Atomic types for flags accessed from multiple threads (`atomic_bool_t`, `atomic_int_t`)
- Mutexes (`pthread_mutex_t`) for protecting shared state
- Read-write locks (`pthread_rwlock_t`) for list traversal
- **Lock ordering policy** (from neowall.h):
  1. Always acquire `output_list_lock` (rwlock) FIRST
  2. Then acquire `state_mutex`
  3. NEVER acquire in reverse order

### Memory Management
- Check all `malloc()`/`calloc()` returns for NULL
- Free resources in reverse order of allocation
- Use valgrind-compatible patterns (matching allocators)

### Function Design
- Forward declarations for static functions at top of file
- Document function purpose, parameters, and return value in comments for public APIs
- Keep functions focused (<100 lines preferred)
- No global variables except where necessary (state management)

### Header Organization
```c
#ifndef _PROJECT_FILE_H
#define _PROJECT_FILE_H

/* Section: Includes */
#include <standard_lib>
#include <external_dep>
#include "project_header.h"

/* Section: Public types */
typedef struct { ... } public_type_t;

/* Section: Public functions */
bool public_function(void *param);

/* Section: Private (in source only) */
#endif /* _PROJECT_FILE_H */
```

### GLSL/Shaders
- GLSL version: `#version 330 core` (OpenGL 3.3 Core)
- Shader code uses Shadertoy compatibility mode
- Use `GLSL_VERSION_STRING` constant from `constants.h`

## Architecture Notes

### Directory Structure
```
src/
  main.c           # Entry point, CLI parsing, daemon logic
  eventloop.c      # Main event loop (timerfd, signalfd, eventfd)
  utils.c         # Logging, utility functions
  egl/             # EGL context management
  compositor/     # Compositor abstraction layer
    backends/
      wayland/    # Wayland-specific implementations
      x11/        # X11-specific implementations
  config/          # Configuration parsing
  textures/        # Procedural texture generation
  transitions/     # Wallpaper transition effects
  render/          # Rendering pipeline
  image/           # Image loading (libpng, libjpeg)
  shader_lib/      # Shader compilation and multipass
  output/          # Output/monitor management
  constellation/   # WIL constellation wallpaper (Unix socket API)
```

### Key Patterns
- **Compositor abstraction**: All display server interaction goes through `compositor_backend_ops` interface
- **Event-driven**: Uses `timerfd`/`signalfd`/`eventfd` instead of busy loops
- **Config is immutable**: No runtime config reload (simplicity)
- **Single binary**: No plugins, everything compiled in

## Common Issues and Debugging

### Build Failures
- Missing dependencies: See README.md Dependencies section
- Wayland not detected: Ensure `wayland-client`, `wayland-egl`, `wayland-protocols` installed
- Clean rebuild: `rm -rf build && meson setup build && ninja -C build`

### Runtime Issues
- Black window on Wayland: Missing native Wayland support, NOT XWayland
- No wallpaper showing: Check compositor compatibility, try different backend
- High CPU usage: Likely missing EGL/GL rendering path

### Debugging Tips
- Run with `-v -v` for extra debug output
- Use `--debug-capabilities` to check compositor features
- Check compositor-specific requirements (e.g., KDE Plasma needs `plasma-shell.xml`)
