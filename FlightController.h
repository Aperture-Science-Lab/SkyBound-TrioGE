#pragma once
#include <stdlib.h>
#include <glut.h>
#include <math.h>
#include <Vector3f.h>
#include "Model_3DS.h"

#define PI 3.14159265359
#define DEG2RAD(x) ((x) * PI / 180.0f)

struct FlightPlayer {
    Vector3f position;
    Vector3f velocity;      // Actual movement vector
    Vector3f acceleration;
    
    // Orientation vectors
    Vector3f forward;
    Vector3f up;
    Vector3f right;
    
    float throttle;         // 0.0 to 100.0
};

class FlightController {
public:
    FlightController();

    void reset(); // Reset the controller to initial state
    void update(float deltaTime);
    void handleInput(unsigned char key, bool pressed);
    void handleMouse(int x, int y, int centerX, int centerY);
    
    // Rendering helpers
    void setupCamera();     // Sets up ModelView matrix
    float getFOV();         // Returns calculated FOV based on speed
    void drawPlane();
    void loadModel(char* path);
    
    FlightPlayer player;
    Model_3DS model;
    
    // State
    bool keyState[256];
    int cameraMode;         // 0 = 3rd Person, 1 = 1st Person
    bool isCrashed;
    bool isGrounded;
    
private:
    float cameraDist;
    float cameraHeight;
    
    // Physics Constants
    const float GRAVITY = 9.8f;
    const float LIFT_FACTOR = 0.002f;
    const float DRAG_FACTOR = 0.01f;
    const float THRUST_POWER = 30.0f;
    const float MASS = 1.0f;
    
    // Effects
    float viewBobTimer;
    float currentFOV;
    
    void rotateVector(Vector3f& vec, const Vector3f& axis, float angle);
    void resolveCollisions(float deltaTime);
    void applyPhysics(float deltaTime);
};