#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cmath>
#include <algorithm>
#include <vector>

#include "grid.h"
#include "particle.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#endif

/* small helper: draw an arrow from (x,y) by vector (vx,vy) */
static void drawArrow(SDL_Renderer *renderer, int x, int y, float vx, float vy) {
    float len = sqrtf(vx*vx + vy*vy);
    if (len < 1e-4f) return;
    float nx = vx / len;
    float ny = vy / len;
    int ex = (int)floorf(x + vx + 0.5f);
    int ey = (int)floorf(y + vy + 0.5f);
    SDL_RenderDrawLine(renderer, x, y, ex, ey);
    /* arrow head */
    float ah = std::min(8.0f, len * 0.35f);
    float hx1 = ex - nx * ah + -ny * (ah * 0.5f);
    float hy1 = ey - ny * ah + nx * (ah * 0.5f);
    float hx2 = ex - nx * ah - -ny * (ah * 0.5f);
    float hy2 = ey - ny * ah - nx * (ah * 0.5f);
    SDL_RenderDrawLine(renderer, ex, ey, (int)floorf(hx1+0.5f), (int)floorf(hy1+0.5f));
    SDL_RenderDrawLine(renderer, ex, ey, (int)floorf(hx2+0.5f), (int)floorf(hy2+0.5f));
}

/* draw a circle outline (approximated by line segments) */
static void drawCircleOutline(SDL_Renderer *renderer, int cx, int cy, int r) {
    if (r <= 0) return;
    int segments = std::max(12, std::min(64, r * 2));
    float theta = 0.0f;
    float dtheta = 2.0f * 3.14159265358979323846f / (float)segments;
    int prev_x = cx + (int)floorf(r * cosf(theta) + 0.5f);
    int prev_y = cy + (int)floorf(r * sinf(theta) + 0.5f);
    for (int i = 1; i <= segments; ++i) {
        theta += dtheta;
        int nx = cx + (int)floorf(r * cosf(theta) + 0.5f);
        int ny = cy + (int)floorf(r * sinf(theta) + 0.5f);
        SDL_RenderDrawLine(renderer, prev_x, prev_y, nx, ny);
        prev_x = nx; prev_y = ny;
    }
}

