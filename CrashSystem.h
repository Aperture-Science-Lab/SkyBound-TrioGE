#pragma once
#include "Vector3f.h"
#include "SmokeSystem.h"
#include "ParticleEffects.h"
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

// Unified Crash System - handles explosion + smoke + sound
// Designed to be shared across all levels
class CrashSystem {
public:
    CrashSystem();
    ~CrashSystem();
    
    // Initialize the crash system (call once at startup)
    void init();
    
    // Trigger a crash at the given position
    // This will:
    // 1. Start explosion particles
    // 2. Start smoke particles
    // 3. Play crash sound (once)
    void triggerCrash(const Vector3f& position);
    
    // Update all particle effects (call every frame)
    void update(float deltaTime);
    
    // Render all particles (call after scene rendering)
    void render(const Vector3f& cameraPosition);
    
    // Check if crash effects are still active
    bool isActive() const;
    
    // Reset the system (clears all particles, allows new crash)
    void reset();
    
    // Get crash position
    const Vector3f& getCrashPosition() const { return crashPosition; }
    
    // Check if crashed
    bool hasCrashed() const { return crashed; }
    
private:
    ExplosionSystem explosion;
    SmokeSystem smoke;
    
    Vector3f crashPosition;
    bool crashed;
    bool soundPlayed;
    
    // Play crash sound
    void playCrashSound();
};
