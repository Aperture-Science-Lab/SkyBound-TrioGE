#pragma once
#include <stdlib.h>
#include "glew.h"
#include <glut.h>
#include <math.h>
#include <Vector3f.h>
#include "Model_3DS.h"
#include "SmokeSystem.h"

#define PI 3.14159265359
#define DEG2RAD(x) ((x) * PI / 180.0f)

struct FlightPlayer {
    Vector3f position;
    Vector3f velocity;
    Vector3f acceleration;
    
    Vector3f forward;
    Vector3f up;
    Vector3f right;
    
    float throttle; 
};

class FlightController {
public:
    FlightController();

    void reset();
    void update(float deltaTime);
    void handleInput(unsigned char key, bool pressed);
    void handleMouse(int x, int y, int centerX, int centerY);
    
    void setupCamera();
    float getFOV();
    void drawPlane();
    void loadModel(char* path);
    
    FlightPlayer player;
    Model_3DS model;
    
    bool keyState[256];
    int cameraMode;
    bool isCrashed;
    bool isGrounded;
    
    // Smoke particle system for crash effects
    SmokeSystem smokeSystem;
    
    // Accessor for speed (used by Level2 for wind particles)
    float getSpeed() const;
    
    // Render smoke particles (call after scene rendering)
    void renderSmoke();

private:
    float cameraDist;
    float cameraHeight;
    
    // Tuned Physics Constants
    const float GRAVITY = 9.8f;
    const float LIFT_FACTOR = 0.0025f;  // Adjusted for realistic takeoff
    const float DRAG_FACTOR = 0.008f;
    const float THRUST_POWER = 40.0f;   // Increased slightly for taxiing
    const float MASS = 1.0f;
    const float TAKEOFF_SPEED = 50.0f;  // Speed required to generate lift
    const float WHEEL_FRICTION = 2.0f;  // Friction when on ground
    
    // Effects
    float viewBobTimer;
    float currentFOV;
    bool wasCrashed;  // Track crash state for smoke triggering
    
    void rotateVector(Vector3f& vec, const Vector3f& axis, float angle);
    void resolveCollisions(float deltaTime);
    void applyPhysics(float deltaTime);
    void handleGroundControls(float deltaTime);
};