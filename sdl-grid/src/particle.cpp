#include "particle.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <time.h>
#include <stdio.h>

static float randf() { return (float)rand() / (float)RAND_MAX; }

ParticleSystem::ParticleSystem(int maxParticles_)
    : maxParticles(maxParticles_), particles(maxParticles_), spawnRate(300.0f), spawnAcc(0.0f), particleLife(2.0f), gravityX(0.0f), gravityY(300.0f) {
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
            r.x = (int)(particles[i].x - psize * 0.5f);
            r.y = (int)(particles[i].y - psize * 0.5f);
            SDL_RenderFillRect(renderer, &r);
        }
    }
}

int ParticleSystem::activeCount() const {
    int c = 0;
    for (int i = 0; i < maxParticles; ++i) if (particles[i].life > 0.0f) ++c;
    return c;
}
