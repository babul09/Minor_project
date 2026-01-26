#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "grid.h"
#include "particle.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#endif

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
                else if (k == SDLK_UP) { float gx, gy; ps.getGravity(gx, gy); gy -= 50.0f; ps.setGravity(gx, gy); }
                else if (k == SDLK_DOWN) { float gx, gy; ps.getGravity(gx, gy); gy += 50.0f; ps.setGravity(gx, gy); }
                else if (k == SDLK_LEFT) { float gx, gy; ps.getGravity(gx, gy); gx -= 50.0f; ps.setGravity(gx, gy); }
                else if (k == SDLK_RIGHT) { float gx, gy; ps.getGravity(gx, gy); gx += 50.0f; ps.setGravity(gx, gy); }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    /* place a collision circle at mouse pos */
                    ps.addCircle((float)e.button.x, (float)e.button.y, circle_radius);
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    /* clear all circles */
                    ps.clearCircles();
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

        float emit_x = grid.centerX(win_w, win_h);
        float emit_y = grid.centerY(win_w, win_h);

        ps.update(dt, emit_x, emit_y);

        grid.draw(renderer, win_w, win_h);

        int cs = grid.computeGridRect(win_w, win_h).cellSize;
        int psize = fmax(1, cs / 6); /* make particles smaller */
        ps.render(renderer, psize);

        /* gravity vector */
        float gx, gy; ps.getGravity(gx, gy);
        SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
        int gvx = (int)(emit_x + gx * 0.05f);
        int gvy = (int)(emit_y + gy * 0.05f);
        SDL_RenderDrawLine(renderer, (int)emit_x, (int)emit_y, gvx, gvy);

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
        float gvals[2] = { gx, gy };
        if (ImGui::SliderFloat2("Gravity", gvals, -1000.0f, 1000.0f)) ps.setGravity(gvals[0], gvals[1]);
        int psize_ui = ps.getParticleSize();
        if (ImGui::SliderInt("Particle size", &psize_ui, 1, 64)) ps.setParticleSize(psize_ui);
        if (ImGui::SliderFloat("Circle radius", &circle_radius, 5.0f, 200.0f)) { }
        ImGui::Text("Collision circles: %d (Left click to place, Right click clears)", ps.circleCount());
        ImGui::Checkbox("Emitting", &ps.emitting);
        if (ImGui::Button("Clear")) ps.clear();
        ImGui::SameLine();
        if (ImGui::Button("Clear circles")) ps.clearCircles();
        ImGui::End();

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
