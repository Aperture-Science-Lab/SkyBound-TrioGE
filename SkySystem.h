#pragma once
#include "glew.h"
#include <glut.h>
#include "Vector3f.h"

// Time of day phases for day/night cycle
enum class TimeOfDay {
    MORNING,    // sky.bmp - sunrise/early morning
    NOON,       // noonsky.bmp - bright midday
    SUNSET,     // sunsetsky.bmp - evening sunset
    NIGHT       // nightsky.bmp - dark night
};

// Lighting configuration for each time of day
struct DayLighting {
    float ambientR, ambientG, ambientB;
    float diffuseR, diffuseG, diffuseB;
    float sunHeight;  // Sun position (Y component)
    bool showLensFlare;
};

class SkySystem {
public:
    SkySystem();
    ~SkySystem();
    
    // Initialize the sky system (generates textures)
    void init();
    
    // Update day/night cycle (call every frame with deltaTime)
    void update(float deltaTime);
    
    // Render the sky sphere centered on player position
    void renderSky(const Vector3f& playerPosition);
    
    // Render lens flare effect (call after scene, before HUD)
    void renderLensFlare(const Vector3f& playerPosition, const Vector3f& playerForward, 
                         int screenWidth, int screenHeight);
    
    // Set sun direction (normalized)
    void setSunDirection(const Vector3f& dir);
    
    // Get sun direction for lighting
    Vector3f getSunDirection() const { return sunDirection; }
    
    // Get current time of day
    TimeOfDay getCurrentTimeOfDay() const { return currentTime; }
    
    // Get current lighting configuration (for scene lighting)
    DayLighting getCurrentLighting() const;
    
    // Check if it's night (for wing lights)
    bool isNightTime() const { return currentTime == TimeOfDay::NIGHT; }
    
    // Get cycle progress (0.0 to 1.0 for full day)
    float getCycleProgress() const { return cycleTimer / fullCycleDuration; }
    
    // Manually set time of day (for testing)
    void setTimeOfDay(TimeOfDay time);
    
private:
    // Sky textures for each phase
    unsigned int tex_sky_morning;
    unsigned int tex_sky_noon;
    unsigned int tex_sky_sunset;
    unsigned int tex_sky_night;
    unsigned int tex_flare[10];  // More flare textures for AAA quality
    
    Vector3f sunDirection;
    float sunIntensity;
    
    // Day/night cycle
    TimeOfDay currentTime;
    TimeOfDay previousTime;
    float cycleTimer;
    float transitionProgress;  // 0.0 to 1.0 for smooth transitions
    bool inTransition;
    
    // Cycle timing (in seconds)
    const float phaseDuration = 60.0f;      // 60 seconds per phase
    const float transitionDuration = 5.0f;  // 5 second transition
    const float fullCycleDuration = 240.0f; // 4 minutes full cycle
    
    // Load sky texture from BMP file (flipVertical: extra flip for textures that are upside down)
    bool loadSkyTexture(const char* filename, unsigned int& texId, bool flipVertical = false);
    
    // Generate lens flare textures (AAA quality)
    void generateFlareTextures();
    
    // Render sun glow on skybox
    void renderSunGlow(const Vector3f& playerPosition);
    
    // Check if sun is visible and get screen position
    bool isSunVisible(const Vector3f& playerPosition, const Vector3f& playerForward,
                      int screenWidth, int screenHeight, float& screenX, float& screenY);
    
    // Get texture ID for a time of day
    unsigned int getTextureForTime(TimeOfDay time);
    
    // Update sun position based on time
    void updateSunPosition();
    
    // Apply scene lighting based on time
    void applyLighting();
};
