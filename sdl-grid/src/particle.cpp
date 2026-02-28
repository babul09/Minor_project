#include "particle.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace {
constexpr float EPSILON = 1e-6f;
constexpr float TWO_PI = 6.283185307179586f;
constexpr float PENETRATION_OFFSET = 0.5f;

// Random number generator
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<float> dist(0.0f, 1.0f);

inline float randf() noexcept { return dist(gen); }

// Helper: draw a filled circle using horizontal scanlines
void drawFilledCircle(SDL_Renderer *renderer, int cx, int cy, int r) {
  for (int dy = -r; dy <= r; ++dy) {
    const int dx = static_cast<int>(
        std::floor(std::sqrt(static_cast<float>(r * r - dy * dy))));
    SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
  }
}

// 2D Perlin noise implementation helpers
constexpr float fadef(float t) noexcept {
  return t * t * t * (t * (t * 6 - 15) + 10);
}

constexpr float lerpf(float a, float b, float t) noexcept {
  return a + (b - a) * t;
}

inline float grad2(int hash, float x, float y) noexcept {
  // 8 directional gradients
  switch (hash & 7) {
  case 0:
    return x + y;
  case 1:
    return -x + y;
  case 2:
    return x - y;
  case 3:
    return -x - y;
  case 4:
    return x;
  case 5:
    return -x;
  case 6:
    return y;
  default:
    return -y;
  }
}

float perlin2(float x, float y) {
  static const int p[256] = {
      151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,
      225, 140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190,
      6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117,
      35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136,
      171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158,
      231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,
      245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209,
      76,  132, 187, 208, 89,  18,  169, 200, 196, 135, 130, 116, 188, 159, 86,
      164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124, 123, 5,
      202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,
      58,  17,  182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,
      154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,   129, 22,  39,  253,
      19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,
      228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,
      145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184,
      84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
      222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156,
      180};
  static int perm[512];
  static bool init = false;
  if (!init) {
    for (int i = 0; i < 256; ++i)
      perm[i] = perm[i + 256] = p[i];
    init = true;
  }

  const int X = static_cast<int>(std::floor(x)) & 255;
  const int Y = static_cast<int>(std::floor(y)) & 255;
  const float xf = x - std::floor(x);
  const float yf = y - std::floor(y);
  const float u = fadef(xf);
  const float v = fadef(yf);

  const int aa = perm[perm[X] + Y];
  const int ab = perm[perm[X] + Y + 1];
  const int ba = perm[perm[X + 1] + Y];
  const int bb = perm[perm[X + 1] + Y + 1];

  const float x1 = lerpf(grad2(aa, xf, yf), grad2(ba, xf - 1.0f, yf), u);
  const float x2 =
      lerpf(grad2(ab, xf, yf - 1.0f), grad2(bb, xf - 1.0f, yf - 1.0f), u);
  return lerpf(x1, x2, v); // Range approximately -1..1
}
} // namespace
ParticleSystem::ParticleSystem(int maxParticles_)
    : maxParticles(maxParticles_), particles(maxParticles_), spawnRate(300.0f),
      spawnAcc(0.0f), particleLife(2.0f), windBaseX(0.0f), windBaseY(300.0f),
      windMode(WIND_UNIFORM), windStrength(200.0f), windCenterX(0.0f),
      windCenterY(0.0f), noiseAmplitude(50.0f), noiseScale(0.01f),
      noiseSpeed(1.0f), windPhase(0.0f), emitterSpeedMin(50.0f),
      emitterSpeedMax(250.0f), emitterSpread(TWO_PI), emitterDirection(0.0f),
      emitterRadius(6.0f), particleSize(2) {
  for (auto &particle : particles) {
    particle.life = 0.0f;
  }
}

void ParticleSystem::clear() noexcept {
  for (auto &particle : particles) {
    particle.life = 0.0f;
  }
}

void ParticleSystem::spawnOne(float emit_x, float emit_y) {
  // Find free slot
  int idx = -1;
  for (int i = 0; i < maxParticles; ++i) {
    if (particles[i].life <= 0.0f) {
      idx = i;
      break;
    }
  }
  if (idx == -1)
    return;

  // Calculate spawn angle
  float ang;
  if (emitterSpread >= TWO_PI) {
    ang = randf() * TWO_PI;
  } else {
    ang = emitterDirection + (randf() - 0.5f) * emitterSpread;
  }

  const float speed =
      emitterSpeedMin + randf() * (emitterSpeedMax - emitterSpeedMin);
  particles[idx].x = emit_x + (randf() - 0.5f) * emitterRadius * 2.0f;
  particles[idx].y = emit_y + (randf() - 0.5f) * emitterRadius * 2.0f;
  particles[idx].vx = std::cos(ang) * speed;
  particles[idx].vy = std::sin(ang) * speed;
  particles[idx].life = particleLife * (0.8f + randf() * 0.4f);
}

