#pragma once
#include <glut.h>
#include <math.h>

#define PI 3.14159265359
#define DEG2RAD(x) ((x) * PI / 180.0f)

struct Player {
    float x, y, z;
    float pitch, yaw, roll;
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
    
    Player player;
    bool keyState[256];
    int cameraMode; // 0 = 3rd Person, 1 = 1st Person
    
private:
    float cameraDist;
    float cameraHeight;
};
