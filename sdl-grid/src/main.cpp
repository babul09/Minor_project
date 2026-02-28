#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

#include "api_client.h"
#include "boundary.h"
#include "grid.h"
#include "particle.h"
#include <SDL2/SDL_image.h>

#ifdef USE_IMGUI
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "imgui.h"
inline bool uiWantsMouse() { return ImGui::GetIO().WantCaptureMouse; }
#else
inline bool uiWantsMouse() { return false; }
#endif

namespace {
constexpr float EPSILON = 1e-4f;
constexpr float ARROW_HEAD_SCALE = 0.35f;
constexpr float ARROW_HEAD_MAX = 8.0f;
constexpr float ARROW_HEAD_WIDTH = 0.5f;
} // namespace

// Helper function: draw an arrow from (x,y) by vector (vx,vy)
static void drawArrow(SDL_Renderer *renderer, int x, int y, float vx,
                      float vy) {
  const float len = std::sqrt(vx * vx + vy * vy);
  if (len < EPSILON)
    return;

  const float nx = vx / len;
  const float ny = vy / len;
  const int ex = static_cast<int>(std::floor(x + vx + 0.5f));
  const int ey = static_cast<int>(std::floor(y + vy + 0.5f));

  SDL_RenderDrawLine(renderer, x, y, ex, ey);

  // Arrow head
  const float ah = std::min(ARROW_HEAD_MAX, len * ARROW_HEAD_SCALE);
  const float hx1 = ex - nx * ah - ny * (ah * ARROW_HEAD_WIDTH);
  const float hy1 = ey - ny * ah + nx * (ah * ARROW_HEAD_WIDTH);
  const float hx2 = ex - nx * ah + ny * (ah * ARROW_HEAD_WIDTH);
  const float hy2 = ey - ny * ah - nx * (ah * ARROW_HEAD_WIDTH);

  SDL_RenderDrawLine(renderer, ex, ey, static_cast<int>(std::floor(hx1 + 0.5f)),
                     static_cast<int>(std::floor(hy1 + 0.5f)));
  SDL_RenderDrawLine(renderer, ex, ey, static_cast<int>(std::floor(hx2 + 0.5f)),
                     static_cast<int>(std::floor(hy2 + 0.5f)));
}

// Helper function: draw a circle outline (approximated by line segments)
static void drawCircleOutline(SDL_Renderer *renderer, int cx, int cy, int r) {
  if (r <= 0)
    return;

  constexpr float PI = 3.14159265358979323846f;
  constexpr int MIN_SEGMENTS = 12;
  constexpr int MAX_SEGMENTS = 64;

  const int segments = std::clamp(r * 2, MIN_SEGMENTS, MAX_SEGMENTS);
  const float dtheta = 2.0f * PI / static_cast<float>(segments);

  float theta = 0.0f;
  int prev_x = cx + static_cast<int>(std::floor(r * std::cos(theta) + 0.5f));
  int prev_y = cy + static_cast<int>(std::floor(r * std::sin(theta) + 0.5f));

  for (int i = 1; i <= segments; ++i) {
    theta += dtheta;
    const int nx =
        cx + static_cast<int>(std::floor(r * std::cos(theta) + 0.5f));
    const int ny =
        cy + static_cast<int>(std::floor(r * std::sin(theta) + 0.5f));
    SDL_RenderDrawLine(renderer, prev_x, prev_y, nx, ny);
    prev_x = nx;
    prev_y = ny;
  }
}

