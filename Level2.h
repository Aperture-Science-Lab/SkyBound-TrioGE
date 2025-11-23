#pragma once
#include "Level.h"
#include "FlightController.h"
#include "Model_3DS.h"
#include "GLTexture.h"
#include <vector>

// Forward declaration
void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

// Structure for Wind Strips
struct WindParticle {
    Vector3f position;
    float length;
    float speed;
    float life;
    bool active;
};

class Level2 : public Level {
public:
    Level2();
    ~Level2() override;
    
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
    FlightController* flightSim;
    
    Model_3DS model_house;
    Model_3DS model_tree;
    GLTexture tex_ground;
    unsigned int tex_sky;
    
    int screenWidth;
    int screenHeight;
    
    // Wind Effect System
    std::vector<WindParticle> windParticles;
    void initWindParticles();
    void updateWindParticles(float deltaTime);
    void renderWindParticles();

    void renderGround();
    void renderSky();
    void loadAssets();
};