void ParticleSystem::getWindAt(float x, float y, float &wx, float &wy) const {
  switch (windMode) {
  case WIND_VORTEX: {
    const float rx = x - windCenterX;
    const float ry = y - windCenterY;
    const float dist = std::sqrt(rx * rx + ry * ry) + EPSILON;
    const float mag = windStrength / dist; // Stronger near center
    wx = -ry / dist * mag;
    wy = rx / dist * mag;
    break;
  }
  case WIND_NOISE: {
    // Use Perlin noise for a smoothly varying vector field
    const float nx = x * noiseScale;
    const float ny = y * noiseScale;
    const float v1 = perlin2(nx + windPhase * 0.7f, ny);
    const float v2 = perlin2(nx + 100.0f, ny + windPhase * 0.7f);
    wx = v1 * noiseAmplitude;
    wy = v2 * noiseAmplitude;
    break;
  }
  case WIND_REALWORLD: {
    // Default fallback if we don't have a 3x3 grid yet
    float baseU = 0.0f;
    float baseV = 0.0f;

    if (realWindUGrid.size() == 9 && rwGridWidth > 0 && rwGridHeight > 0) {
      // Find normalized position (0.0 to 1.0) along screen map
      float normX =
          std::max(0.0f, std::min(1.0f, x / static_cast<float>(rwGridWidth)));
      float normY =
          std::max(0.0f, std::min(1.0f, y / static_cast<float>(rwGridHeight)));

      // Maps 0.0-1.0 to the 2 grid segments (0-1, 1-2)
      float gridX = normX * 2.0f;
      float gridY = normY * 2.0f;

      int x0 = std::min(1, static_cast<int>(gridX));
      int x1 = x0 + 1;
      int y0 = std::min(1, static_cast<int>(gridY));
      int y1 = y0 + 1;

      float tx = gridX - x0;
      float ty = gridY - y0;

      // Lookup u, v in the 3x3 1D array (row-major: y * 3 + x)
      auto getU = [&](int _x, int _y) { return realWindUGrid[_y * 3 + _x]; };
      auto getV = [&](int _x, int _y) { return realWindVGrid[_y * 3 + _x]; };

      // Bilinear interpolation for U
      float uTop = getU(x0, y0) * (1.0f - tx) + getU(x1, y0) * tx;
      float uBottom = getU(x0, y1) * (1.0f - tx) + getU(x1, y1) * tx;
      baseU = uTop * (1.0f - ty) + uBottom * ty;

      // Bilinear interpolation for V
      float vTop = getV(x0, y0) * (1.0f - tx) + getV(x1, y0) * tx;
      float vBottom = getV(x0, y1) * (1.0f - tx) + getV(x1, y1) * tx;
      baseV = vTop * (1.0f - ty) + vBottom * ty;
    } else if (realWindUGrid.size() == 1) { // Single point fallback
      baseU = realWindUGrid[0];
      baseV = realWindVGrid[0];
    }

    // Add turbulence using Perlin noise
    const float nx = x * noiseScale;
    const float ny = y * noiseScale;
    const float v1 = perlin2(nx + windPhase * 0.7f, ny);
    const float v2 = perlin2(nx + 100.0f, ny + windPhase * 0.7f);
    wx = baseU + (v1 * noiseAmplitude);
    wy = baseV + (v2 * noiseAmplitude);
    break;
  }
  case WIND_UNIFORM:
  default:
    wx = windBaseX;
    wy = windBaseY;
    break;
  }
}

