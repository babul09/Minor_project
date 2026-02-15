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

/* --- small 2D Perlin noise (improved) --- */
static inline float fadef(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float grad2(int hash, float x, float y) {
    /* 8 directional gradients */
    switch (hash & 7) {
        case 0: return  x + y;
        case 1: return -x + y;
        case 2: return  x - y;
        case 3: return -x - y;
        case 4: return  x;
        case 5: return -x;
        case 6: return  y;
        default: return -y;
    }
}
static float perlin2(float x, float y) {
    static const int p[256] = {151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244, 102,143,54,65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,119,248,152, 2,44,154,163,70,221,153,101,155,167, 43,172,9,129,22,39,253, 19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180};
    static int perm[512];
    static bool init = false;
    if (!init) { for (int i = 0; i < 256; ++i) perm[i] = perm[i + 256] = p[i]; init = true; }

    int X = (int)floorf(x) & 255;
    int Y = (int)floorf(y) & 255;
    float xf = x - floorf(x);
    float yf = y - floorf(y);
    float u = fadef(xf);
    float v = fadef(yf);

    int aa = perm[perm[X] + Y];
    int ab = perm[perm[X] + Y + 1];
    int ba = perm[perm[X + 1] + Y];
    int bb = perm[perm[X + 1] + Y + 1];

    float x1 = lerpf(grad2(aa, xf, yf), grad2(ba, xf - 1.0f, yf), u);
    float x2 = lerpf(grad2(ab, xf, yf - 1.0f), grad2(bb, xf - 1.0f, yf - 1.0f), u);
    return lerpf(x1, x2, v); /* range approximately -1..1 */
}

ParticleSystem::ParticleSystem(int maxParticles_)
    : maxParticles(maxParticles_), particles(maxParticles_), spawnRate(300.0f), spawnAcc(0.0f), particleLife(2.0f),
      windBaseX(0.0f), windBaseY(300.0f), windMode(WIND_UNIFORM), windStrength(200.0f),
      windCenterX(0.0f), windCenterY(0.0f), noiseAmplitude(50.0f), noiseScale(0.01f), noiseSpeed(1.0f), windPhase(0.0f),
      emitterSpeedMin(50.0f), emitterSpeedMax(250.0f), emitterSpread(6.283185307179586f), emitterDirection(0.0f), emitterRadius(6.0f),
      particleSize(2) {
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

    float ang;
    if (emitterSpread >= 6.283185307179586f) ang = randf() * 6.283185307179586f;
    else ang = emitterDirection + (randf() - 0.5f) * emitterSpread;

    float speed = emitterSpeedMin + randf() * (emitterSpeedMax - emitterSpeedMin);
    particles[idx].x = emit_x + (randf() - 0.5f) * emitterRadius * 2.0f;
    particles[idx].y = emit_y + (randf() - 0.5f) * emitterRadius * 2.0f;
    particles[idx].vx = cosf(ang) * speed;
    particles[idx].vy = sinf(ang) * speed;
    particles[idx].life = particleLife * (0.8f + randf() * 0.4f);
}

void ParticleSystem::getWindAt(float x, float y, float &wx, float &wy) const {
    switch (windMode) {
        case WIND_VORTEX: {
            float rx = x - windCenterX;
            float ry = y - windCenterY;
            float dist = sqrtf(rx * rx + ry * ry) + 1e-6f;
            float mag = windStrength / dist; /* stronger near center */
            wx = -ry / dist * mag;
            wy = rx / dist * mag;
            break;
        }
        case WIND_NOISE: {
            /* use Perlin noise for a smoothly varying vector field */
            float nx = x * noiseScale;
            float ny = y * noiseScale;
            float v1 = perlin2(nx + windPhase * 0.7f, ny);
            float v2 = perlin2(nx + 100.0f, ny + windPhase * 0.7f);
            wx = v1 * noiseAmplitude;
            wy = v2 * noiseAmplitude;
            break;
        }
        case WIND_UNIFORM:
        default:
            wx = windBaseX;
            wy = windBaseY;
            break;
    }
}

void ParticleSystem::update(float dt, float emit_x, float emit_y) {
    if (emitting && spawnRate > 0.0f) {
        float to_spawn = spawnRate * dt + spawnAcc;
        int n = (int)to_spawn;
        spawnAcc = to_spawn - n;
        for (int i = 0; i < n; ++i) spawnOne(emit_x, emit_y);
    }

    /* advance wind/noise phase */
    windPhase += noiseSpeed * dt;

    for (int i = 0; i < maxParticles; ++i) {
        if (particles[i].life > 0.0f) {
            float wx, wy; getWindAt(particles[i].x, particles[i].y, wx, wy);
            particles[i].vx += wx * dt;
            particles[i].vy += wy * dt;
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

void ParticleSystem::render(SDL_Renderer *renderer, int psize, float scale, float centerX, float centerY) {
    SDL_Rect r;
    r.w = psize;
    r.h = psize;
    SDL_SetRenderDrawColor(renderer, 255, 180, 60, 255);
    for (int i = 0; i < maxParticles; ++i) {
        if (particles[i].life > 0.0f) {
            int baseSize = (particleSize > 0) ? particleSize : psize;
            int drawSize = std::max(1, (int)floorf(baseSize * scale + 0.5f));
            r.w = drawSize;
            r.h = drawSize;
            float sx = (particles[i].x - centerX) * scale + centerX;
            float sy = (particles[i].y - centerY) * scale + centerY;
            r.x = (int)(sx - drawSize * 0.5f);
            r.y = (int)(sy - drawSize * 0.5f);
            SDL_RenderFillRect(renderer, &r);
        }
    }

    /* draw collision circles */
    SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);
    for (const Circle &c : circles) {
        int cx = (int)floorf((c.x - centerX) * scale + centerX + 0.5f);
        int cy = (int)floorf((c.y - centerY) * scale + centerY + 0.5f);
        int cr = std::max(1, (int)floorf(c.r * scale + 0.5f));
        drawFilledCircle(renderer, cx, cy, cr);
        /* outline */
        SDL_SetRenderDrawColor(renderer, 60, 120, 200, 255);
        for (int d = 0; d < 2; ++d) drawFilledCircle(renderer, cx, cy, std::max(0, cr - d));
        SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);
    }
}



int ParticleSystem::activeCount() const {
    int c = 0;
    for (int i = 0; i < maxParticles; ++i) if (particles[i].life > 0.0f) ++c;
    return c;
}

void ParticleSystem::accumulateGrid(const GridRect &gridRect, int rows, int cols, std::vector<int> &outCounts) const {
    if (rows <= 0 || cols <= 0) return;
    size_t total = (size_t)rows * (size_t)cols;
    if (outCounts.size() != total) outCounts.assign(total, 0);
    else std::fill(outCounts.begin(), outCounts.end(), 0);

    for (int i = 0; i < maxParticles; ++i) {
        if (particles[i].life <= 0.0f) continue;
        float px = particles[i].x;
        float py = particles[i].y;
        if (px < gridRect.x || py < gridRect.y || px >= gridRect.x + gridRect.w || py >= gridRect.y + gridRect.h) continue;
        int col = (int)((px - gridRect.x) / (float)gridRect.cellSize);
        int row = (int)((py - gridRect.y) / (float)gridRect.cellSize);
        if (col < 0 || col >= cols || row < 0 || row >= rows) continue;
        outCounts[row * cols + col] += 1;
    }
}
