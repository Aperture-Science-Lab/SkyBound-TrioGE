#include "CrashSystem.h"

CrashSystem::CrashSystem() 
    : crashed(false), soundPlayed(false) {
}

CrashSystem::~CrashSystem() {
}

void CrashSystem::init() {
    explosion.init();
    smoke.init();
    crashed = false;
    soundPlayed = false;
}

void CrashSystem::triggerCrash(const Vector3f& position) {
    if (crashed) return;  // Only one crash at a time
    
    crashPosition = position;
    crashed = true;
    
    // Start both particle effects at crash position
    explosion.startExplosion(position);
    smoke.startSmoke(position);
    
    // Play crash sound once
    if (!soundPlayed) {
        playCrashSound();
        soundPlayed = true;
    }
}

void CrashSystem::update(float deltaTime) {
    if (!crashed) return;
    
    // Update both particle systems
    explosion.update(deltaTime);
    smoke.update(deltaTime);
}

void CrashSystem::render(const Vector3f& cameraPosition) {
    if (!crashed) return;
    
    // Render explosion particles
    explosion.render(cameraPosition);
    
    // Render smoke particles
    smoke.render(cameraPosition);
}

bool CrashSystem::isActive() const {
    if (!crashed) return false;
    
    // Active if either particle system still has active particles
    return explosion.isActive() || smoke.isActive();
}

void CrashSystem::reset() {
    crashed = false;
    soundPlayed = false;
    explosion.reset();
    smoke.reset();
}

void CrashSystem::playCrashSound() {
    // Try multiple paths for the crash sound
    // PlaySound with SND_ASYNC so it doesn't block
    if (!PlaySoundA("../sound/plane-crash.wav", NULL, SND_FILENAME | SND_ASYNC)) {
        if (!PlaySoundA("sound/plane-crash.wav", NULL, SND_FILENAME | SND_ASYNC)) {
            // Sound file not found, continue silently
        }
    }
}
