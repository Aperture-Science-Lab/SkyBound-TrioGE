#pragma once
#include <stdlib.h>
#include <glut.h>
#include <math.h>
#include "Model_3DS.h"

#define PI 3.14159265359
#define DEG2RAD(x) ((x) * PI / 180.0f)

struct Vector3 {
    float x, y, z;
    Vector3(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
    
    Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
    Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
    Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
    
    void normalize() {
        float len = sqrt(x*x + y*y + z*z);
        if (len > 0) { x /= len; y /= len; z /= len; }
    }
};

struct FlightPlayer {
    Vector3 position;
    Vector3 forward;
    Vector3 up;
    Vector3 right;
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
    
    void rotateVector(Vector3& vec, const Vector3& axis, float angle);
};
