#include "FlightController.h"
#include <stdio.h>

FlightController::FlightController() {
    player.position = Vector3(0.0f, 40.0f, 0.0f);
    player.forward = Vector3(0.0f, 0.0f, -1.0f);
    player.up = Vector3(0.0f, 1.0f, 0.0f);
    player.right = Vector3(1.0f, 0.0f, 0.0f);
    player.speed = 40.0f;
    
    cameraMode = 0;
    cameraDist = 30.0f; // Increased distance for better view
    cameraHeight = 8.0f; 

    for(int i=0; i<256; i++) keyState[i] = false;
}

void FlightController::rotateVector(Vector3& vec, const Vector3& axis, float angle) {
    float rad = DEG2RAD(angle);
    float c = cos(rad);
    float s = sin(rad);
    
    // Rodrigues' rotation formula
    Vector3 k = axis;
    k.normalize();
    
    // v_rot = v * cos(theta) + (k x v) * sin(theta) + k * (k . v) * (1 - cos(theta))
    
    // Cross product k x v
    Vector3 k_cross_v(
        k.y * vec.z - k.z * vec.y,
        k.z * vec.x - k.x * vec.z,
        k.x * vec.y - k.y * vec.x
    );
    
    // Dot product k . v
    float k_dot_v = k.x * vec.x + k.y * vec.y + k.z * vec.z;
    
    Vector3 term1 = vec * c;
    Vector3 term2 = k_cross_v * s;
    Vector3 term3 = k * (k_dot_v * (1.0f - c));
    
    vec = term1 + term2 + term3;
    vec.normalize();
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

    float sensitivity = 0.08f; // Increased sensitivity slightly

    // Mouse X -> Roll (Rotate Up and Right around Forward)
    // Inverted X for natural feel: Moving mouse right rolls right
    if (fabs(dx) > 0.1f) {
        rotateVector(player.up, player.forward, -dx * sensitivity);
        rotateVector(player.right, player.forward, -dx * sensitivity);
    }

    // Mouse Y -> Pitch (Rotate Forward and Up around Right)
    // Pulling back (positive dy) should pitch up
    if (fabs(dy) > 0.1f) {
        rotateVector(player.forward, player.right, -dy * sensitivity);
        rotateVector(player.up, player.right, -dy * sensitivity);
    }
    
    // Re-orthogonalize to prevent drift
    // Forward is master, Right is derived, Up is derived
    player.forward.normalize();
    
    // Right = Forward x Up (temp)
    // Actually, let's just re-cross
    Vector3 tempRight(
        player.forward.y * player.up.z - player.forward.z * player.up.y,
        player.forward.z * player.up.x - player.forward.x * player.up.z,
        player.forward.x * player.up.y - player.forward.y * player.up.x
    );
    player.right = tempRight;
    player.right.normalize();
    
    // Up = Right x Forward
    player.up = Vector3(
        player.right.y * player.forward.z - player.right.z * player.forward.y,
        player.right.z * player.forward.x - player.right.x * player.forward.z,
        player.right.x * player.forward.y - player.right.y * player.forward.x
    );
    player.up.normalize();
}

void FlightController::update(float deltaTime) {
    // Thrust
    if (keyState['w'] || keyState['W'])
        player.speed += 30.0f * deltaTime;
    if (keyState['s'] || keyState['S'])
        player.speed -= 30.0f * deltaTime;

    // Clamp speed
    if (player.speed < 10.0f) player.speed = 10.0f; // Minimum speed to fly
    if (player.speed > 200.0f) player.speed = 200.0f;

    // Yaw (Rudder) - Rotate Forward and Right around Up
    float yawSpeed = 40.0f;
    if (keyState['a'] || keyState['A']) {
        rotateVector(player.forward, player.up, yawSpeed * deltaTime);
        rotateVector(player.right, player.up, yawSpeed * deltaTime);
    }
    if (keyState['d'] || keyState['D']) {
        rotateVector(player.forward, player.up, -yawSpeed * deltaTime);
        rotateVector(player.right, player.up, -yawSpeed * deltaTime);
    }

    // Move along forward vector
    player.position = player.position + (player.forward * player.speed * deltaTime);
}

void FlightController::setupCamera() {
    if (cameraMode == 1) {
        // 1st Person (Cockpit View)
        // Position camera slightly forward and up from center
        Vector3 camPos = player.position + (player.forward * 4.0f) + (player.up * 1.5f);
        Vector3 target = camPos + player.forward;
        
        gluLookAt(camPos.x, camPos.y, camPos.z,
                  target.x, target.y, target.z,
                  player.up.x, player.up.y, player.up.z);
    } else {
        // 3rd Person (Chase Cam)
        // Position camera behind and above
        Vector3 camPos = player.position - (player.forward * cameraDist) + (player.up * cameraHeight);
        Vector3 target = player.position + (player.forward * 20.0f); // Look ahead of the plane
        
        gluLookAt(camPos.x, camPos.y, camPos.z,
                  target.x, target.y, target.z,
                  player.up.x, player.up.y, player.up.z);
    }
}

void FlightController::loadModel(char* path) {
    model.Load(path);
    
    // Manually load the texture for the plane
    // This overrides any texture defined in the 3DS file
    // We apply it to all materials assuming it's a texture atlas
    for (int i = 0; i < model.numMaterials; i++) {
        model.Materials[i].tex.Load("models/plane/mitsubishi_a6m2_zero_texture.bmp");
        model.Materials[i].textured = true;
    }
}

void FlightController::drawPlane() {
    glPushMatrix();
    glTranslatef(player.position.x, player.position.y, player.position.z);
    
    // Construct rotation matrix from basis vectors
    // OpenGL uses column-major order
    float rotMatrix[16] = {
        player.right.x, player.right.y, player.right.z, 0,
        player.up.x,    player.up.y,    player.up.z,    0,
        -player.forward.x, -player.forward.y, -player.forward.z, 0, // Forward is -Z in OpenGL
        0,              0,              0,              1
    };
    
    glMultMatrixf(rotMatrix);

    // Scale the model
    // Adjust these values based on the actual model size
    glScalef(1, 1, 1); 
    
    // Rotate if necessary (many models are Y-up, but might need adjustment)
    glRotatef(90, 1, 0, 0);
    
    model.Draw();
    
    glPopMatrix();
}
