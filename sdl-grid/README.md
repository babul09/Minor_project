# SDL Grid Simulation

Minimal SDL2 project that draws a rectangular 2D grid — a starting point for building simulations.

## Requirements
- Linux
- SDL2 development libraries (e.g., `libsdl2-dev` on Debian/Ubuntu)

Install on Debian/Ubuntu:

```bash
sudo apt update && sudo apt install libsdl2-dev build-essential
```

## Build

```bash
make
```

## Run

```bash
./sdl-grid [rows cols cell_size]

# examples
./sdl-grid           # default 20 rows, 30 cols, 24 px initial cell size (will scale to window)
./sdl-grid 40 60 12  # 40 rows, 60 cols, initial cell size = 12 (grid auto-fits to window)
```

The window is resizable; the grid automatically scales to fit the current window resolution and stays centered. Press ESC or close the window to quit.

## Controls
- Space — toggle emitter on/off
- Z / X — decrease / increase spawn rate (particles/sec)
- Arrow keys — change global force field (gravity) vector
- C — clear all particles

Particle behavior: particles are spawned from the center of the grid with randomized velocity and a short lifespan. The global force field affects particle acceleration; adjust it with the arrow keys to see different behaviors.

## C++ & Dear ImGui (optional)
The project has been converted to C++ and modularized into `src/grid.*` and `src/particle.*`.

Dear ImGui integration is optional and is guarded by the `USE_IMGUI` compile flag. To enable ImGui:

1. Clone Dear ImGui into `third_party/imgui` (or place its sources somewhere convenient).
2. Build by passing ImGui sources to `make`. Example (from project root):

```bash
make IMGUI_SRCS="third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_widgets.cpp third_party/imgui/backends/imgui_impl_sdl.cpp third_party/imgui/backends/imgui_impl_sdlrenderer.cpp" IMGUI_DEFINES="-DUSE_IMGUI"
```

You may need to adjust include paths if your ImGui headers are not in a default include location. The app will show a small ImGui panel with sliders and stats when built with ImGui enabled.

