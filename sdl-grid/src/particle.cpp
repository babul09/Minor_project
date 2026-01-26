#include "particle.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <time.h>
#include <stdio.h>

static float randf() { return (float)rand() / (float)RAND_MAX; }

/* helper: draw a filled circle using horizontal scanlines */
static void drawFilledCircle(SDL_Renderer *renderer, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; ++dy) {
        int dx = (int)floorf(sqrtf((float)(r * r - dy * dy)));
        SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

ParticleSystem::ParticleSystem(int maxParticles_)
    : maxParticles(maxParticles_), particles(maxParticles_), spawnRate(300.0f), spawnAcc(0.0f), particleLife(2.0f), gravityX(0.0f), gravityY(300.0f), particleSize(2) {
    for (int i = 0; i < maxParticles; ++i) particles[i].life = 0.0f;
    srand((unsigned int)time(NULL));
}

void ParticleSystem::clear() {
    for (int i = 0; i < maxParticles; ++i) particles[i].life = 0.0f;
}

void ParticleSystem::spawnOne(float emit_x, float emit_y) {
    int idx = -1;
    for (int i = 0; i < maxParticles; ++i) {
        if (particles[i].life <= 0.0f) { idx = i; break; }
    }
    if (idx == -1) return;

    float ang = randf() * 6.283185307179586f;
    float speed = 50.0f + randf() * 200.0f;
    particles[idx].x = emit_x + (randf() - 0.5f) * 6.0f;
    particles[idx].y = emit_y + (randf() - 0.5f) * 6.0f;
    particles[idx].vx = cosf(ang) * speed;
    particles[idx].vy = sinf(ang) * speed;
    particles[idx].life = particleLife * (0.8f + randf() * 0.4f);
}

void ParticleSystem::update(float dt, float emit_x, float emit_y) {
    if (emitting && spawnRate > 0.0f) {
        float to_spawn = spawnRate * dt + spawnAcc;
        int n = (int)to_spawn;
        spawnAcc = to_spawn - n;
        for (int i = 0; i < n; ++i) spawnOne(emit_x, emit_y);
    }

    for (int i = 0; i < maxParticles; ++i) {
        if (particles[i].life > 0.0f) {
            particles[i].vx += gravityX * dt;
            particles[i].vy += gravityY * dt;
            particles[i].x += particles[i].vx * dt;
            particles[i].y += particles[i].vy * dt;

            /* simple circle collisions: reflect velocity when inside a circle */
            for (const Circle &c : circles) {
                float rx = particles[i].x - c.x;
                float ry = particles[i].y - c.y;
                float dist2 = rx * rx + ry * ry;
                float r2 = c.r * c.r;
                if (dist2 < r2 && dist2 > 0.0f) {
                    float dist = sqrtf(dist2);
                    float nx = rx / (dist + 1e-6f);
                    float ny = ry / (dist + 1e-6f);
                    float vdotn = particles[i].vx * nx + particles[i].vy * ny;
                    /* reflect velocity */
                    particles[i].vx -= 2.0f * vdotn * nx;
                    particles[i].vy -= 2.0f * vdotn * ny;
                    /* push outside slightly to avoid stuck */
                    float pen = (c.r - dist) + 0.5f;
                    particles[i].x += nx * pen;
                    particles[i].y += ny * pen;
                }
            }

            particles[i].life -= dt;
        }
    }
}

void ParticleSystem::render(SDL_Renderer *renderer, int psize) {
    SDL_Rect r;
    r.w = psize;
    r.h = psize;
    SDL_SetRenderDrawColor(renderer, 255, 180, 60, 255);
    for (int i = 0; i < maxParticles; ++i) {
        if (particles[i].life > 0.0f) {
            int drawSize = (particleSize > 0) ? particleSize : psize;
            r.w = drawSize;
            r.h = drawSize;
            r.x = (int)(particles[i].x - drawSize * 0.5f);
            r.y = (int)(particles[i].y - drawSize * 0.5f);
            SDL_RenderFillRect(renderer, &r);
        }
    }

    /* draw collision circles */
    SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);
    for (const Circle &c : circles) {
        drawFilledCircle(renderer, (int)c.x, (int)c.y, (int)floorf(c.r));
        /* outline */
        SDL_SetRenderDrawColor(renderer, 60, 120, 200, 255);
        for (int d = 0; d < 2; ++d) drawFilledCircle(renderer, (int)c.x, (int)c.y, (int)floorf(c.r) - d);
        SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);
    }
}



int ParticleSystem::activeCount() const {
    int c = 0;
    for (int i = 0; i < maxParticles; ++i) if (particles[i].life > 0.0f) ++c;
    return c;
}
