#pragma once
#include "Vector3f.h"
#include <vector>
#include <string>
#include <windows.h>
#include <mmsystem.h>

// Bullet/Projectile structure
struct Bullet {
    Vector3f position;
    Vector3f velocity;
    Vector3f direction;
    float speed;
    float life;
    float maxLife;
    bool active;
};

// Ground explosion effect
struct GroundExplosion {
    Vector3f position;
    float timer;
    float maxTime;
    float size;
    float alpha;
    int frame;        // Animation frame
    bool active;
};

// Shooting System - Handles plane weapons
// Designed to be shared across all levels
class ShootingSystem {
public:
    ShootingSystem();
    ~ShootingSystem();
    
    // Initialize the system
    void init();
    
    // Fire a bullet from the plane
    // position: plane position
    // forward: plane forward direction
    void fire(const Vector3f& position, const Vector3f& forward);
    
    // Update all bullets and explosions
    // Returns true if any bullet hit the ground
    void update(float deltaTime);
    
    // Render bullets (tracer rounds)
    void renderBullets();
    
    // Render ground explosions
    void renderExplosions(const Vector3f& cameraPosition);
    
    // Reset all bullets and explosions
    void reset();
    
    // Check if can fire (cooldown)
    bool canFire() const { return fireCooldown <= 0.0f; }
    
    // Get active bullet count
    int getActiveBulletCount() const;
    
    // Get active explosion count  
    int getActiveExplosionCount() const;
    
private:
    std::vector<Bullet> bullets;
    std::vector<GroundExplosion> explosions;
    
    unsigned int explosionTexture;  // Explosion sprite texture
    float fireCooldown;             // Time until next shot allowed
    float fireRate;                 // Shots per second
    
    std::string basePath;           // Path to sound folder
    
    // Spawn a new bullet
    void spawnBullet(const Vector3f& position, const Vector3f& direction);
    
    // Spawn explosion at ground impact
    void spawnExplosion(const Vector3f& position);
    
    // Play shooting sound
    void playShootSound();
    
    // Play explosion sound
    void playExplosionSound();
    
    // Load explosion texture
    bool loadExplosionTexture(const char* filename);
    
    // Get full path to file
    std::string getFullPath(const char* filename);
    
    // Check if bullet hit ground
    bool checkGroundCollision(Bullet& bullet);
};
