#pragma once
#include "Vector3f.h"
#include <vector>

// ============ WIND PARTICLE SYSTEM ============
struct WindParticle {
    Vector3f position;
    float length;
    float speed;
    float life;
    bool active;
};

class WindSystem {
public:
    WindSystem();
    ~WindSystem();
    
    void init(int maxParticles = 50);
    void update(float deltaTime, const Vector3f& cameraPos, const Vector3f& forward, float speed);
    void render(const Vector3f& forward, float speed);
    void reset();
    
private:
    std::vector<WindParticle> particles;
    float minSpeedThreshold;  // Speed below which no wind is shown
};

// ============ EXPLOSION PARTICLE SYSTEM ============
struct ExplosionParticle {
    Vector3f position;
    Vector3f velocity;
    float size;           // Current size (grows over time)
    float startSize;      // Initial size
    float maxSize;        // Maximum size
    float alpha;          // Transparency
    float life;           // Remaining lifetime
    float maxLife;        // Total lifetime
    float rotation;       // Billboard rotation
    float rotationSpeed;  // Rotation speed
    bool active;
};

class ExplosionSystem {
public:
    ExplosionSystem();
    ~ExplosionSystem();
    
    void init();
    
    // Start an explosion at the given position
    void startExplosion(const Vector3f& position);
    
    // Check if explosion is finished
    bool isActive() const { return explosionActive || hasActiveParticles(); }
    
    // Reset the system
    void reset();
    
    // Update all particles
    void update(float deltaTime);
    
    // Render all particles (billboard quads facing camera)
    void render(const Vector3f& cameraPosition);
    
private:
    std::vector<ExplosionParticle> particles;
    unsigned int textureID;
    
    Vector3f explosionCenter;   // Center of explosion
    bool explosionActive;       // Is explosion in progress?
    float explosionTimer;       // Time since explosion started
    float spawnTimer;           // Timer for spawning particles
    float spawnDuration;        // How long to spawn particles
    
    void spawnParticle();
    bool hasActiveParticles() const;
    float randomFloat(float min, float max);
    
    // Load explosion texture
    bool loadExplosionTexture(const char* filename);
};

// ============ JET TRAIL PARTICLE SYSTEM ============
struct JetTrailParticle {
    Vector3f position;
    float size;
    float alpha;
    float life;
    float maxLife;
    bool active;
};

class JetTrailSystem {
public:
    JetTrailSystem();
    ~JetTrailSystem();

    void init(int maxParticles = 200);
    void update(float deltaTime);
    void render();
    void reset();

    // Spawn a trail particle at the given position
    void diffEmit(const Vector3f& position, const Vector3f& velocity);

private:
    std::vector<JetTrailParticle> particles;
    unsigned int textureID;
    float spawnTimer;
    float spawnInterval;
    
    // Helper to load texture
    bool loadTrailTexture(const char* filename);
};


// ============ COMBINED PARTICLE EFFECTS MANAGER ============
class ParticleEffects {
public:
    ParticleEffects();
    ~ParticleEffects();
    
    // Initialize all particle systems
    void init();
    
    // Update all systems
    void update(float deltaTime, const Vector3f& cameraPos, const Vector3f& forward, float speed);
    
    // Render wind particles
    void renderWind(const Vector3f& forward, float speed);
    
    // Render explosion particles
    void renderExplosion(const Vector3f& cameraPosition);
    
    // Start an explosion effect
    void triggerExplosion(const Vector3f& position);
    
    // Check if explosion is still playing
    bool isExplosionActive() const;
    
    // Reset all effects
    void reset();
    
    // Individual systems (for direct access if needed)
    WindSystem wind;
    ExplosionSystem explosion;
    JetTrailSystem jetTrail;

    // Render jet trail
    void renderJetTrail();

    // Emit jet trail from a plane position
    void emitJetTrail(const Vector3f& position, const Vector3f& velocity);
};
