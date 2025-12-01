#pragma once
#include "Vector3f.h"
#include <vector>

// Structure for individual smoke particles
struct SmokeParticle {
    Vector3f position;
    Vector3f velocity;
    float size;           // Current size (grows over time)
    float startSize;      // Initial size
    float maxSize;        // Maximum size before fading
    float alpha;          // Transparency (1.0 = opaque, 0.0 = invisible)
    float life;           // Remaining lifetime
    float maxLife;        // Total lifetime
    float rotation;       // Billboard rotation angle
    float rotationSpeed;  // How fast it rotates
    bool active;
};

class SmokeSystem {
public:
    SmokeSystem();
    ~SmokeSystem();
    
    // Initialize the smoke system and load texture
    void init();
    
    // Start emitting smoke at the given position
    void startSmoke(const Vector3f& position);
    
    // Stop emitting new smoke particles
    void stopSmoke();
    
    // Reset the entire system (clears all particles)
    void reset();
    
    // Update all particles (call every frame)
    void update(float deltaTime);
    
    // Render all particles (billboard quads facing camera)
    void render(const Vector3f& cameraPosition);
    
    // Check if smoke is currently active
    bool isActive() const { return smokeActive || hasActiveParticles(); }
    
private:
    std::vector<SmokeParticle> particles;
    unsigned int textureID;
    
    Vector3f emitPosition;      // Where smoke is emitting from
    bool smokeActive;           // Is the emitter active?
    float spawnTimer;           // Timer for spawning new particles
    float spawnRate;            // Time between particle spawns
    float emitDuration;         // How long to emit particles
    float emitTimer;            // Current emit time
    
    // Spawn a new smoke particle
    void spawnParticle();
    
    // Check if there are any active particles
    bool hasActiveParticles() const;
    
    // Random float helper
    float randomFloat(float min, float max);
};
