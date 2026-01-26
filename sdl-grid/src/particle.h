#ifndef PARTICLE_H
#define PARTICLE_H

#include <SDL2/SDL.h>
#include <vector>

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

    void setGravity(float gx, float gy) { gravityX = gx; gravityY = gy; }
    void getGravity(float &gx, float &gy) const { gx = gravityX; gy = gravityY; }

    void clear();
    void update(float dt, float emit_x, float emit_y);
    void render(SDL_Renderer *renderer, int psize);
    int activeCount() const;

    bool emitting = true;

private:
    std::vector<Particle> particles;
    int maxParticles;
    float spawnRate; /* per second */
    float spawnAcc;
    float particleLife;
    float gravityX, gravityY;

    void spawnOne(float emit_x, float emit_y);
};

#endif // PARTICLE_H