int main(int argc, char **argv) {
  int rows = 20;
  int cols = 30;
  int cell_size = 24;

  if (argc >= 4) {
    try {
      rows = std::stoi(argv[1]);
      cols = std::stoi(argv[2]);
      cell_size = std::stoi(argv[3]);
      if (rows <= 0 || cols <= 0 || cell_size <= 0) {
        throw std::invalid_argument("Values must be positive");
      }
    } catch (const std::exception &e) {
      std::cerr << "Invalid args. Usage: " << argv[0]
                << " [rows cols cell_size]\n";
      std::cerr << "Error: " << e.what() << "\n";
      return 1;
    }
  }

  const int window_w = cols * cell_size;
  const int window_h = rows * cell_size;

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
    return 1;
  }

  if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
    std::cerr << "IMG_Init Error: " << IMG_GetError() << "\n";
    return 1;
  }

  SDL_Window *window = SDL_CreateWindow(
      "SDL Grid - Particles", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      window_w, window_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
  if (!window) {
    std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << "\n";
    SDL_Quit();
    return 1;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    SDL_DestroyWindow(window);
    std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << "\n";
    SDL_Quit();
    return 1;
  }

#ifdef USE_IMGUI
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);
#endif

  // Initialize grid and particle system
  Grid grid(rows, cols, cell_size);
  ParticleSystem ps(2000);
  Boundary boundary(rows, cols); // containment boundary for particles

  // boundary editing state
  bool boundaryEditMode = false;
  bool draggingBoundary = false;
  bool boundaryMarkValue = true; // true: mark, false: unmark

  // Simulation parameters
  float circle_radius = 20.0f; // Radius for placing collision circles
  float view_scale = 1.0f;     // Visual zoom scale (1.0 = 100%)
  bool showWindField = true;
  float wind_vis_scale = 0.05f; // Visual multiplier for wind arrows

  // Accumulation / heatmap settings
  bool showAccumulation = false;
  bool accumAutoScale = true;
  float accumMaxDisplay = 50.0f; // Manual max if auto-scale is off
  float accumDecay = 1.0f;       // Per-second decay rate

  // Emitter UI & interaction state
  bool emitterSettingsOpen = false;
  float emitterWindowPosX = 100.0f, emitterWindowPosY = 100.0f;

  std::vector<std::pair<float, float>> emitters;
  emitters.push_back(
      {grid.centerX(window_w, window_h), grid.centerY(window_w, window_h)});

  int draggingEmitterIndex = -1;
  bool emitterWasDragged = false;
  bool emitterEditMode = false; // E to toggle; arrows move when enabled

  // Simulation Controls
  bool isPaused = false;

  // Accumulation buffers sized to the configured grid
  std::vector<float> accum(rows * cols, 0.0f);
  std::vector<int> cellCounts(rows * cols, 0);

  // API State and UI
  double locLat = 37.7749, locLon = -122.4194; // Default: San Francisco
  int mapZoom = 15; // Set higher default zoom for "college campus" feel
  std::string wmApiKey = "";
  std::string aqApiKey = "";
  std::string mbApiKey = "";

  // Read .env file
  std::ifstream envFile(".env");
  if (envFile.is_open()) {
    std::string line;
    while (std::getline(envFile, line)) {
      if (line.find("OPENWEATHER_API_KEY=") == 0)
        wmApiKey = line.substr(20);
      else if (line.find("OPENAQ_API_KEY=") == 0)
        aqApiKey = line.substr(15);
      else if (line.find("MAPBOX_API_KEY=") == 0)
        mbApiKey = line.substr(15);
    }
  }

  std::future<WindData> windFuture;
  std::future<PollutionData> polFuture;
  std::future<MapData> mapFuture;

  SDL_Texture *mapTexture = nullptr;
  double mapOriginX = 0, mapOriginY = 0;
  bool mapLoaded = false;
  WindData currentWind;
  PollutionData currentPol;

  Uint32 last_tick = SDL_GetTicks();
  bool running = true;
  SDL_Event e;

  while (running) {
    while (SDL_PollEvent(&e)) {
#ifdef USE_IMGUI
      ImGui_ImplSDL2_ProcessEvent(&e);
#endif
      if (e.type == SDL_QUIT)
        running = false;
      else if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_ESCAPE)
          running = false;
        else if (k == SDLK_SPACE)
          ps.emitting = !ps.emitting;
        else if (k == SDLK_c)
          ps.clear();
        else if (k == SDLK_z) {
          ps.setSpawnRate(fmaxf(0.0f, ps.getSpawnRate() - 50.0f));
        } else if (k == SDLK_x) {
          ps.setSpawnRate(ps.getSpawnRate() + 50.0f);
        } else if (k == SDLK_e) {
          emitterEditMode = !emitterEditMode;
        } else if (k == SDLK_a) { /* place emitter at mouse */
          int mx, my;
          SDL_GetMouseState(&mx, &my);
          int tmp_w, tmp_h;
          SDL_GetWindowSize(window, &tmp_w, &tmp_h);
          float cx_tmp = grid.centerX(tmp_w, tmp_h);
          float cy_tmp = grid.centerY(tmp_w, tmp_h);
          float cx_tmp2 = grid.centerX(tmp_w, tmp_h);
          float cy_tmp2 = grid.centerY(tmp_w, tmp_h);
          emitters.push_back({(mx - cx_tmp2) / view_scale + cx_tmp2,
                              (my - cy_tmp2) / view_scale + cy_tmp2});
        } else if (k == SDLK_UP) {
          if (emitterEditMode && !emitters.empty()) {
            emitters.back().second -= 8.0f;
          } else {
            float wx, wy;
            ps.getWindBase(wx, wy);
            wy -= 50.0f;
            ps.setWindBase(wx, wy);
          }
        } else if (k == SDLK_DOWN) {
          if (emitterEditMode && !emitters.empty()) {
            emitters.back().second += 8.0f;
          } else {
            float wx, wy;
            ps.getWindBase(wx, wy);
            wy += 50.0f;
            ps.setWindBase(wx, wy);
          }
        } else if (k == SDLK_LEFT) {
          if (emitterEditMode && !emitters.empty()) {
            emitters.back().first -= 8.0f;
          } else {
            float wx, wy;
            ps.getWindBase(wx, wy);
            wx -= 50.0f;
            ps.setWindBase(wx, wy);
          }
        } else if (k == SDLK_RIGHT) {
          if (emitterEditMode && !emitters.empty()) {
            emitters.back().first += 8.0f;
          } else {
            float wx, wy;
            ps.getWindBase(wx, wy);
            wx += 50.0f;
            ps.setWindBase(wx, wy);
          }
        }
      } else if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT ||
            e.button.button == SDL_BUTTON_RIGHT) {
          // Map from screen to world coordinates (respects zoom)
          int tmp_w, tmp_h;
          SDL_GetWindowSize(window, &tmp_w, &tmp_h);
          const float cx_tmp = grid.centerX(tmp_w, tmp_h);
          const float cy_tmp = grid.centerY(tmp_w, tmp_h);
          const float world_x = (e.button.x - cx_tmp) / view_scale + cx_tmp;
          const float world_y = (e.button.y - cy_tmp) / view_scale + cy_tmp;

          if (boundaryEditMode && !uiWantsMouse()) {
            // start marking or clearing boundary cells
            draggingBoundary = true;
            boundaryMarkValue = (e.button.button != SDL_BUTTON_LEFT);
            int r, c;
            if (boundary.cellAt(world_x, world_y,
                                grid.computeGridRect(tmp_w, tmp_h), r, c)) {
              boundary.setCell(r, c, boundaryMarkValue);
            }
          } else {
            // Handle emitter / circle placement as before
            const float clickThresh =
                std::max(8.0f, ps.getEmitterRadius() * 0.5f);

            bool clickedEmitter = false;
            if (!uiWantsMouse()) {
              for (size_t i = 0; i < emitters.size(); ++i) {
                const float dx_e = world_x - emitters[i].first;
                const float dy_e = world_y - emitters[i].second;
                if ((dx_e * dx_e + dy_e * dy_e) < (clickThresh * clickThresh)) {
                  draggingEmitterIndex = i;
                  emitterWasDragged = false;
                  clickedEmitter = true;
                  break;
                }
              }
            }

            if (!clickedEmitter && !uiWantsMouse() &&
                e.button.button == SDL_BUTTON_LEFT) {
              if (emitterEditMode) {
                // If in edit mode, add new emitter at click
                emitters.push_back({world_x, world_y});
              } else {
                // Place a collision circle
                ps.addCircle(world_x, world_y, circle_radius);
              }
            }
          }
        }
        // right button clears circles when not in boundary mode
        if (!boundaryEditMode && e.button.button == SDL_BUTTON_RIGHT) {
          ps.clearCircles();
        }
      } else if (e.type == SDL_MOUSEMOTION) {
        if (draggingEmitterIndex >= 0 && (e.motion.state & SDL_BUTTON_LMASK) &&
            !boundaryEditMode) {
          int tmp_w, tmp_h;
          SDL_GetWindowSize(window, &tmp_w, &tmp_h);
          const float cx_tmp = grid.centerX(tmp_w, tmp_h);
          const float cy_tmp = grid.centerY(tmp_w, tmp_h);
          const float world_x = (e.motion.x - cx_tmp) / view_scale + cx_tmp;
          const float world_y = (e.motion.y - cy_tmp) / view_scale + cy_tmp;
          if (static_cast<size_t>(draggingEmitterIndex) < emitters.size()) {
            emitters[draggingEmitterIndex].first = world_x;
            emitters[draggingEmitterIndex].second = world_y;
          }
          emitterWasDragged = true;
        }
        if (draggingBoundary &&
            (e.motion.state & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK))) {
          int tmp_w, tmp_h;
          SDL_GetWindowSize(window, &tmp_w, &tmp_h);
          const float cx_tmp = grid.centerX(tmp_w, tmp_h);
          const float cy_tmp = grid.centerY(tmp_w, tmp_h);
          const float world_x = (e.motion.x - cx_tmp) / view_scale + cx_tmp;
          const float world_y = (e.motion.y - cy_tmp) / view_scale + cy_tmp;
          int r, c;
          if (boundary.cellAt(world_x, world_y,
                              grid.computeGridRect(tmp_w, tmp_h), r, c)) {
            boundary.setCell(r, c, boundaryMarkValue);
          }
        }
      } else if (e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button == SDL_BUTTON_LEFT && draggingEmitterIndex != -1) {
          if (!emitterWasDragged) {
            // Treat as a click: open popup
            emitterSettingsOpen = !emitterSettingsOpen;
            emitterWindowPosX = static_cast<float>(e.button.x);
            emitterWindowPosY = static_cast<float>(e.button.y);
          }
          draggingEmitterIndex = -1;
        }
        if ((e.button.button == SDL_BUTTON_LEFT ||
             e.button.button == SDL_BUTTON_RIGHT) &&
            draggingBoundary) {
          draggingBoundary = false;
        }
      }
    }

    Uint32 now = SDL_GetTicks();
    float dt = (now - last_tick) / 1000.0f;
    constexpr float MAX_DT = 0.05f;
    if (dt > MAX_DT)
      dt = MAX_DT; // Clamp to avoid large time steps
    last_tick = now;

    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    if (windFuture.valid() && windFuture.wait_for(std::chrono::seconds(0)) ==
                                  std::future_status::ready) {
      currentWind = windFuture.get();
      if (currentWind.valid) {
        float zoomScaleFactor = std::pow(2.0f, mapZoom - 12.0f);
        float windScale = 20.0f * zoomScaleFactor;

        std::vector<float> uGrid;
        std::vector<float> vGrid;
        for (size_t i = 0; i < currentWind.speeds.size(); ++i) {
          float rad = currentWind.degrees[i] * (3.14159265f / 180.0f);
          uGrid.push_back(currentWind.speeds[i] * -std::sin(rad) * windScale);
          vGrid.push_back(currentWind.speeds[i] * std::cos(rad) * windScale);
        }

        ps.setRealWindGrid(uGrid, vGrid, win_w, win_h);
        if (ps.getWindMode() != ParticleSystem::WIND_REALWORLD)
          ps.setWindMode(ParticleSystem::WIND_REALWORLD);

        if (!currentWind.speeds.empty()) {
          std::cout << "Wind Grid loaded! Center node: "
                    << currentWind.speeds[4] << "m/s at "
                    << currentWind.degrees[4] << "deg\n";
        }
      } else {
        std::cerr << "Wind API Error: " << currentWind.errorMessage << "\n";
      }
    }
    if (polFuture.valid() && polFuture.wait_for(std::chrono::seconds(0)) ==
                                 std::future_status::ready) {
      currentPol = polFuture.get();
      if (currentPol.valid) {
        ps.setSpawnRate(currentPol.pm25 * 10.0f);
        std::cout << "Pollution loaded: PM2.5 = " << currentPol.pm25 << "\n";
      } else {
        std::cerr << "Pollution API Error: " << currentPol.errorMessage << "\n";
      }
    }
    if (mapFuture.valid() && mapFuture.wait_for(std::chrono::seconds(0)) ==
                                 std::future_status::ready) {
      MapData md = mapFuture.get();
      if (md.valid && !md.imageBytes.empty()) {
        SDL_RWops *rw = SDL_RWFromMem(md.imageBytes.data(),
                                      static_cast<int>(md.imageBytes.size()));
        SDL_Surface *surf = IMG_Load_RW(rw, 1);
        if (surf) {
          if (mapTexture)
            SDL_DestroyTexture(mapTexture);
          mapTexture = SDL_CreateTextureFromSurface(renderer, surf);
          SDL_FreeSurface(surf);
          mapLoaded = true;
          CoordinateTransformer::latLonToScreen(locLat, locLon, win_w, win_h,
                                                mapOriginX, mapOriginY);
          std::cout << "Map loaded successfully.\n";
        } else {
          std::cerr << "SDL_image error parsing map: " << IMG_GetError()
                    << "\n";
        }
      } else {
        std::cerr << "Map API Error: " << md.errorMessage << "\n";
      }
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    if (mapLoaded && mapTexture) {
      int w, h;
      SDL_QueryTexture(mapTexture, NULL, NULL, &w, &h);

      const float cx = win_w * 0.5f;
      const float cy = win_h * 0.5f;

      int scaled_w =
          std::max(1, static_cast<int>(std::floor(w * view_scale + 0.5f)));
      int scaled_h =
          std::max(1, static_cast<int>(std::floor(h * view_scale + 0.5f)));
      int scaled_x = static_cast<int>(std::floor(cx - scaled_w * 0.5f + 0.5f));
      int scaled_y = static_cast<int>(std::floor(cy - scaled_h * 0.5f + 0.5f));

      SDL_Rect dst = {scaled_x, scaled_y, scaled_w, scaled_h};
      SDL_RenderCopy(renderer, mapTexture, NULL, &dst);
    }

    // Compute grid rectangle once per frame
    GridRect baseR = grid.computeGridRect(win_w, win_h);

    // Update emitter position and particle system (respecting boundary)
    if (!emitters.empty()) {
      ps.setWindCenter(emitters[0].first,
                       emitters[0].second); // default vortex to first emitter
    }

    if (!isPaused) {
      ps.update(dt, emitters, boundary, baseR);
    }

    grid.draw(renderer, win_w, win_h, view_scale);

    // Draw bold grid boundary
    const float gcx = baseR.x + baseR.w * 0.5f;
    const float gcy = baseR.y + baseR.h * 0.5f;
    const int scaled_w =
        std::max(1, static_cast<int>(std::floor(baseR.w * view_scale + 0.5f)));
    const int scaled_h =
        std::max(1, static_cast<int>(std::floor(baseR.h * view_scale + 0.5f)));
    const int scaled_x =
        static_cast<int>(std::floor(gcx - scaled_w * 0.5f + 0.5f));
    const int scaled_y =
        static_cast<int>(std::floor(gcy - scaled_h * 0.5f + 0.5f));

    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_Rect brect = {scaled_x, scaled_y, scaled_w, scaled_h};
    // Draw two rects for bolder boundary
    SDL_RenderDrawRect(renderer, &brect);
    SDL_Rect brect2 = {scaled_x + 1, scaled_y + 1, scaled_w - 2, scaled_h - 2};
    SDL_RenderDrawRect(renderer, &brect2);

    // draw any user-defined containment boundary
    boundary.render(renderer, baseR, view_scale, grid.centerX(win_w, win_h),
                    grid.centerY(win_w, win_h));

    // Accumulation heatmap (decay + sample particle positions)
    if (showAccumulation) {
      // Apply exponential decay
      const float decayFactor = std::exp(-accumDecay * dt);
      float maxVal = 1e-6f;
      for (auto &val : accum) {
        val *= decayFactor;
      }

      // Sample particle counts into cellCounts
      ps.accumulateGrid(baseR, rows, cols, cellCounts);
      for (size_t i = 0; i < cellCounts.size(); ++i) {
        accum[i] += static_cast<float>(cellCounts[i]);
        if (accum[i] > maxVal)
          maxVal = accum[i];
      }

      // Determine normalization
      const float normMax = std::max(
          (accumAutoScale ? maxVal : std::max(1.0f, accumMaxDisplay)), 1e-6f);

      // Draw overlay cells
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      const float cx = grid.centerX(win_w, win_h);
      const float cy = grid.centerY(win_w, win_h);

      for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
          const float world_x =
              static_cast<float>(baseR.x + c * baseR.cellSize);
          const float world_y =
              static_cast<float>(baseR.y + r * baseR.cellSize);
          const float sw = static_cast<float>(baseR.cellSize) * view_scale;
          const float sh = static_cast<float>(baseR.cellSize) * view_scale;
          const int sx = static_cast<int>(
              std::floor((world_x - cx) * view_scale + cx + 0.5f));
          const int sy = static_cast<int>(
              std::floor((world_y - cy) * view_scale + cy + 0.5f));
          const int sw_int =
              std::max(1, static_cast<int>(std::floor(sw + 0.5f)));
          const int sh_int =
              std::max(1, static_cast<int>(std::floor(sh + 0.5f)));

          const float val = accum[r * cols + c] / normMax;
          if (val <= 0.001f)
            continue;

          // Heat color mapping: hue 240 (blue) -> 0 (red)
          const float hue = (1.0f - std::min(1.0f, val)) * 240.0f; // degrees
          constexpr float saturation = 1.0f;
          constexpr float value = 1.0f;

          // Convert HSV to RGB
          const float h = hue / 60.0f;
          const int ih = static_cast<int>(std::floor(h)) % 6;
          const float fpart = h - std::floor(h);
          const float p = value * (1.0f - saturation);
          const float q = value * (1.0f - saturation * fpart);
          const float t = value * (1.0f - saturation * (1.0f - fpart));

          float rf, gf, bf;
          switch (ih) {
          case 0:
            rf = value;
            gf = t;
            bf = p;
            break;
          case 1:
            rf = q;
            gf = value;
            bf = p;
            break;
          case 2:
            rf = p;
            gf = value;
            bf = t;
            break;
          case 3:
            rf = p;
            gf = q;
            bf = value;
            break;
          case 4:
            rf = t;
            gf = p;
            bf = value;
            break;
          default:
            rf = value;
            gf = p;
            bf = q;
            break;
          }

          const Uint8 R = static_cast<Uint8>(std::min(1.0f, rf) * 255.0f);
          const Uint8 G = static_cast<Uint8>(std::min(1.0f, gf) * 255.0f);
          const Uint8 B = static_cast<Uint8>(std::min(1.0f, bf) * 255.0f);
          const Uint8 A = static_cast<Uint8>(std::min(0.9f, val) * 180.0f);

          SDL_SetRenderDrawColor(renderer, R, G, B, A);
          SDL_Rect cellRect = {sx, sy, sw_int, sh_int};
          SDL_RenderFillRect(renderer, &cellRect);
        }
      }
    }

    // Wind field visualization (sample the grid and draw arrows)
    if (showWindField) {
      GridRect vr = baseR;
      SDL_SetRenderDrawColor(renderer, 120, 200, 120, 255);
      const float cx = grid.centerX(win_w, win_h);
      const float cy = grid.centerY(win_w, win_h);
      const int step = std::max(1, vr.cellSize * 2);

      for (int yy = vr.y + vr.cellSize / 2; yy < vr.y + vr.h; yy += step) {
        for (int xx = vr.x + vr.cellSize / 2; xx < vr.x + vr.w; xx += step) {
          const float world_x = static_cast<float>(xx);
          const float world_y = static_cast<float>(yy);

          float fx, fy;
          ps.getWindAt(world_x, world_y, fx, fy);

          const float end_world_x = world_x + fx * wind_vis_scale;
          const float end_world_y = world_y + fy * wind_vis_scale;
          const int sx = static_cast<int>(
              std::floor((world_x - cx) * view_scale + cx + 0.5f));
          const int sy = static_cast<int>(
              std::floor((world_y - cy) * view_scale + cy + 0.5f));
          const int ex = static_cast<int>(
              std::floor((end_world_x - cx) * view_scale + cx + 0.5f));
          const int ey = static_cast<int>(
              std::floor((end_world_y - cy) * view_scale + cy + 0.5f));

          drawArrow(renderer, sx, sy, static_cast<float>(ex - sx),
                    static_cast<float>(ey - sy));
        }
      }
    }

    GridRect gr = grid.computeGridRect(win_w, win_h);
    const int cs = std::max(
        1, static_cast<int>(std::floor(gr.cellSize * view_scale + 0.5f)));
    const int psize = std::max(1, cs / 6); // Make particles smaller
    ps.render(renderer, psize, view_scale, grid.centerX(win_w, win_h),
              grid.centerY(win_w, win_h));

    // Draw wind vector at emitter locations
    for (size_t i = 0; i < emitters.size(); ++i) {
      float wx_e, wy_e;
      ps.getWindAt(emitters[i].first, emitters[i].second, wx_e, wy_e);
      SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);

      const float cx = grid.centerX(win_w, win_h);
      const float cy = grid.centerY(win_w, win_h);
      const int sx_emit_x = static_cast<int>(
          std::floor((emitters[i].first - cx) * view_scale + cx + 0.5f));
      const int sx_emit_y = static_cast<int>(
          std::floor((emitters[i].second - cy) * view_scale + cy + 0.5f));
      const float end_world_x = emitters[i].first + wx_e * wind_vis_scale;
      const float end_world_y = emitters[i].second + wy_e * wind_vis_scale;
      const int sx_end_x = static_cast<int>(
          std::floor((end_world_x - cx) * view_scale + cx + 0.5f));
      const int sx_end_y = static_cast<int>(
          std::floor((end_world_y - cy) * view_scale + cy + 0.5f));
      SDL_RenderDrawLine(renderer, sx_emit_x, sx_emit_y, sx_end_x, sx_end_y);

      // Draw emitter radius boundary + center marker
      SDL_SetRenderDrawColor(renderer, 200, 120, 120, 255);
      const int emitter_sr_screen =
          std::max(1, static_cast<int>(std::floor(
                          ps.getEmitterRadius() * view_scale + 0.5f)));
      drawCircleOutline(renderer, sx_emit_x, sx_emit_y, emitter_sr_screen);

      // Small filled center
      SDL_SetRenderDrawColor(renderer, 220, 80, 80, 255);
      SDL_Rect er = {sx_emit_x - 3, sx_emit_y - 3, 6, 6};
      SDL_RenderFillRect(renderer, &er);

      if (emitterEditMode) {
        // Highlight when edit mode is active
        SDL_SetRenderDrawColor(renderer, 255, 200, 120, 255);
        drawCircleOutline(renderer, sx_emit_x, sx_emit_y,
                          emitter_sr_screen + 4);
      }
    }

