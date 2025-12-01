#include "SoundSystem.h"
#include <stdio.h>
#include <direct.h>

SoundSystem::SoundSystem() 
    : currentSound(PlaneSound::NONE), previousSound(PlaneSound::NONE), 
      initialized(false), crashed(false), landed(false), basePath("") {
}

SoundSystem::~SoundSystem() {
    stopAll();
}

std::string SoundSystem::getFullPath(const char* filename) {
    // If basePath is set, use it
    if (!basePath.empty()) {
        return basePath + "\\" + filename;
    }
    
    // Otherwise return relative path
    return std::string(filename);
}

void SoundSystem::playLoopingSound(const char* filename) {
    std::string fullPath = getFullPath(filename);
    PlaySoundA(fullPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP | SND_NODEFAULT);
}

void SoundSystem::init() {
    if (initialized) return;
    
    // Try to determine absolute path to sound folder
    char cwd[MAX_PATH];
    if (_getcwd(cwd, MAX_PATH)) {
        std::string cwdStr(cwd);
        
        // Check if sound folder exists in current directory
        std::string testPath1 = cwdStr + "\\sound";
        std::string testPath2 = cwdStr + "\\..\\sound";
        
        // Test which path works by checking if a known file exists
        std::string testFile1 = testPath1 + "\\coin.wav";
        std::string testFile2 = testPath2 + "\\coin.wav";
        
        FILE* f = nullptr;
        if (fopen_s(&f, testFile1.c_str(), "rb") == 0 && f) {
            fclose(f);
            basePath = testPath1;
        } else if (fopen_s(&f, testFile2.c_str(), "rb") == 0 && f) {
            fclose(f);
            basePath = testPath2;
        } else {
            // Fallback: try hardcoded project path
            basePath = "C:\\Users\\Victus\\Documents\\Sky-bound\\sound";
        }
    } else {
        // Fallback to hardcoded path
        basePath = "C:\\Users\\Victus\\Documents\\Sky-bound\\sound";
    }
    
    initialized = true;
    crashed = false;
    landed = false;
    currentSound = PlaneSound::NONE;
    previousSound = PlaneSound::NONE;
}

bool SoundSystem::update(bool isGrounded, float speed) {
    if (!initialized || crashed || landed) return false;
    
    // Store previous sound for transition detection
    previousSound = currentSound;
    
    // Determine which sound should be playing
    PlaneSound targetSound;
    
    if (isGrounded && speed < 30.0f) {
        // On ground and slow/stopped = idle sound
        targetSound = PlaneSound::IDLE;
    } else {
        // Flying or moving fast on ground = flying sound
        targetSound = PlaneSound::FLYING;
    }
    
    // Detect touchdown (flying -> idle transition)
    bool touchdown = (previousSound == PlaneSound::FLYING && targetSound == PlaneSound::IDLE);
    
    // Only change if different from current
    if (targetSound != currentSound) {
        currentSound = targetSound;
        
        if (targetSound == PlaneSound::IDLE) {
            playLoopingSound("propeller-plane-idle-01.wav");
        } else if (targetSound == PlaneSound::FLYING) {
            playLoopingSound("propeller-plane-flying-steady-01.wav");
        }
    }
    
    return touchdown;
}

void SoundSystem::playCrashSound() {
    crashed = true;
    currentSound = PlaneSound::NONE;
    
    // Stop looping sounds and play crash
    std::string fullPath = getFullPath("plane-crash.wav");
    PlaySoundA(fullPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void SoundSystem::playLandingSound() {
    landed = true;
    currentSound = PlaneSound::NONE;
    
    // Stop looping sounds and play landing
    std::string fullPath = getFullPath("planlanding.wav");
    PlaySoundA(fullPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

void SoundSystem::playCoinSound() {
    // Play coin collection sound - this one doesn't stop engine sounds
    // We use a simple approach: just play the sound, it may briefly interrupt
    // For a proper implementation, we'd need multiple audio channels
    std::string fullPath = getFullPath("coin.wav");
    
    // Use sndPlaySound which can play simultaneously (sort of)
    // Actually PlaySound only supports one at a time, so we'll accept the brief interruption
    // and then the update() will restart the engine sound next frame
    PlaySoundA(fullPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    
    // Force engine sound to restart on next update
    currentSound = PlaneSound::NONE;
}

void SoundSystem::playTouchdownSound() {
    // Play landing sound but don't stop engine - it will continue after
    std::string fullPath = getFullPath("planlanding.wav");
    PlaySoundA(fullPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    
    // Force engine sound to restart on next update
    currentSound = PlaneSound::NONE;
}

void SoundSystem::stopAll() {
    PlaySoundA(NULL, NULL, 0);
    currentSound = PlaneSound::NONE;
}

void SoundSystem::reset() {
    crashed = false;
    landed = false;
    currentSound = PlaneSound::NONE;
    previousSound = PlaneSound::NONE;
    PlaySoundA(NULL, NULL, 0);
}
