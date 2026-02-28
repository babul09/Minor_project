#ifndef PARTICLE_H
#define PARTICLE_H

#include "boundary.h" // needed for update() signature
#include "grid.h"     // For GridRect used by accumulateGrid
#include <SDL2/SDL.h>
#include <vector>

// Particle structure representing a single particle in the system
struct Particle {
  float x, y;   // Position
  float vx, vy; // Velocity
  float life;   // Remaining lifetime in seconds
};

// Particle system with advanced wind fields and collision detection
class ParticleSystem {
public:
  explicit ParticleSystem(int maxParticles = 2000);

  // Spawn rate control
  void setSpawnRate(float r) noexcept { spawnRate = r; }
  [[nodiscard]] float getSpawnRate() const noexcept { return spawnRate; }

  void setParticleLife(float l) noexcept { particleLife = l; }
  [[nodiscard]] float getParticleLife() const noexcept { return particleLife; }

  // Wind field modes
  enum WindMode {
    WIND_UNIFORM = 0,
    WIND_VORTEX = 1,
    WIND_NOISE = 2,
    WIND_REALWORLD = 3
  };

  // Wind control (px/s^2)
  void setWindBase(float wx, float wy) noexcept {
    windBaseX = wx;
    windBaseY = wy;
  }
  void getWindBase(float &wx, float &wy) const noexcept {
    wx = windBaseX;
    wy = windBaseY;
  }

  // Real-world 3x3 wind control
  void setRealWindGrid(const std::vector<float> &uMap,
                       const std::vector<float> &vMap, int gridWidth,
                       int gridHeight) noexcept {
    realWindUGrid = uMap;
    realWindVGrid = vMap;
    rwGridWidth = gridWidth;
    rwGridHeight = gridHeight;
  }
  void getRealWind(std::vector<float> &uMap,
                   std::vector<float> &vMap) const noexcept {
    uMap = realWindUGrid;
    vMap = realWindVGrid;
  }

  void setWindMode(int mode) noexcept { windMode = mode; }
  [[nodiscard]] int getWindMode() const noexcept { return windMode; }

  void setWindStrength(float s) noexcept { windStrength = s; }
  [[nodiscard]] float getWindStrength() const noexcept { return windStrength; }

  void setWindCenter(float x, float y) noexcept {
    windCenterX = x;
    windCenterY = y;
  }
  void getWindCenter(float &x, float &y) const noexcept {
    x = windCenterX;
    y = windCenterY;
  }

  void setNoiseParams(float amplitude, float scale, float speed) noexcept {
    noiseAmplitude = amplitude;
    noiseScale = scale;
    noiseSpeed = speed;
  }
  void getNoiseParams(float &amplitude, float &scale,
                      float &speed) const noexcept {
    amplitude = noiseAmplitude;
    scale = noiseScale;
    speed = noiseSpeed;
  }

  // Get wind vector (acceleration) at world position
  void getWindAt(float x, float y, float &wx, float &wy) const;

  // Emitter settings
  void setEmitterSpeedRange(float min, float max) noexcept {
    emitterSpeedMin = min;
    emitterSpeedMax = max;
  }
  void getEmitterSpeedRange(float &min, float &max) const noexcept {
    min = emitterSpeedMin;
    max = emitterSpeedMax;
  }

  void setEmitterSpread(float s) noexcept { emitterSpread = s; }
  [[nodiscard]] float getEmitterSpread() const noexcept {
    return emitterSpread;
  }

  void setEmitterDirection(float d) noexcept { emitterDirection = d; }
  [[nodiscard]] float getEmitterDirection() const noexcept {
    return emitterDirection;
  }

  void setEmitterRadius(float r) noexcept { emitterRadius = r; }
  [[nodiscard]] float getEmitterRadius() const noexcept {
    return emitterRadius;
  }

  // Particle rendering size in pixels
  void setParticleSize(int s) noexcept { particleSize = s; }
  [[nodiscard]] int getParticleSize() const noexcept { return particleSize; }

  // Collision circles management
  struct Circle {
    float x, y, r;
  };

  void addCircle(float x, float y, float r) { circles.push_back({x, y, r}); }
  void clearCircles() noexcept { circles.clear(); }
  [[nodiscard]] int circleCount() const noexcept {
    return static_cast<int>(circles.size());
  }

  // Core simulation methods
  void clear() noexcept;
  // dt: time step, emitters: list of active emitter world positions
  // boundary: containment region, gridRect used to map positions to cells
  void update(float dt, const std::vector<std::pair<float, float>> &emitters,
              const Boundary &boundary, const GridRect &gridRect);

  // Instantly spawns particles across the entire grid mapping for visualizing
  // paths
  void populateRandomly(const Boundary &boundary, const GridRect &gridRect);

  // Render with view scale and center for proper zoom support
  void render(SDL_Renderer *renderer, int psize, float scale = 1.0f,
              float centerX = 0.0f, float centerY = 0.0f) const;

  [[nodiscard]] int activeCount() const noexcept;

  // Accumulate active particle counts into a grid-shaped histogram (row-major)
  void accumulateGrid(const GridRect &gridRect, int rows, int cols,
                      std::vector<int> &outCounts) const;

  bool emitting = true;

private:
  int maxParticles;
  std::vector<Particle> particles;
  float spawnRate;    // Per second
  float spawnAcc;     // Accumulator for fractional spawns
  float particleLife; // Default lifetime

  // Wind field internal state
  float windBaseX, windBaseY;                   // Base/uniform wind (px/s^2)
  int windMode;                                 // Current WindMode
  float windStrength;                           // Vortex/general strength
  float windCenterX, windCenterY;               // For vortex mode
  float noiseAmplitude, noiseScale, noiseSpeed; // Noise mode params
  float windPhase;                              // Time accumulator for noise

  // 3x3 Wind Field Grid
  std::vector<float> realWindUGrid;
  std::vector<float> realWindVGrid;
  int rwGridWidth = 0;
  int rwGridHeight = 0;

  // Emitter parameters
  float emitterSpeedMin, emitterSpeedMax;
  float emitterSpread;    // Radians, full circle = 2*pi
  float emitterDirection; // Radians (center of spread)
  float emitterRadius;    // Pixel jitter around emitter

  int particleSize; // Rendering size in pixels
  std::vector<Circle> circles;

  void spawnOne(float emit_x, float emit_y);
};

#endif // PARTICLE_H
