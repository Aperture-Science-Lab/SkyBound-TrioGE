#pragma once
#include "Level.h"
#include "FlightController.h"
#include "Model_3DS.h"
#include "GLTexture.h"
#include "SkySystem.h"
#include "ParticleEffects.h"
#include "CrashSystem.h"
#include "SoundSystem.h"
#include <vector>

// Forward declaration
void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

// Structure for Fuel Container Collectables
struct FuelCollectable {
    Vector3f position;
    bool collected;
    float bobOffset;      // For up/down animation
    float rotationAngle;  // For spinning animation
    float glowIntensity;  // For pulsing glow effect
};

// Structure for Building Obstacles
struct BuildingObstacle {
    Vector3f position;
    int modelIndex;       // Which building model to use (0-9)
    float rotation;       // Y-axis rotation
    float scale;          // Scale factor
    float width;          // Collision box width
    float height;         // Collision box height
    float depth;          // Collision box depth
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
    Model_3DS model_fuelContainer;  // Fuel container model
    Model_3DS model_buildings[10];  // 10 different building models
    Model_3DS model_airport;        // Airport runway model
    Model_3DS model_city;           // City model with buildings
    bool cityModelLoaded;           // Flag to track if city loaded successfully
    GLTexture tex_ground;
    
    // Particle Effects System (shared between levels)
    ParticleEffects particleEffects;
    
    // City Model System
    Vector3f cityPosition;
    float cityRotation;
    float cityScale;
    void initCity();
    void renderCity();
    
    // Sky and Lens Flare System (shared/reusable)
    SkySystem skySystem;
    
    int screenWidth;
    int screenHeight;
    
    // Airport/Landing Target System
    Vector3f airportPosition;
    float airportRotation;
    float airportScale;
    float arrowBobOffset;           // For arrow animation
    bool hasLanded;                 // Win condition
    bool showWinMessage;
    float winMessageTimer;
    float runwayLightTimer;         // Timer for runway light animation
    void initAirport();
    void renderAirport();
    void renderRunwayLights(bool isNight);  // Render runway indicator lights
    void renderTargetArrow();
    void checkLandingCondition();
    void renderWinScreen();
    
    // Game Timer and Scoring System
    float gameTimer;                // Countdown timer
    float maxGameTime;              // Total time allowed
    int score;                      // Player score
    int landingBonus;               // Bonus for early landing
    bool gameOver;                  // Time ran out
    bool showGameOver;
    void renderGameOverScreen();
    void calculateScore();
    
    // Unified Crash System (explosion + smoke + sound)
    CrashSystem crashSystem;
    
    // Sound System (idle + flying + crash sounds)
    SoundSystem soundSystem;
    
    // Building Obstacle System
    std::vector<BuildingObstacle> buildings;
    void initBuildings();
    void renderBuildings();
    void checkBuildingCollision();
    
    // Fuel Collectable System
    std::vector<FuelCollectable> fuelContainers;
    int collectedCount;
    float collectableTimer;  // For animation timing
    void initFuelContainers();
    void updateFuelContainers(float deltaTime);
    void renderFuelContainers();
    void checkFuelCollision();
    void renderHUD();
    
    void renderGround();
    void loadAssets();
};