void ParticleSystem::update(float dt, float emit_x, float emit_y,
                            const Boundary &boundary,
                            const GridRect &gridRect) {
  // Spawn particles if emitting
  if (emitting && spawnRate > 0.0f) {
    const float to_spawn = spawnRate * dt + spawnAcc;
    const int n = static_cast<int>(to_spawn);
    spawnAcc = to_spawn - n;
    for (int i = 0; i < n; ++i) {
      spawnOne(emit_x, emit_y);
    }
  }

  // Advance wind/noise phase
  windPhase += noiseSpeed * dt;

  // Update all active particles
  for (int i = 0; i < maxParticles; ++i) {
    if (particles[i].life > 0.0f) {
      float wx, wy;
      getWindAt(particles[i].x, particles[i].y, wx, wy);
      particles[i].vx += wx * dt;
      particles[i].vy += wy * dt;

      // remember old position for collision response
      const float oldx = particles[i].x;
      const float oldy = particles[i].y;
      particles[i].x += particles[i].vx * dt;
      particles[i].y += particles[i].vy * dt;

      // boundary containment: keep particle inside selected cells
      if (!boundary.contains(particles[i].x, particles[i].y, gridRect)) {
        int oldR, oldC;
        bool dummy = boundary.cellAt(oldx, oldy, gridRect, oldR, oldC);
        (void)dummy; // suppress nodiscard warning
        int newR, newC;
        if (boundary.cellAt(particles[i].x, particles[i].y, gridRect, newR,
                            newC)) {
          if (newR != oldR)
            particles[i].vy = -particles[i].vy;
          if (newC != oldC)
            particles[i].vx = -particles[i].vx;
        } else {
          // went outside grid entirely
          particles[i].vx = -particles[i].vx;
          particles[i].vy = -particles[i].vy;
        }
        particles[i].x = oldx;
        particles[i].y = oldy;
      }

      // Simple circle collisions: reflect velocity when inside a circle
      for (const auto &c : circles) {
        const float rx = particles[i].x - c.x;
        const float ry = particles[i].y - c.y;
        const float dist2 = rx * rx + ry * ry;
        const float r2 = c.r * c.r;

        if (dist2 < r2 && dist2 > 0.0f) {
          const float dist = std::sqrt(dist2);
          const float nx = rx / (dist + EPSILON);
          const float ny = ry / (dist + EPSILON);
          const float vdotn = particles[i].vx * nx + particles[i].vy * ny;

          // Reflect velocity
          particles[i].vx -= 2.0f * vdotn * nx;
          particles[i].vy -= 2.0f * vdotn * ny;

          // Push outside slightly to avoid getting stuck
          const float pen = (c.r - dist) + PENETRATION_OFFSET;
          particles[i].x += nx * pen;
          particles[i].y += ny * pen;
        }
      }

      particles[i].life -= dt;
    }
  }
}

void ParticleSystem::render(SDL_Renderer *renderer, int psize, float scale,
                            float centerX, float centerY) const {
  SDL_Rect r;
  r.w = psize;
  r.h = psize;
  SDL_SetRenderDrawColor(renderer, 255, 180, 60, 255);

  for (int i = 0; i < maxParticles; ++i) {
    if (particles[i].life > 0.0f) {
      const int baseSize = (particleSize > 0) ? particleSize : psize;
      const int drawSize =
          std::max(1, static_cast<int>(std::floor(baseSize * scale + 0.5f)));
      r.w = drawSize;
      r.h = drawSize;
      const float sx = (particles[i].x - centerX) * scale + centerX;
      const float sy = (particles[i].y - centerY) * scale + centerY;
      r.x = static_cast<int>(sx - drawSize * 0.5f);
      r.y = static_cast<int>(sy - drawSize * 0.5f);
      SDL_RenderFillRect(renderer, &r);
    }
  }

  // Draw collision circles
  SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);
  for (const auto &c : circles) {
    const int cx =
        static_cast<int>(std::floor((c.x - centerX) * scale + centerX + 0.5f));
    const int cy =
        static_cast<int>(std::floor((c.y - centerY) * scale + centerY + 0.5f));
    const int cr =
        std::max(1, static_cast<int>(std::floor(c.r * scale + 0.5f)));
    drawFilledCircle(renderer, cx, cy, cr);

    // Outline
    SDL_SetRenderDrawColor(renderer, 60, 120, 200, 255);
    for (int d = 0; d < 2; ++d) {
      drawFilledCircle(renderer, cx, cy, std::max(0, cr - d));
    }
    SDL_SetRenderDrawColor(renderer, 100, 180, 255, 255);
  }
}

int ParticleSystem::activeCount() const noexcept {
  int count = 0;
  for (const auto &p : particles) {
    if (p.life > 0.0f)
      ++count;
  }
  return count;
}

void ParticleSystem::accumulateGrid(const GridRect &gridRect, int rows,
                                    int cols,
                                    std::vector<int> &outCounts) const {
  if (rows <= 0 || cols <= 0)
    return;
  const size_t total = static_cast<size_t>(rows) * static_cast<size_t>(cols);
  if (outCounts.size() != total) {
    outCounts.assign(total, 0);
  } else {
    std::fill(outCounts.begin(), outCounts.end(), 0);
  }

  for (const auto &p : particles) {
    if (p.life <= 0.0f)
      continue;
    const float px = p.x;
    const float py = p.y;
    if (px < gridRect.x || py < gridRect.y || px >= gridRect.x + gridRect.w ||
        py >= gridRect.y + gridRect.h) {
      continue;
    }
    const int col = static_cast<int>((px - gridRect.x) /
                                     static_cast<float>(gridRect.cellSize));
    const int row = static_cast<int>((py - gridRect.y) /
                                     static_cast<float>(gridRect.cellSize));
    if (col < 0 || col >= cols || row < 0 || row >= rows)
      continue;
    outCounts[row * cols + col] += 1;
  }
}