int main(int argc, char **argv) {
    int rows = 20;
    int cols = 30;
    int cell_size = 24;

    if (argc >= 4) {
        rows = atoi(argv[1]);
        cols = atoi(argv[2]);
        cell_size = atoi(argv[3]);
        if (rows <= 0 || cols <= 0 || cell_size <= 0) {
            fprintf(stderr, "Invalid args. Usage: %s [rows cols cell_size]\n", argv[0]);
            return 1;
        }
    }

    int window_w = cols * cell_size;
    int window_h = rows * cell_size;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("SDL Grid - Particles", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_w, window_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
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

    Grid grid(rows, cols, cell_size);
    ParticleSystem ps(2000);
    float circle_radius = 20.0f; /* used when placing collision circles with left click */
    float view_scale = 1.0f; /* visual zoom scale (1.0 = 100%) */
    bool showWindField = true;
    float wind_vis_scale = 0.05f; /* visual multiplier for wind arrows */

    /* accumulation / heatmap */
    bool showAccumulation = false;
    bool accumAutoScale = true;
    float accumMaxDisplay = 50.0f; /* manual max if auto-scale off */
    float accumDecay = 1.0f; /* per-second decay */

    /* emitter UI & interaction state */
    bool emitterSettingsOpen = false;
    float emitterWindowPosX = 100.0f, emitterWindowPosY = 100.0f;
    /* emitter world position (start centered in grid) */
    float emitterX = grid.centerX(window_w, window_h);
    float emitterY = grid.centerY(window_w, window_h);
    bool draggingEmitter = false;
    bool emitterWasDragged = false;
    bool emitterEditMode = false; /* E to toggle; arrows move when enabled */

    /* accumulation buffers sized to the configured grid */
    std::vector<float> accum(rows * cols, 0.0f);
    std::vector<int> cellCounts(rows * cols, 0);

    Uint32 last_tick = SDL_GetTicks();
    bool running = true;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
#ifdef USE_IMGUI
            ImGui_ImplSDL2_ProcessEvent(&e);
#endif
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) running = false;
                else if (k == SDLK_SPACE) ps.emitting = !ps.emitting;
                else if (k == SDLK_c) ps.clear();
                else if (k == SDLK_z) { ps.setSpawnRate(fmaxf(0.0f, ps.getSpawnRate() - 50.0f)); }
                else if (k == SDLK_x) { ps.setSpawnRate(ps.getSpawnRate() + 50.0f); }
                else if (k == SDLK_e) { emitterEditMode = !emitterEditMode; }
                else if (k == SDLK_a) { /* place emitter at mouse */
                    int mx, my; SDL_GetMouseState(&mx, &my);
                    int tmp_w, tmp_h; SDL_GetWindowSize(window, &tmp_w, &tmp_h);
                    float cx_tmp = grid.centerX(tmp_w, tmp_h);
                    float cy_tmp = grid.centerY(tmp_w, tmp_h);
                    emitterX = (mx - cx_tmp) / view_scale + cx_tmp;
                    emitterY = (my - cy_tmp) / view_scale + cy_tmp;
                }
                else if (k == SDLK_UP) {
                    if (emitterEditMode) { emitterY -= 8.0f; }
                    else { float wx, wy; ps.getWindBase(wx, wy); wy -= 50.0f; ps.setWindBase(wx, wy); }
                }
                else if (k == SDLK_DOWN) {
                    if (emitterEditMode) { emitterY += 8.0f; }
                    else { float wx, wy; ps.getWindBase(wx, wy); wy += 50.0f; ps.setWindBase(wx, wy); }
                }
                else if (k == SDLK_LEFT) {
                    if (emitterEditMode) { emitterX -= 8.0f; }
                    else { float wx, wy; ps.getWindBase(wx, wy); wx -= 50.0f; ps.setWindBase(wx, wy); }
                }
                else if (k == SDLK_RIGHT) {
                    if (emitterEditMode) { emitterX += 8.0f; }
                    else { float wx, wy; ps.getWindBase(wx, wy); wx += 50.0f; ps.setWindBase(wx, wy); }
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    /* map from screen -> world so it respects zoom */
                    int tmp_w, tmp_h; SDL_GetWindowSize(window, &tmp_w, &tmp_h);
                    float cx_tmp = grid.centerX(tmp_w, tmp_h);
                    float cy_tmp = grid.centerY(tmp_w, tmp_h);
                    float world_x = (e.button.x - cx_tmp) / view_scale + cx_tmp;
                    float world_y = (e.button.y - cy_tmp) / view_scale + cy_tmp;

                    /* start dragging emitter if click is near it */
                    float dx_e = world_x - emitterX;
                    float dy_e = world_y - emitterY;
                    float clickThresh = std::max(8.0f, ps.getEmitterRadius() * 0.5f);
                    if (!ImGui::GetIO().WantCaptureMouse && (dx_e*dx_e + dy_e*dy_e) < (clickThresh * clickThresh)) {
                        draggingEmitter = true;
                        emitterWasDragged = false;
                    } else if (!ImGui::GetIO().WantCaptureMouse) {
                        /* place a collision circle */
                        ps.addCircle(world_x, world_y, circle_radius);
                    }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    /* clear all circles */
                    ps.clearCircles();
                }
            }
            else if (e.type == SDL_MOUSEMOTION) {
                if (draggingEmitter && (e.motion.state & SDL_BUTTON_LMASK)) {
                    int tmp_w, tmp_h; SDL_GetWindowSize(window, &tmp_w, &tmp_h);
                    float cx_tmp = grid.centerX(tmp_w, tmp_h);
                    float cy_tmp = grid.centerY(tmp_w, tmp_h);
                    float world_x = (e.motion.x - cx_tmp) / view_scale + cx_tmp;
                    float world_y = (e.motion.y - cy_tmp) / view_scale + cy_tmp;
                    emitterX = world_x;
                    emitterY = world_y;
                    emitterWasDragged = true;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT && draggingEmitter) {
                    draggingEmitter = false;
                    if (!emitterWasDragged) {
                        /* treat as a click: open popup */
                        emitterSettingsOpen = !emitterSettingsOpen;
                        emitterWindowPosX = (float)e.button.x;
                        emitterWindowPosY = (float)e.button.y;
                    }
                }
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - last_tick) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        last_tick = now;

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);

        /* emitter position (can be moved/dragged) */
        /* emitterX/emitterY initialized once; keep using them */
        ps.setWindCenter(emitterX, emitterY);
        ps.update(dt, emitterX, emitterY);

        grid.draw(renderer, win_w, win_h, view_scale);

        /* bold grid boundary */
        GridRect baseR = grid.computeGridRect(win_w, win_h);
        float gcx = baseR.x + baseR.w * 0.5f;
        float gcy = baseR.y + baseR.h * 0.5f;
        int scaled_w = std::max(1, (int)floorf(baseR.w * view_scale + 0.5f));
        int scaled_h = std::max(1, (int)floorf(baseR.h * view_scale + 0.5f));
        int scaled_x = (int)floorf(gcx - scaled_w * 0.5f + 0.5f);
        int scaled_y = (int)floorf(gcy - scaled_h * 0.5f + 0.5f);
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
        SDL_Rect brect = { scaled_x, scaled_y, scaled_w, scaled_h };
        /* draw two rects for bolder boundary */
        SDL_RenderDrawRect(renderer, &brect);
        SDL_Rect brect2 = { scaled_x+1, scaled_y+1, scaled_w-2, scaled_h-2 };
        SDL_RenderDrawRect(renderer, &brect2);

        /* accumulation heatmap (decay + sample particle positions) */
        if (showAccumulation) {
            /* decay */
            float decayFactor = expf(-accumDecay * dt);
            float maxVal = 1e-6f;
            for (size_t i = 0; i < accum.size(); ++i) { accum[i] *= decayFactor; }

            /* sample particle counts into cellCounts */
            ps.accumulateGrid(baseR, rows, cols, cellCounts);
            for (size_t i = 0; i < cellCounts.size(); ++i) { accum[i] += (float)cellCounts[i]; if (accum[i] > maxVal) maxVal = accum[i]; }

            /* determine normalization */
            float normMax = accumAutoScale ? maxVal : fmaxf(1.0f, accumMaxDisplay);
            normMax = fmaxf(normMax, 1e-6f);

            /* draw overlay cells */
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            float cx = grid.centerX(win_w, win_h);
            float cy = grid.centerY(win_w, win_h);
            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    float world_x = (float)(baseR.x + c * baseR.cellSize);
                    float world_y = (float)(baseR.y + r * baseR.cellSize);
                    float sw = (float)baseR.cellSize * view_scale;
                    float sh = (float)baseR.cellSize * view_scale;
                    int sx = (int)floorf((world_x - cx) * view_scale + cx + 0.5f);
                    int sy = (int)floorf((world_y - cy) * view_scale + cy + 0.5f);
                    int sw_int = std::max(1, (int)floorf(sw + 0.5f));
                    int sh_int = std::max(1, (int)floorf(sh + 0.5f));
                    float val = accum[r * cols + c] / normMax;
                    if (val <= 0.001f) continue;
                    /* heat color: hue 240 (blue) -> 0 (red) */
                    float hue = (1.0f - fminf(1.0f, val)) * 240.0f; /* degrees */
                    float s = 1.0f, vcol = 1.0f;
                    /* convert HSV to RGB (quick) */
                    float h = hue / 60.0f;
                    int ih = (int)floorf(h) % 6;
                    float fpart = h - floorf(h);
                    float p = vcol * (1.0f - s);
                    float q = vcol * (1.0f - s * fpart);
                    float t = vcol * (1.0f - s * (1.0f - fpart));
                    float rf, gf, bf;
                    switch (ih) {
                        case 0: rf = vcol; gf = t; bf = p; break;
                        case 1: rf = q; gf = vcol; bf = p; break;
                        case 2: rf = p; gf = vcol; bf = t; break;
                        case 3: rf = p; gf = q; bf = vcol; break;
                        case 4: rf = t; gf = p; bf = vcol; break;
                        default: rf = vcol; gf = p; bf = q; break;
                    }
                    Uint8 R = (Uint8)(fminf(1.0f, rf) * 255.0f);
                    Uint8 G = (Uint8)(fminf(1.0f, gf) * 255.0f);
                    Uint8 B = (Uint8)(fminf(1.0f, bf) * 255.0f);
                    Uint8 A = (Uint8)(fminf(0.9f, val) * 180.0f);
                    SDL_SetRenderDrawColor(renderer, R, G, B, A);
                    SDL_Rect cellRect = { sx, sy, sw_int, sh_int };
                    SDL_RenderFillRect(renderer, &cellRect);
                }
            }
        }

        /* wind field visualization (sample the grid and draw little arrows) */
        if (showWindField) {
            GridRect vr = baseR;
            SDL_SetRenderDrawColor(renderer, 120, 200, 120, 255);
            float cx = grid.centerX(win_w, win_h);
            float cy = grid.centerY(win_w, win_h);
            int step = std::max(1, vr.cellSize * 2);
            for (int yy = vr.y + vr.cellSize/2; yy < vr.y + vr.h; yy += step) {
                for (int xx = vr.x + vr.cellSize/2; xx < vr.x + vr.w; xx += step) {
                    float world_x = (float)xx; /* computeGridRect returns world coords */
                    float world_y = (float)yy;
                    float fx, fy; ps.getWindAt(world_x, world_y, fx, fy);
                    float end_world_x = world_x + fx * wind_vis_scale;
                    float end_world_y = world_y + fy * wind_vis_scale;
                    int sx = (int)floorf((world_x - cx) * view_scale + cx + 0.5f);
                    int sy = (int)floorf((world_y - cy) * view_scale + cy + 0.5f);
                    int ex = (int)floorf((end_world_x - cx) * view_scale + cx + 0.5f);
                    int ey = (int)floorf((end_world_y - cy) * view_scale + cy + 0.5f);
                    drawArrow(renderer, sx, sy, (float)(ex - sx), (float)(ey - sy));
                }
            }
        }

        GridRect gr = grid.computeGridRect(win_w, win_h);
        int cs = std::max(1, (int)floorf(gr.cellSize * view_scale + 0.5f));
        int psize = std::max(1, cs / 6); /* make particles smaller */
        ps.render(renderer, psize, view_scale, grid.centerX(win_w, win_h), grid.centerY(win_w, win_h));

        /* wind vector at emitter (visualized) */
        float wx_e, wy_e; ps.getWindAt(emitterX, emitterY, wx_e, wy_e);
        SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
        float cx = grid.centerX(win_w, win_h);
        float cy = grid.centerY(win_w, win_h);
        int sx_emit_x = (int)floorf((emitterX - cx) * view_scale + cx + 0.5f);
        int sx_emit_y = (int)floorf((emitterY - cy) * view_scale + cy + 0.5f);
        float end_world_x = emitterX + wx_e * wind_vis_scale;
        float end_world_y = emitterY + wy_e * wind_vis_scale;
        int sx_end_x = (int)floorf((end_world_x - cx) * view_scale + cx + 0.5f);
        int sx_end_y = (int)floorf((end_world_y - cy) * view_scale + cy + 0.5f);
        SDL_RenderDrawLine(renderer, sx_emit_x, sx_emit_y, sx_end_x, sx_end_y);

        /* draw emitter radius boundary + center marker */
        SDL_SetRenderDrawColor(renderer, 200, 120, 120, 255);
        int emitter_sr_screen = std::max(1, (int)floorf(ps.getEmitterRadius() * view_scale + 0.5f));
        drawCircleOutline(renderer, sx_emit_x, sx_emit_y, emitter_sr_screen);
        /* small filled center */
        SDL_SetRenderDrawColor(renderer, 220, 80, 80, 255);
        SDL_Rect er = { sx_emit_x - 3, sx_emit_y - 3, 6, 6 };
        SDL_RenderFillRect(renderer, &er);
        if (emitterEditMode) {
            /* highlight when edit mode active */
            SDL_SetRenderDrawColor(renderer, 255, 200, 120, 255);
            drawCircleOutline(renderer, sx_emit_x, sx_emit_y, emitter_sr_screen + 4);
        }

