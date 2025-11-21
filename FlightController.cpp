#include "FlightController.h"
#include <stdio.h>

FlightController::FlightController() {
    player.x = 0.0f;
    player.y = 40.0f; // Start in the air
    player.z = 0.0f;
    player.pitch = 0.0f;
    player.yaw = 0.0f;
    player.roll = 0.0f;
    player.speed = 0.0f; // Start with 0 speed, or maybe some speed? User didn't specify speed, but "in air" usually implies flying. Let's keep 0 for safety or maybe 20? 
    // User said "start on the air", usually implies flying. Let's give it some speed.
    player.speed = 40.0f;

    
    cameraMode = 0;
    cameraDist = 15.0f;
    cameraHeight = 5.0f;
    
    for(int i=0; i<256; i++) keyState[i] = false;
}

void FlightController::handleInput(unsigned char key, bool pressed) {
    keyState[key] = pressed;
    
    if (pressed) {
        if (key == 'c' || key == 'C') {
            cameraMode = !cameraMode;
        }
    }
}

void FlightController::handleMouse(int x, int y, int centerX, int centerY) {
    if (x == centerX && y == centerY) return;

    float dx = (float)(x - centerX);
    float dy = (float)(y - centerY);

    float sensitivity = 0.05f; // Reduced sensitivity for better control

    // Mouse X -> Roll (Adds to Roll)
    player.roll += dx * sensitivity;

    // Mouse Y -> Pitch (Adds to Pitch)
    // Note: If Mouse Up (y < centerY), dy is negative.
    // If user wants "Mouse Y adds to Pitch", then we just do +=.
    // If they want "Up pitches nose down", and Up is negative dy, then adding negative dy decreases pitch (Nose Down).
    // This matches "Inverted" feel if Pitch+ is Nose Up.
    player.pitch += dy * sensitivity;

}

void FlightController::update(float deltaTime) {
    // Thrust
    if (keyState['w'] || keyState['W'])
        player.speed += 20.0f * deltaTime;
    if (keyState['s'] || keyState['S'])
        player.speed -= 20.0f * deltaTime;

    // Clamp speed
    if (player.speed < 0.0f) player.speed = 0.0f;
    if (player.speed > 100.0f) player.speed = 100.0f;

    // Yaw (Rudder)
    if (keyState['a'] || keyState['A'])
        player.yaw += 40.0f * deltaTime;
    if (keyState['d'] || keyState['D'])
        player.yaw -= 40.0f * deltaTime;

    // Physics
    float radYaw = DEG2RAD(player.yaw);
    float radPitch = DEG2RAD(player.pitch);

    float dirX = sin(radYaw) * cos(radPitch);
    float dirY = sin(radPitch);
    float dirZ = -cos(radYaw) * cos(radPitch);

    player.x += dirX * player.speed * deltaTime;
    player.y += dirY * player.speed * deltaTime;
    player.z += dirZ * player.speed * deltaTime;
}

void FlightController::setupCamera() {
    if (cameraMode == 1) {
        // 1st Person
        glRotatef(-player.roll, 0.0f, 0.0f, 1.0f);
        glRotatef(-player.pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(-player.yaw, 0.0f, 1.0f, 0.0f);
        glTranslatef(-player.x, -player.y, -player.z);
    } else {
        // 3rd Person
        glTranslatef(0.0f, -cameraHeight, -cameraDist);
        glRotatef(-player.roll, 0.0f, 0.0f, 1.0f);
        glRotatef(-player.pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(-player.yaw, 0.0f, 1.0f, 0.0f);
        glTranslatef(-player.x, -player.y, -player.z);
    }
}

void FlightController::drawPlane() {
    glPushMatrix();
    glTranslatef(player.x, player.y, player.z);
    
    // Apply rotations
    glRotatef(player.yaw, 0.0f, 1.0f, 0.0f);
    glRotatef(player.pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(player.roll, 0.0f, 0.0f, 1.0f);

    // Scale the model to be bigger
    glScalef(3.0f, 3.0f, 3.0f);

    glBegin(GL_TRIANGLES);

    // Body
    glColor3f(0.7f, 0.7f, 0.7f);
    glVertex3f(0.0f, 0.0f, -2.0f);
    glVertex3f(-0.5f, 0.0f, 1.0f);
    glVertex3f(0.5f, 0.0f, 1.0f);
    
    glColor3f(0.5f, 0.5f, 0.5f);
    glVertex3f(0.0f, 0.0f, -2.0f);
    glVertex3f(0.5f, 0.0f, 1.0f);
    glVertex3f(-0.5f, 0.0f, 1.0f);
    
    // Wings
    glColor3f(0.8f, 0.2f, 0.2f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(-2.0f, 0.0f, 0.5f);
    glVertex3f(-0.5f, 0.0f, 1.0f);
    
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.5f, 0.0f, 1.0f);
    glVertex3f(2.0f, 0.0f, 0.5f);
    
    // Tail
    glColor3f(0.2f, 0.2f, 0.8f);
    glVertex3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 1.0f, 1.5f);
    glVertex3f(0.0f, 0.0f, 1.5f);
    glEnd();
    
    glPopMatrix();
}