#ifdef USE_IMGUI
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Simulation");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Particles: %d", ps.activeCount());
    float sr = ps.getSpawnRate();
    if (ImGui::SliderFloat("Spawn rate", &sr, 0.0f, 2000.0f))
      ps.setSpawnRate(sr);
    float pl = ps.getParticleLife();
    if (ImGui::SliderFloat("Particle life", &pl, 0.1f, 10.0f))
      ps.setParticleLife(pl);
    /* Wind controls (replaces gravity) */
    int wind_mode_ui = ps.getWindMode();
    const char *wind_modes[] = {"Uniform", "Vortex", "Noise", "Real-world"};
    if (ImGui::Combo("Wind mode", &wind_mode_ui, wind_modes,
                     IM_ARRAYSIZE(wind_modes)))
      ps.setWindMode(wind_mode_ui);

    if (wind_mode_ui == ParticleSystem::WIND_UNIFORM) {
      float wx, wy;
      ps.getWindBase(wx, wy);
      if (ImGui::SliderFloat2("Wind (base)", &wx, -1000.0f, 1000.0f))
        ps.setWindBase(wx, wy);
    } else if (wind_mode_ui == ParticleSystem::WIND_VORTEX) {
      float s = ps.getWindStrength();
      if (ImGui::SliderFloat("Vortex strength", &s, 0.0f, 2000.0f))
        ps.setWindStrength(s);
      if (ImGui::Button("Center = emitter") && !emitters.empty())
        ps.setWindCenter(emitters[0].first, emitters[0].second);
    } else if (wind_mode_ui == ParticleSystem::WIND_NOISE) {
      float a, sc, sp;
      ps.getNoiseParams(a, sc, sp);
      if (ImGui::SliderFloat("Noise amplitude", &a, 0.0f, 1000.0f)) {
        // value updated via reference
      }
      if (ImGui::SliderFloat("Noise scale", &sc, 0.001f, 0.1f)) {
        // value updated via reference
      }
      if (ImGui::SliderFloat("Noise speed", &sp, 0.0f, 10.0f)) {
        // value updated via reference
      }
      ps.setNoiseParams(a, sc, sp);
    } else if (wind_mode_ui == ParticleSystem::WIND_REALWORLD) {
      std::vector<float> u, v;
      ps.getRealWind(u, v);
      if (u.size() >= 5) {
        ImGui::Text("Real-World Wind: U=%.1f, V=%.1f", u[4], v[4]);
      } else if (!u.empty()) {
        ImGui::Text("Real-World Wind: U=%.1f, V=%.1f", u[0], v[0]);
      }
    }
    ImGui::Checkbox("Show wind field", &showWindField);

    ImGui::Separator();
    ImGui::Text("Saved Configurations");
    static char configFilenameBuf[128] = "example";
    ImGui::InputText("Filename", configFilenameBuf,
                     IM_ARRAYSIZE(configFilenameBuf));
    if (ImGui::Button("Save Config")) {
      std::string path = "configs/" + std::string(configFilenameBuf) + ".json";
      json j;
      j["locLat"] = locLat;
      j["locLon"] = locLon;
      j["mapZoom"] = mapZoom;
      j["view_scale"] = view_scale;
      j["showWindField"] = showWindField;
      j["wind_vis_scale"] = wind_vis_scale;
      j["showAccumulation"] = showAccumulation;
      j["accumAutoScale"] = accumAutoScale;
      j["accumMaxDisplay"] = accumMaxDisplay;
      j["accumDecay"] = accumDecay;

      j["ps"]["spawnRate"] = ps.getSpawnRate();
      j["ps"]["particleLife"] = ps.getParticleLife();
      j["ps"]["windMode"] = ps.getWindMode();
      j["ps"]["windStrength"] = ps.getWindStrength();
      float a, sc, sp;
      ps.getNoiseParams(a, sc, sp);
      j["ps"]["noiseAmpli"] = a;
      j["ps"]["noiseScale"] = sc;
      j["ps"]["noiseSpeed"] = sp;

      json eList = json::array();
      for (const auto &e : emitters) {
        eList.push_back({{"x", e.first}, {"y", e.second}});
      }
      j["emitters"] = eList;

      std::ofstream o(path);
      o << std::setw(4) << j << std::endl;
      std::cout << "Saved config to " << path << "\n";
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Config")) {
      std::string path = "configs/" + std::string(configFilenameBuf) + ".json";
      std::ifstream i(path);
      if (i.is_open()) {
        json j;
        i >> j;
        locLat = j.value("locLat", locLat);
        locLon = j.value("locLon", locLon);
        mapZoom = j.value("mapZoom", mapZoom);
        view_scale = j.value("view_scale", view_scale);
        showWindField = j.value("showWindField", showWindField);
        wind_vis_scale = j.value("wind_vis_scale", wind_vis_scale);
        showAccumulation = j.value("showAccumulation", showAccumulation);
        accumAutoScale = j.value("accumAutoScale", accumAutoScale);
        accumMaxDisplay = j.value("accumMaxDisplay", accumMaxDisplay);
        accumDecay = j.value("accumDecay", accumDecay);

        if (j.contains("ps")) {
          ps.setSpawnRate(j["ps"].value("spawnRate", ps.getSpawnRate()));
          ps.setParticleLife(
              j["ps"].value("particleLife", ps.getParticleLife()));
          ps.setWindMode(j["ps"].value("windMode", ps.getWindMode()));
          ps.setWindStrength(
              j["ps"].value("windStrength", ps.getWindStrength()));
          float a, sc, sp;
          ps.getNoiseParams(a, sc, sp);
          a = j["ps"].value("noiseAmpli", a);
          sc = j["ps"].value("noiseScale", sc);
          sp = j["ps"].value("noiseSpeed", sp);
          ps.setNoiseParams(a, sc, sp);
        }

        if (j.contains("emitters") && j["emitters"].is_array()) {
          emitters.clear();
          for (const auto &e : j["emitters"]) {
            if (e.contains("x") && e.contains("y")) {
              emitters.push_back({e["x"].get<float>(), e["y"].get<float>()});
            }
          }
        }

        // Auto-fetch data
        std::vector<double> gridLats;
        std::vector<double> gridLons;
        CoordinateTransformer::calculateGridCoordinates(
            locLat, locLon, mapZoom, win_w, win_h, gridLats, gridLons);
        windFuture = ApiClient::fetchWindDataGridAsync(gridLats, gridLons);
        polFuture =
            ApiClient::fetchPollutionDataAsync(locLat, locLon, aqApiKey);
        mapFuture = ApiClient::fetchMapImageAsync(locLat, locLon, win_w, win_h,
                                                  mapZoom, mbApiKey);
        std::cout << "Loaded config from " << path << "\n";
      } else {
        std::cerr << "Failed to open config file: " << path << "\n";
      }
    }

    // File Dropdown
    static std::vector<std::string> recentConfigs;
    static std::string selectedConfig = "";
    if (ImGui::BeginCombo("Recent Configs", selectedConfig.c_str())) {
      recentConfigs.clear();
      if (std::filesystem::exists("configs")) {
        std::vector<std::filesystem::path> files;
        for (const auto &entry :
             std::filesystem::directory_iterator("configs")) {
          if (entry.path().extension() == ".json") {
            files.push_back(entry.path());
          }
        }
        std::sort(
            files.begin(), files.end(),
            [](const std::filesystem::path &a, const std::filesystem::path &b) {
              return std::filesystem::last_write_time(a) >
                     std::filesystem::last_write_time(b);
            });
        for (size_t k = 0; k < std::min<size_t>(5, files.size()); ++k) {
          recentConfigs.push_back(files[k].stem().string());
        }
      }
      for (const auto &name : recentConfigs) {
        bool is_selected = (selectedConfig == name);
        if (ImGui::Selectable(name.c_str(), is_selected)) {
          selectedConfig = name;
          strncpy(configFilenameBuf, name.c_str(),
                  sizeof(configFilenameBuf) - 1);
        }
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
    ImGui::Separator();
    ImGui::Text("Real-World API Control");
    ImGui::InputDouble("Lat", &locLat);
    ImGui::InputDouble("Lon", &locLon);
    ImGui::InputInt("Zoom Level (0-22)", &mapZoom);
    if (mapZoom < 0)
      mapZoom = 0;
    if (mapZoom > 22)
      mapZoom = 22;

    if (ImGui::Button("Fetch Data")) {

      std::vector<double> gridLats;
      std::vector<double> gridLons;
      CoordinateTransformer::calculateGridCoordinates(
          locLat, locLon, mapZoom, win_w, win_h, gridLats, gridLons);

      windFuture = ApiClient::fetchWindDataGridAsync(gridLats, gridLons);
      polFuture = ApiClient::fetchPollutionDataAsync(locLat, locLon, aqApiKey);
      mapFuture = ApiClient::fetchMapImageAsync(locLat, locLon, win_w, win_h,
                                                mapZoom, mbApiKey);
    }
    if (currentWind.valid && !currentWind.speeds.empty())
      ImGui::Text("Center Wind: %.1fm/s %.1f deg", currentWind.speeds[4],
                  currentWind.degrees[4]);
    else if (!currentWind.errorMessage.empty())
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Wind Err: %s",
                         currentWind.errorMessage.c_str());

    if (currentPol.valid)
      ImGui::Text("PM2.5: %.2f", currentPol.pm25);
    else if (!currentPol.errorMessage.empty())
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "AQ Err: %s",
                         currentPol.errorMessage.c_str());
    ImGui::SliderFloat("Field visual scale", &wind_vis_scale, 0.0f, 0.5f);

    /* accumulation UI */
    ImGui::Separator();
    ImGui::Checkbox("Show accumulation heatmap", &showAccumulation);
    ImGui::SameLine();
    if (ImGui::Button("Clear accumulation")) {
      std::fill(accum.begin(), accum.end(), 0.0f);
    }
    ImGui::Checkbox("Auto-scale heatmap", &accumAutoScale);
    if (!accumAutoScale)
      ImGui::SliderFloat("Heatmap max", &accumMaxDisplay, 1.0f, 500.0f);
    ImGui::SliderFloat("Accumulation decay (1/s)", &accumDecay, 0.0f, 10.0f);

    // Boundary editing controls
    ImGui::Separator();
    ImGui::Text("Boundary");
    ImGui::Checkbox("Boundary edit mode", &boundaryEditMode);
    ImGui::SameLine();
    if (ImGui::Button("Clear boundary"))
      boundary.clear();
    ImGui::Text("Cells selected: %d", boundary.selectedCount());

    int psize_ui = ps.getParticleSize();
    if (ImGui::SliderInt("Particle size", &psize_ui, 1, 64))
      ps.setParticleSize(psize_ui);
    if (ImGui::SliderFloat("Circle radius", &circle_radius, 5.0f, 200.0f)) {
    }
    if (ImGui::SliderFloat("Scale", &view_scale, 0.25f, 4.0f)) {
    }
    ImGui::Text(
        "Collision circles: %d (Left click to place, Right click clears)",
        ps.circleCount());
    ImGui::Checkbox("Emitting", &ps.emitting);
    if (ImGui::Button("Clear"))
      ps.clear();
    ImGui::SameLine();
    if (ImGui::Button("Clear circles"))
      ps.clearCircles();
    ImGui::Separator();
    ImGui::Text("Simulation Controls");
    ImGui::Checkbox("Pause Simulation (Space)", &isPaused);
    ImGui::SameLine();
    if (ImGui::Button("Populate Grid Randomly")) {
      GridRect baseR = grid.computeGridRect(win_w, win_h);
      ps.populateRandomly(boundary, baseR);
    }
    ImGui::Separator();
    ImGui::Text("Emitters: %zu (click/drag to move)", emitters.size());
    if (ImGui::Button("Add Emitter at Center")) {
      emitters.push_back(
          {grid.centerX(win_w, win_h), grid.centerY(win_w, win_h)});
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All Emitters")) {
      emitters.clear();
    }
    ImGui::Checkbox("Emitter edit mode (E)", &emitterEditMode);
    if (ImGui::Button("Set First Emitter to mouse (A)") && !emitters.empty()) {
      int mx, my;
      SDL_GetMouseState(&mx, &my);
      float cx_tmp = grid.centerX(win_w, win_h);
      float cy_tmp = grid.centerY(win_w, win_h);
      emitters[0].first = (mx - cx_tmp) / view_scale + cx_tmp;
      emitters[0].second = (my - cy_tmp) / view_scale + cy_tmp;
      ps.setWindCenter(emitters[0].first, emitters[0].second);
    }
    ImGui::End();

    /* emitter popup (opened by clicking near the emitter) */
    if (emitterSettingsOpen) {
      const float PI = 3.14159265358979323846f;
      ImGui::SetNextWindowPos(ImVec2(emitterWindowPosX, emitterWindowPosY),
                              ImGuiCond_Once);
      ImGui::Begin("Emitter Settings", &emitterSettingsOpen,
                   ImGuiWindowFlags_AlwaysAutoResize);
      float smin, smax;
      ps.getEmitterSpeedRange(smin, smax);
      if (ImGui::SliderFloat("Speed min", &smin, 0.0f, 2000.0f))
        ps.setEmitterSpeedRange(smin, smax);
      if (ImGui::SliderFloat("Speed max", &smax, 0.0f, 2000.0f))
        ps.setEmitterSpeedRange(smin, smax);
      float spread = ps.getEmitterSpread() * 180.0f / PI;
      if (ImGui::SliderFloat("Spread (deg)", &spread, 0.0f, 360.0f))
        ps.setEmitterSpread(spread * PI / 180.0f);
      float dirDeg = ps.getEmitterDirection() * 180.0f / PI;
      if (ImGui::SliderFloat("Direction (deg)", &dirDeg, 0.0f, 360.0f))
        ps.setEmitterDirection(dirDeg * PI / 180.0f);
      float er = ps.getEmitterRadius();
      if (ImGui::SliderFloat("Emitter radius", &er, 0.0f, 200.0f))
        ps.setEmitterRadius(er);
      float sr_local = ps.getSpawnRate();
      if (ImGui::SliderFloat("Spawn rate", &sr_local, 0.0f, 2000.0f))
        ps.setSpawnRate(sr_local);
      if (ImGui::Button("Reset defaults")) {
        ps.setEmitterSpeedRange(50.0f, 250.0f);
        ps.setEmitterSpread(6.283185307179586f);
        ps.setEmitterDirection(0.0f);
        ps.setEmitterRadius(6.0f);
      }
      ImGui::SameLine();
      if (ImGui::Button("Close"))
        emitterSettingsOpen = false;
      ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
#endif

    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

#ifdef USE_IMGUI
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
#endif

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  IMG_Quit();
  SDL_Quit();
  return 0;
}
