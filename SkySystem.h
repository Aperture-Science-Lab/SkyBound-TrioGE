#pragma once
#include "glew.h"
#include <glut.h>
#include "Vector3f.h"

class SkySystem {
public:
    SkySystem();
    ~SkySystem();
    
    // Initialize the sky system (generates textures)
    void init();
    
    // Render the sky sphere centered on player position
    void renderSky(const Vector3f& playerPosition);
    
    // Render lens flare effect (call after scene, before HUD)
    void renderLensFlare(const Vector3f& playerPosition, const Vector3f& playerForward, 
                         int screenWidth, int screenHeight);
    
    // Set sun direction (normalized)
    void setSunDirection(const Vector3f& dir);
    
    // Get sun direction for lighting
    Vector3f getSunDirection() const { return sunDirection; }
    
private:
    unsigned int tex_sky;
    unsigned int tex_flare[10];  // More flare textures for AAA quality
    
    Vector3f sunDirection;
    float sunIntensity;
    
    // Load sky texture from BMP file
    bool loadSkyTexture(const char* filename);
    
    // Generate lens flare textures (AAA quality)
    void generateFlareTextures();
    
    // Render sun glow on skybox
    void renderSunGlow(const Vector3f& playerPosition);
    
    // Check if sun is visible and get screen position
    bool isSunVisible(const Vector3f& playerPosition, const Vector3f& playerForward,
                      int screenWidth, int screenHeight, float& screenX, float& screenY);
};
