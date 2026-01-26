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
    void render(SDL_Renderer *renderer, int psize);
    int activeCount() const;

    bool emitting = true;

private:
    int maxParticles;
    std::vector<Particle> particles;
    float spawnRate; /* per second */
    float spawnAcc;
    float particleLife;
    float gravityX, gravityY;
    int particleSize; /* pixels */
    std::vector<Circle> circles;

    void spawnOne(float emit_x, float emit_y);
};

#endif // PARTICLE_H