#ifdef USE_IMGUI
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Simulation");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Particles: %d", ps.activeCount());
        float sr = ps.getSpawnRate();
        if (ImGui::SliderFloat("Spawn rate", &sr, 0.0f, 2000.0f)) ps.setSpawnRate(sr);
        float pl = ps.getParticleLife();
        if (ImGui::SliderFloat("Particle life", &pl, 0.1f, 10.0f)) ps.setParticleLife(pl);
        /* Wind controls (replaces gravity) */
        int wind_mode_ui = ps.getWindMode();
        const char *wind_modes[] = {"Uniform", "Vortex", "Noise"};
        if (ImGui::Combo("Wind mode", &wind_mode_ui, wind_modes, IM_ARRAYSIZE(wind_modes))) ps.setWindMode(wind_mode_ui);

        if (wind_mode_ui == ParticleSystem::WIND_UNIFORM) {
            float wx, wy; ps.getWindBase(wx, wy);
            if (ImGui::SliderFloat2("Wind (base)", &wx, -1000.0f, 1000.0f)) ps.setWindBase(wx, wy);
        } else if (wind_mode_ui == ParticleSystem::WIND_VORTEX) {
            float s = ps.getWindStrength();
            if (ImGui::SliderFloat("Vortex strength", &s, 0.0f, 2000.0f)) ps.setWindStrength(s);
            if (ImGui::Button("Center = emitter")) ps.setWindCenter(emitterX, emitterY);
        } else if (wind_mode_ui == ParticleSystem::WIND_NOISE) {
            float a, sc, sp; ps.getNoiseParams(a, sc, sp);
            if (ImGui::SliderFloat("Noise amplitude", &a, 0.0f, 1000.0f)) ;
            if (ImGui::SliderFloat("Noise scale", &sc, 0.001f, 0.1f)) ;
            if (ImGui::SliderFloat("Noise speed", &sp, 0.0f, 10.0f)) ;
            ps.setNoiseParams(a, sc, sp);
        }
        ImGui::Checkbox("Show wind field", &showWindField);
        ImGui::SliderFloat("Field visual scale", &wind_vis_scale, 0.0f, 0.5f);

        /* accumulation UI */
        ImGui::Separator();
        ImGui::Checkbox("Show accumulation heatmap", &showAccumulation);
        ImGui::SameLine();
        if (ImGui::Button("Clear accumulation")) { std::fill(accum.begin(), accum.end(), 0.0f); }
        ImGui::Checkbox("Auto-scale heatmap", &accumAutoScale);
        if (!accumAutoScale) ImGui::SliderFloat("Heatmap max", &accumMaxDisplay, 1.0f, 500.0f);
        ImGui::SliderFloat("Accumulation decay (1/s)", &accumDecay, 0.0f, 10.0f);

        int psize_ui = ps.getParticleSize();
        if (ImGui::SliderInt("Particle size", &psize_ui, 1, 64)) ps.setParticleSize(psize_ui);
        if (ImGui::SliderFloat("Circle radius", &circle_radius, 5.0f, 200.0f)) { }
        if (ImGui::SliderFloat("Scale", &view_scale, 0.25f, 4.0f)) { }
        ImGui::Text("Collision circles: %d (Left click to place, Right click clears)", ps.circleCount());
        ImGui::Checkbox("Emitting", &ps.emitting);
        if (ImGui::Button("Clear")) ps.clear();
        ImGui::SameLine();
        if (ImGui::Button("Clear circles")) ps.clearCircles();
        ImGui::Separator();
        ImGui::Text("Emitter: (click/drag to move)");
        ImGui::Text("Position: %.1f, %.1f", emitterX, emitterY);
        ImGui::SameLine();
        if (ImGui::Button("Center emitter")) { emitterX = grid.centerX(win_w, win_h); emitterY = grid.centerY(win_w, win_h); ps.setWindCenter(emitterX, emitterY); }
        ImGui::Checkbox("Emitter edit mode (E)", &emitterEditMode);
        if (ImGui::Button("Set emitter to mouse (A)")) {
            int mx, my; SDL_GetMouseState(&mx, &my);
            float cx_tmp = grid.centerX(win_w, win_h);
            float cy_tmp = grid.centerY(win_w, win_h);
            emitterX = (mx - cx_tmp) / view_scale + cx_tmp;
            emitterY = (my - cy_tmp) / view_scale + cy_tmp;
            ps.setWindCenter(emitterX, emitterY);
        }
        ImGui::End();

        /* emitter popup (opened by clicking near the emitter) */
        if (emitterSettingsOpen) {
            const float PI = 3.14159265358979323846f;
            ImGui::SetNextWindowPos(ImVec2(emitterWindowPosX, emitterWindowPosY), ImGuiCond_Once);
            ImGui::Begin("Emitter Settings", &emitterSettingsOpen, ImGuiWindowFlags_AlwaysAutoResize);
            float smin, smax; ps.getEmitterSpeedRange(smin, smax);
            if (ImGui::SliderFloat("Speed min", &smin, 0.0f, 2000.0f)) ps.setEmitterSpeedRange(smin, smax);
            if (ImGui::SliderFloat("Speed max", &smax, 0.0f, 2000.0f)) ps.setEmitterSpeedRange(smin, smax);
            float spread = ps.getEmitterSpread() * 180.0f / PI;
            if (ImGui::SliderFloat("Spread (deg)", &spread, 0.0f, 360.0f)) ps.setEmitterSpread(spread * PI / 180.0f);
            float dirDeg = ps.getEmitterDirection() * 180.0f / PI;
            if (ImGui::SliderFloat("Direction (deg)", &dirDeg, 0.0f, 360.0f)) ps.setEmitterDirection(dirDeg * PI / 180.0f);
            float er = ps.getEmitterRadius();
            if (ImGui::SliderFloat("Emitter radius", &er, 0.0f, 200.0f)) ps.setEmitterRadius(er);
            float sr_local = ps.getSpawnRate();
            if (ImGui::SliderFloat("Spawn rate", &sr_local, 0.0f, 2000.0f)) ps.setSpawnRate(sr_local);
            if (ImGui::Button("Reset defaults")) {
                ps.setEmitterSpeedRange(50.0f, 250.0f);
                ps.setEmitterSpread(6.283185307179586f);
                ps.setEmitterDirection(0.0f);
                ps.setEmitterRadius(6.0f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) emitterSettingsOpen = false;
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
    SDL_Quit();
    return 0;
}
