#ifndef PARTICLE_H
#define PARTICLE_H

#include <SDL2/SDL.h>
#include <vector>
#include "grid.h"  /* for GridRect used by accumulateGrid */

struct Particle {
    float x, y;
    float vx, vy;
    float life; /* seconds */
};

class ParticleSystem {
public:
    ParticleSystem(int maxParticles = 2000);

    void setSpawnRate(float r) { spawnRate = r; }
    float getSpawnRate() const { return spawnRate; }

    void setParticleLife(float l) { particleLife = l; }
    float getParticleLife() const { return particleLife; }

    /* Wind field API (replaces gravity) */
    enum WindMode { WIND_UNIFORM = 0, WIND_VORTEX = 1, WIND_NOISE = 2 };

    /* uniform/base wind (px/s^2) */
    void setWindBase(float wx, float wy) { windBaseX = wx; windBaseY = wy; }
    void getWindBase(float &wx, float &wy) const { wx = windBaseX; wy = windBaseY; }

    void setWindMode(int mode) { windMode = mode; }
    int getWindMode() const { return windMode; }

    void setWindStrength(float s) { windStrength = s; }
    float getWindStrength() const { return windStrength; }

    void setWindCenter(float x, float y) { windCenterX = x; windCenterY = y; }
    void getWindCenter(float &x, float &y) const { x = windCenterX; y = windCenterY; }

    void setNoiseParams(float amplitude, float scale, float speed) { noiseAmplitude = amplitude; noiseScale = scale; noiseSpeed = speed; }
    void getNoiseParams(float &amplitude, float &scale, float &speed) const { amplitude = noiseAmplitude; scale = noiseScale; speed = noiseSpeed; }

    /* get wind vector (acceleration) at world position */
    void getWindAt(float x, float y, float &wx, float &wy) const;

    /* emitter settings (click emitter to open) */
    void setEmitterSpeedRange(float min, float max) { emitterSpeedMin = min; emitterSpeedMax = max; }
    void getEmitterSpeedRange(float &min, float &max) const { min = emitterSpeedMin; max = emitterSpeedMax; }
    void setEmitterSpread(float s) { emitterSpread = s; }
    float getEmitterSpread() const { return emitterSpread; }
    void setEmitterDirection(float d) { emitterDirection = d; }
    float getEmitterDirection() const { return emitterDirection; }
    void setEmitterRadius(float r) { emitterRadius = r; }
    float getEmitterRadius() const { return emitterRadius; }

    /* particle draw size in pixels */
    void setParticleSize(int s) { particleSize = s; }
    int getParticleSize() const { return particleSize; }

    /* collision circles */
    struct Circle { float x, y, r; };
    void addCircle(float x, float y, float r) { circles.push_back({x,y,r}); }
    void clearCircles() { circles.clear(); }
    int circleCount() const { return (int)circles.size(); }

    void clear();
    void update(float dt, float emit_x, float emit_y);
    /* render takes a view scale and center so drawings stay correct when zoomed */
    void render(SDL_Renderer *renderer, int psize, float scale = 1.0f, float centerX = 0.0f, float centerY = 0.0f);
    int activeCount() const;

    /* accumulate active particle counts into a grid-shaped histogram (row-major)
       outCounts will be resized to rows*cols if necessary */
    void accumulateGrid(const GridRect &gridRect, int rows, int cols, std::vector<int> &outCounts) const;

    bool emitting = true;

private:
    int maxParticles;
    std::vector<Particle> particles;
    float spawnRate; /* per second */
    float spawnAcc;
    float particleLife;

    /* wind field internal state */
    float windBaseX, windBaseY; /* base/uniform wind (px/s^2) */
    int windMode; /* WindMode */
    float windStrength; /* vortex / general strength */
    float windCenterX, windCenterY; /* for vortex mode */
    float noiseAmplitude, noiseScale, noiseSpeed; /* noise mode params */
    float windPhase; /* time accumulator for noise */

    /* emitter parameters */
    float emitterSpeedMin, emitterSpeedMax;
    float emitterSpread; /* radians, full circle = 2*pi */
    float emitterDirection; /* radians (center of spread) */
    float emitterRadius; /* px jitter around emitter */

    int particleSize; /* pixels */
    std::vector<Circle> circles;

    void spawnOne(float emit_x, float emit_y);
};

#endif // PARTICLE_H
