#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <string>

#pragma comment(lib, "winmm.lib")

// Sound states for the plane
enum class PlaneSound {
    NONE,
    IDLE,
    FLYING
};

// Unified Sound System - handles all game sounds
// Designed to be shared across all levels
class SoundSystem {
public:
    SoundSystem();
    ~SoundSystem();
    
    // Initialize the sound system
    void init();
    
    // Update sound based on plane state (call every frame)
    // isGrounded: true if plane is on ground
    // speed: current speed of the plane
    // Returns true if a touchdown was detected (flying -> idle transition)
    bool update(bool isGrounded, float speed);
    
    // Play crash sound (one-shot)
    void playCrashSound();
    
    // Play landing sound (one-shot)
    void playLandingSound();
    
    // Play coin/fuel collection sound (one-shot, doesn't stop engine)
    void playCoinSound();
    
    // Play touchdown sound (when landing anywhere, not stopping engine)
    void playTouchdownSound();
    
    // Stop all sounds
    void stopAll();
    
    // Reset (stop all and allow new sounds)
    void reset();
    
private:
    PlaneSound currentSound;
    PlaneSound previousSound;  // Track previous state for transition detection
    bool initialized;
    bool crashed;
    bool landed;
    std::string basePath;  // Absolute path to sound folder
    
    // Helper to play a looping sound
    void playLoopingSound(const char* filename);
    
    // Helper to get full path to a sound file
    std::string getFullPath(const char* filename);
};
