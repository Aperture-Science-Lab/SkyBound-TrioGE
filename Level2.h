#pragma once
#include "Level.h"
#include "FlightController.h"
#include "Model_3DS.h"
#include "GLTexture.h"

// Forward declaration
void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

class Level2 : public Level {
public:
    Level2();
    ~Level2() override;
    
    // Override base class methods
    void init() override;
    void update(float deltaTime) override;
    void render() override;
    void cleanup() override;
    
    void handleKeyboard(unsigned char key, bool pressed) override;
    void handleKeyboardUp(unsigned char key) override;
    void handleMouse(int x, int y) override;
    
    void onEnter() override;
    void onExit() override;
    
private:
    // Flight simulator components
    FlightController* flightSim;
    
    // Models
    Model_3DS model_house;
    Model_3DS model_tree;
    
    // Textures
    GLTexture tex_ground;
    unsigned int tex_sky;
    
    // Window dimensions for mouse centering
    int screenWidth;
    int screenHeight;
    
    // Rendering helper methods
    void renderGround();
    void renderSky();
    void loadAssets();
};
