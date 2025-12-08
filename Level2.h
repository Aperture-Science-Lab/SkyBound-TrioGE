#pragma once
#include "Level.h"
#include "FlightController.h"
#include "Model_3DS.h"
#include "GLTexture.h"
#include "SkySystem.h"
#include "ParticleEffects.h"
#include "CrashSystem.h"
#include "SoundSystem.h"
#include "ShadowSystem.h"
#include "ShootingSystem.h"
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
    Model_3DS model_airportTerminal; // Airport terminal model
    GLuint tex_airportTerminal;      // Airport terminal texture
    GLTexture tex_ground;
    GLuint tex_runway;              // Runway texture
    GLuint tex_grass;               // Grass billboard texture
    GLuint tex_fuelContainer;       // Fuel container texture
    
    // Optimized Billboard Grass System (no 3D models - much faster)
    static const int GRASS_GRID_SIZE = 12;      // Grid cells around player (reduced)
    static const int GRASS_PER_CELL = 2;        // Grass patches per cell
    float grassRenderDistance;                   // Max render distance
    float grassCellSize;                         // Size of each grid cell
    void renderGrass();
    
    // Particle Effects System (shared between levels)
    ParticleEffects particleEffects;
    
    // Airport Terminal
    Vector3f terminalPosition;
    float terminalRotation;
    float terminalScale;
    void renderAirportTerminal();
    
    // Sky and Lens Flare System (shared/reusable)
    SkySystem skySystem;
    
    int screenWidth;
    int screenHeight;
    
    // Airport/Landing Target System - Realistic Runway
    Vector3f runwayPosition;        // Center of runway
    float runwayRotation;           // Runway heading (degrees)
    float runwayLength;             // Length of runway
    float runwayWidth;              // Width of runway
    float arrowBobOffset;           // For arrow animation
    bool hasLanded;                 // Win condition
    bool showWinMessage;
    float winMessageTimer;
    float runwayLightTimer;         // Timer for runway light animation
    bool hasTouchedDown;            // Track if plane touched down on runway
    float touchdownLocalZ;          // Local Z position where plane touched down
    void initAirport();
    void renderAirport();           // Renders textured runway primitive
    void renderRunwayMarkings();    // White runway markings
    void renderRunwayLights(bool isNight);  // Runway indicator lights
    void renderPAPI(bool isNight);  // Precision Approach Path Indicator
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
    
    // Shadow System (blob shadows + ambient occlusion)
    ShadowSystem shadowSystem;
    void renderShadows();
    
    // Shooting System (bullets + ground explosions)
    ShootingSystem shootingSystem;
    
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