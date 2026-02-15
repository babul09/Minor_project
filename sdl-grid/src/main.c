#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

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

    SDL_Window *window = SDL_CreateWindow("SDL Grid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_w, window_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
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

    /* Particle system setup */
    #define MAX_PARTICLES 2000
    typedef struct {
        float x, y;
        float vx, vy;
        float life; /* seconds */
    } Particle;

    Particle particles[MAX_PARTICLES];
    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].life = 0.0f;

    bool running = true;
    SDL_Event e;

    bool emitting = true;
    float spawn_rate = 300.0f; /* particles per second */
    float spawn_acc = 0.0f;
    float particle_life = 2.0f; /* seconds */

    /* base wind (replaces gravity) */
    float wind_base_x = 0.0f;
    float wind_base_y = 300.0f; /* px/s^2 downward (default matches previous gravity) */

    Uint32 last_tick = SDL_GetTicks();
    srand((unsigned int)time(NULL));

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) running = false;
                else if (k == SDLK_SPACE) emitting = !emitting;
                else if (k == SDLK_c) { /* clear particles */
                    for (int i = 0; i < MAX_PARTICLES; ++i) particles[i].life = 0.0f;
                } else if (k == SDLK_z) { spawn_rate = fmaxf(0.0f, spawn_rate - 50.0f); printf("spawn_rate=%.1f\n", spawn_rate); }
                else if (k == SDLK_x) { spawn_rate += 50.0f; printf("spawn_rate=%.1f\n", spawn_rate); }
                else if (k == SDLK_UP) { wind_base_y -= 50.0f; printf("wind=(%.1f,%.1f)\n", wind_base_x, wind_base_y); }
                else if (k == SDLK_DOWN) { wind_base_y += 50.0f; printf("wind=(%.1f,%.1f)\n", wind_base_x, wind_base_y); }
                else if (k == SDLK_LEFT) { wind_base_x -= 50.0f; printf("wind=(%.1f,%.1f)\n", wind_base_x, wind_base_y); }
                else if (k == SDLK_RIGHT) { wind_base_x += 50.0f; printf("wind=(%.1f,%.1f)\n", wind_base_x, wind_base_y); }
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - last_tick) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f; /* clamp to avoid large steps */
        last_tick = now;

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);

        /* compute cell size that fits the window while keeping square cells */
        int cs_w = win_w / cols;
        int cs_h = win_h / rows;
        int cs = cs_w < cs_h ? cs_w : cs_h;
        if (cs < 1) cs = 1;

        int grid_w = cs * cols;
        int grid_h = cs * rows;
        int offset_x = (win_w - grid_w) / 2;
        int offset_y = (win_h - grid_h) / 2;

        /* Emitter position: center of grid */
        float emit_x = offset_x + grid_w * 0.5f;
        float emit_y = offset_y + grid_h * 0.5f;

        /* spawn particles */
        if (emitting && spawn_rate > 0.0f) {
            float to_spawn = spawn_rate * dt + spawn_acc;
            int n = (int)to_spawn;
            spawn_acc = to_spawn - n;
            for (int i = 0; i < n; ++i) {
                /* find free slot */
                int idx = -1;
                for (int j = 0; j < MAX_PARTICLES; ++j) {
                    if (particles[j].life <= 0.0f) { idx = j; break; }
                }
                if (idx == -1) break; /* no space */

                float ang = ((float)rand() / RAND_MAX) * 6.283185307179586f; /* 2*pi */
                float speed = 50.0f + ((float)rand() / RAND_MAX) * 200.0f; /* px/s */
                particles[idx].x = emit_x + ((float)rand() / RAND_MAX - 0.5f) * 10.0f;
                particles[idx].y = emit_y + ((float)rand() / RAND_MAX - 0.5f) * 10.0f;
                particles[idx].vx = cosf(ang) * speed;
                particles[idx].vy = sinf(ang) * speed;
                particles[idx].life = particle_life * (0.8f + ((float)rand() / RAND_MAX) * 0.4f);
            }
        }

        /* update particles (apply uniform wind) */
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            if (particles[i].life > 0.0f) {
                particles[i].vx += wind_base_x * dt;
                particles[i].vy += wind_base_y * dt;
                particles[i].x += particles[i].vx * dt;
                particles[i].y += particles[i].vy * dt;
                particles[i].life -= dt;
            }
        }

        /* draw grid */
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        for (int r = 0; r <= rows; ++r) {
            int y = offset_y + r * cs;
            SDL_RenderDrawLine(renderer, offset_x, y, offset_x + grid_w, y);
        }
        for (int c = 0; c <= cols; ++c) {
            int x = offset_x + c * cs;
            SDL_RenderDrawLine(renderer, x, offset_y, x, offset_y + grid_h);
        }

        /* draw particles */
        int psize = fmax(1, cs / 3);
        SDL_Rect prect;
        SDL_SetRenderDrawColor(renderer, 255, 180, 60, 255);
        for (int i = 0; i < MAX_PARTICLES; ++i) {
            if (particles[i].life > 0.0f) {
                prect.w = psize; prect.h = psize;
                prect.x = (int)(particles[i].x - psize / 2);
                prect.y = (int)(particles[i].y - psize / 2);
                SDL_RenderFillRect(renderer, &prect);
            }
        }

        /* draw wind vector (visual) */
        SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
        float gv_scale = 0.05f; /* visual scale */
        int wx = (int)(emit_x + wind_base_x * gv_scale);
        int wy = (int)(emit_y + wind_base_y * gv_scale);
        SDL_RenderDrawLine(renderer, (int)emit_x, (int)emit_y, wx, wy);

        SDL_RenderPresent(renderer);
        SDL_Delay(16); /* ~60 FPS */
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
