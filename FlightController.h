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
    Vector3f forward;
    Vector3f up;
    Vector3f right;
    float speed;
};

class FlightController {
public:
    FlightController();
    
    void update(float deltaTime);
    void handleInput(unsigned char key, bool pressed);
    void handleMouse(int x, int y, int centerX, int centerY);
    void setupCamera();
    void drawPlane();
    void loadModel(char* path);
    
    FlightPlayer player;
    Model_3DS model;
    bool keyState[256];
    int cameraMode; // 0 = 3rd Person, 1 = 1st Person
    
private:
    float cameraDist;
    float cameraHeight;
    
    void rotateVector(Vector3f& vec, const Vector3f& axis, float angle);
};
