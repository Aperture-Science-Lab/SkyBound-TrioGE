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

// Structure for Rings to fly through
struct Ring {
    Vector3f position;
    float radius;           // Ring radius
    bool passed;            // Has player passed through
    bool isGolden;          // Final ring is golden
    float rotationAngle;    // For spinning animation
};

// Structure for Toolkit Collectables
struct Toolkit {
    Vector3f position;
    bool collected;
    float bobOffset;        // For up/down animation
    float rotationAngle;    // For spinning animation
};

// Structure for Incoming Rockets
struct Rocket {
    Vector3f position;
    Vector3f velocity;
    bool active;
    float lifetime;
};

class Level1 : public Level {
public:
    Level1();
    ~Level1() override;
    
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
    
    // Models
    Model_3DS model_carrier;        // Aircraft carrier
    Model_3DS model_wrench;         // Toolkit/wrench collectable
    Model_3DS model_crane;          // Port crane
    Model_3DS model_container;      // Shipping container
    Model_3DS model_helipad;        // Helipad
    Model_3DS model_tents;          // Tents
    Model_3DS model_tank;           // Tank
    Model_3DS model_truck;          // Truck
    Model_3DS model_rocket;         // Rocket
    Model_3DS model_boat;           // Boat
    Model_3DS model_humvee;         // Humvee
    
    // Textures
    GLuint tex_water;               // Water texture
    GLuint tex_concrete;            // Port concrete texture
    GLuint tex_carrier;             // Carrier deck texture
    GLuint tex_rings;               // Rings and rockets texture
    GLuint tex_rocket;              // Rocket texture
    GLuint tex_container_red;       // Red container texture
    GLuint tex_container_blue;      // Blue container texture
    GLuint tex_container_yellow;    // Yellow container texture
    GLuint tex_helipad_metal;       // Helipad metal texture
    GLuint tex_tent;                // Tent texture
    GLuint tex_tank_rubber;         // Tank rubber texture
    GLuint tex_tank1;               // Tank texture 1
    GLuint tex_tank2;               // Tank texture 2
    GLuint tex_tank3;               // Tank texture 3
    GLuint tex_tank4;               // Tank texture 4
    GLuint tex_boat;                // Boat texture
    GLuint tex_humvee;              // Humvee texture

    // Boats
    struct BoatInstance {
        Vector3f position;
        Vector3f startPosition;
        Vector3f endPosition;
        Vector3f forwardDir;     // Facing direction in world space
        float moveSpeed;
        float phase;
        float bobSpeed;
        float bobAmount;
        float pathLength;
        bool isMoving;
        bool movingForward;
    };
    std::vector<BoatInstance> boats;
    
    // Particle Effects System
    ParticleEffects particleEffects;
    
    // Sky System (shared/reusable)
    SkySystem skySystem;
    
    // Screen dimensions
    int screenWidth;
    int screenHeight;
    
    // Aircraft Carrier (takeoff platform)
    Vector3f carrierPosition;
    float carrierRotation;
    float carrierScale;
    void renderCarrier();
    
    // Port/Ground System
    float waterLevel;
    float portHeight;               // Port is higher than water
    void renderGround();
    void renderWater();

    void renderPort();
    void drawLighthouse();
    
private:
    GLuint tex_lighthouse_wall;
    GLuint tex_lighthouse_top;
    // Ring System
    std::vector<Ring> rings;
    int ringsPassedCount;
    int totalRings;
    float ringTimer;                // For animation
    void initRings();
    void updateRings(float deltaTime);
    void renderRings();
    void checkRingPassage();
    void renderRing(const Ring& ring);
    
    // Toolkit Collectable System
    std::vector<Toolkit> toolkits;
    int collectedCount;
    float collectableTimer;
    void initToolkits();
    void updateToolkits(float deltaTime);
    void renderToolkits();
    void checkToolkitCollision();
    
    // Rocket/Missile System
    std::vector<Rocket> rockets;
    float rocketSpawnTimer;
    float rocketSpawnInterval;
    void initRockets();
    void updateRockets(float deltaTime);
    void renderRockets();
    void spawnRocket();
    void checkRocketCollision();
    
    // Game State
    float gameTimer;
    float maxGameTime;
    int score;
    bool gameOver;
    bool showGameOver;
    bool levelComplete;
    bool showLevelComplete;
    float levelCompleteTimer;
    
    // Spawn protection
    float spawnProtectionTimer;  // Invulnerability at start
    bool hasSpawnProtection;
    
    // Unified Crash System
    CrashSystem crashSystem;
    
    // Sound System
    SoundSystem soundSystem;
    
    // Shadow System
    ShadowSystem shadowSystem;
    void renderShadows();

    // Boats
    void initBoats();
    void updateBoats(float deltaTime);
    void renderBoats();
    
    // Shooting System
    ShootingSystem shootingSystem;
    void checkBulletRocketCollision();
    
    // Port lights (turn on at night)
    void renderPortLights();
    
    // HUD and UI
    void renderHUD();
    void renderGameOverScreen();
    void renderLevelCompleteScreen();
    
    // Collision with carrier deck (solid ground)
    bool isOnCarrierDeck(const Vector3f& pos);
    
    void loadAssets();
};
