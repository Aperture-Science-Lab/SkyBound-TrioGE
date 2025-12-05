#include "FlightController.h"
#include <stdio.h>
#include <iostream>

FlightController::FlightController() {
    modelLoaded = false;  // Model needs to be loaded for this instance
    reset();
    // Closer camera as requested
    cameraDist = 15.0f;  // Was 30.0f
    cameraHeight = 5.0f; // Was 8.0f
    wasCrashed = false;
    wingLightTimer = 0.0f;
    smokeSystem.init();  // Initialize smoke particle system
}

void FlightController::reset() {
    player.position = Vector3f(0.0f, 0.5f, 0.0f); // Start on Ground
    player.velocity = Vector3f(0.0f, 0.0f, 0.0f);
    player.acceleration = Vector3f(0.0f, 0.0f, 0.0f);

    player.forward = Vector3f(0.0f, 0.0f, -1.0f);
    player.up = Vector3f(0.0f, 1.0f, 0.0f);
    player.right = Vector3f(1.0f, 0.0f, 0.0f);

    player.throttle = 0.0f;
    cameraMode = 0;
    isCrashed = false;
    isGrounded = true; // Start grounded
    viewBobTimer = 0.0f;
    currentFOV = 45.0f;
    wasCrashed = false;
    wingLightTimer = 0.0f;
    smokeSystem.reset();  // Clear smoke particles on reset

    for(int i=0; i<256; i++) keyState[i] = false;
}

float FlightController::getSpeed() const {
    return sqrt(player.velocity.x*player.velocity.x + 
                player.velocity.y*player.velocity.y + 
                player.velocity.z*player.velocity.z);
}

void FlightController::rotateVector(Vector3f& vec, const Vector3f& axis, float angle) {
    float rad = DEG2RAD(angle);
    float c = cos(rad);
    float s = sin(rad);
    Vector3f k = axis.unit();
    Vector3f k_cross_v(k.y*vec.z - k.z*vec.y, k.z*vec.x - k.x*vec.z, k.x*vec.y - k.y*vec.x);
    float k_dot_v = k.x*vec.x + k.y*vec.y + k.z*vec.z;
    vec = (vec * c) + (k_cross_v * s) + (k * (k_dot_v * (1.0f - c)));
}

void FlightController::handleInput(unsigned char key, bool pressed) {
    keyState[key] = pressed;
    if (pressed) {
        if (key == 'c' || key == 'C') cameraMode = !cameraMode;
        if (key == 'r' || key == 'R') reset();
    }
}

void FlightController::handleMouse(int x, int y, int centerX, int centerY) {
    if (isCrashed) return;
    if (x == centerX && y == centerY) return;

    float dx = (float)(x - centerX);
    float dy = (float)(y - centerY);
    float sensitivity = 0.1f;

    // If Grounded: Mouse Y (Pitch) works for takeoff rotation, Mouse X (Roll) is disabled
    // If Flying: Full control
    
    if (!isGrounded) {
        // Roll (Mouse X)
        if (fabs(dx) > 0.1f) {
            rotateVector(player.up, player.forward, -dx * sensitivity);
            rotateVector(player.right, player.forward, -dx * sensitivity);
        }
    }

    // Pitch (Mouse Y) - Always active (needed to lift nose for takeoff)
    if (fabs(dy) > 0.1f) {
        rotateVector(player.forward, player.right, dy * sensitivity);
        rotateVector(player.up, player.right, dy * sensitivity);
    }

    // Re-orthogonalize
    player.forward = player.forward.unit();
    player.right = player.right.unit();
    player.up = player.up.unit();
    player.right = player.forward.cross(player.up).unit();
    player.up = player.right.cross(player.forward).unit();
}

void FlightController::applyPhysics(float deltaTime) {
    if (isCrashed) return;

    // 1. Thrust
    Vector3f thrustForce = player.forward * player.throttle * THRUST_POWER;

    // 2. Gravity
    Vector3f gravityForce(0.0f, -GRAVITY * MASS, 0.0f);

    // 3. Lift
    float speed = getSpeed();
    float speedSq = speed * speed;
    Vector3f liftForce(0,0,0);

    // Calculate Angle of Attack (AoA) factor
    // Lift is max when forward vector is slightly pitched up relative to velocity
    float pitchFactor = player.forward.y + 0.5f; 
    if (pitchFactor < 0) pitchFactor = 0;

    // Only apply significant lift if moving fast enough
    if (speed > 10.0f) { 
        liftForce = player.up * (speedSq * LIFT_FACTOR * pitchFactor);
    }

    // 4. Drag
    Vector3f dragForce = player.velocity * (-DRAG_FACTOR * speed);

    // Total Force
    Vector3f totalForce = thrustForce + gravityForce + liftForce + dragForce;

    // Ground Friction (Wheel drag)
    if (isGrounded) {
        // Apply extra drag to stop plane when throttle is off
        totalForce = totalForce - (player.velocity * WHEEL_FRICTION);
        
        // Prevent sliding sideways (Wheel physics)
        // Project velocity onto Right vector
        float driftSpeed = player.velocity.x * player.right.x + 
                           player.velocity.y * player.right.y + 
                           player.velocity.z * player.right.z;
        
        // Apply opposing force to drift
        Vector3f driftForce = player.right * (-driftSpeed * 5.0f); // Strong lateral friction
        totalForce = totalForce + driftForce;
    }

    // Integration
    player.acceleration = totalForce * (1.0f / MASS);
    player.velocity = player.velocity + (player.acceleration * deltaTime);
    player.position = player.position + (player.velocity * deltaTime);
}

void FlightController::update(float deltaTime) {
    // Update wing light timer for blinking effect
    wingLightTimer += deltaTime;
    if (wingLightTimer > 2.0f) wingLightTimer -= 2.0f;  // Reset every 2 seconds
    
    if (!isCrashed) {
        // Throttle
        if (keyState['w'] || keyState['W']) player.throttle += 0.5f * deltaTime;
        if (keyState['s'] || keyState['S']) player.throttle -= 0.5f * deltaTime;
        
        // Control Logic Switch
        if (isGrounded) {
            // --- TAXI MODE ---
            // A/D controls YAW (Steering), not Roll
            float turnSpeed = 30.0f; // Degrees per second
            // Only steer if moving
            float steerEffect = getSpeed() / 10.0f;
            if (steerEffect > 1.0f) steerEffect = 1.0f;

            if (keyState['a'] || keyState['A']) {
                rotateVector(player.forward, player.up, turnSpeed * steerEffect * deltaTime);
                rotateVector(player.right, player.up, turnSpeed * steerEffect * deltaTime);
            }
            if (keyState['d'] || keyState['D']) {
                rotateVector(player.forward, player.up, -turnSpeed * steerEffect * deltaTime);
                rotateVector(player.right, player.up, -turnSpeed * steerEffect * deltaTime);
            }
            
            // Keep wings level while taxiing (auto-level roll)
            if (player.right.y != 0) {
                rotateVector(player.up, player.forward, -player.right.y * 2.0f * deltaTime);
                rotateVector(player.right, player.forward, -player.right.y * 2.0f * deltaTime);
            }

        } else {
            // --- FLIGHT MODE ---
            // A/D controls YAW (Rudder)
            if (keyState['a'] || keyState['A']) {
                rotateVector(player.forward, player.up, 40.0f * deltaTime);
                rotateVector(player.right, player.up, 40.0f * deltaTime);
            }
            if (keyState['d'] || keyState['D']) {
                rotateVector(player.forward, player.up, -40.0f * deltaTime);
                rotateVector(player.right, player.up, -40.0f * deltaTime);
            }
        }
    }
    
    // Clamp throttle
    if (player.throttle < 0.0f) player.throttle = 0.0f;
    if (player.throttle > 1.0f) player.throttle = 1.0f; // 100%

    applyPhysics(deltaTime);
    resolveCollisions(deltaTime);

    // Update View Bobbing
    float speed = getSpeed();
    if (speed > 0.1f && !isCrashed) {
        // Increase frequency with speed
        viewBobTimer += deltaTime * (speed * 0.2f);
    }
    
    // Handle smoke particles for crash effect
    if (isCrashed && !wasCrashed) {
        // Just crashed - start smoke
        smokeSystem.startSmoke(player.position);
        wasCrashed = true;
    }
    
    // Update smoke particles
    smokeSystem.update(deltaTime);
}

void FlightController::resolveCollisions(float deltaTime) {
    // Ground check
    if (player.position.y <= 0.5f) {
        player.position.y = 0.5f;
        
        // Check landing/crash conditions
        if (!isGrounded) {
            float verticalSpeed = player.velocity.y;
            float speed = getSpeed();
            
            // Hard crash: Falling too fast or hitting ground nose first
            if (verticalSpeed < -10.0f || (speed > 40.0f && player.forward.y < -0.2f)) {
                isCrashed = true;
                player.throttle = 0;
                std::cout << "CRASHED! Ground Impact." << std::endl;
            } else {
                // Successful landing
                isGrounded = true;
                player.velocity.y = 0; // Stop falling
                
                // Flatten out pitch slightly on landing
                if (player.forward.y < 0) player.forward.y = 0;
            }
        } else {
            // Already on ground
            player.velocity.y = 0; // Cancel gravity accumulation
            
            // Takeoff Check
            // If speed > takeoff speed AND nose is pitched up
            float speed = getSpeed();
            if (speed > TAKEOFF_SPEED && player.forward.y > 0.1f) {
                isGrounded = false; // Liftoff!
                player.position.y += 0.1f; // Unstick
            }
        }
    } else {
        // In the air
        isGrounded = false;
    }
}

float FlightController::getFOV() {
    float speed = getSpeed();
    float targetFOV = 45.0f + (speed * 0.4f); // Dynamic FOV
    if (targetFOV > 100.0f) targetFOV = 100.0f;
    
    currentFOV += (targetFOV - currentFOV) * 0.05f; // Smooth transition
    return currentFOV;
}

void FlightController::setupCamera() {
    // Realistic View Bobbing (Turbulence + Engine Vibration)
    float speed = getSpeed();
    float shakeMag = (speed / 100.0f) * 0.05f; // Very subtle, scales with speed
    if (shakeMag > 0.08f) shakeMag = 0.08f;    // Cap vibration

    // High frequency vibration (engine)
    float vibX = ((float)(rand() % 100) / 100.0f - 0.5f) * shakeMag;
    float vibY = ((float)(rand() % 100) / 100.0f - 0.5f) * shakeMag;
    
    // Low frequency sway (wind)
    float swayX = sin(viewBobTimer * 2.0f) * shakeMag * 2.0f;
    float swayY = cos(viewBobTimer * 1.5f) * shakeMag * 2.0f;

    Vector3f offset = (player.right * (vibX + swayX)) + (player.up * (vibY + swayY));

    if (isCrashed) {
        gluLookAt(player.position.x + 20, player.position.y + 10, player.position.z + 20,
                  player.position.x, player.position.y, player.position.z,
                  0, 1, 0);
        return;
    }

    Vector3f camPos, target;

    if (cameraMode == 1) { // Cockpit
        camPos = player.position + (player.forward * 2.0f) + (player.up * 1.2f) + offset;
        target = camPos + player.forward;
    } else { // 3rd Person (Closer now)
        camPos = player.position - (player.forward * cameraDist) + (player.up * cameraHeight) + offset;
        target = player.position + (player.forward * 10.0f);
    }

    gluLookAt(camPos.x, camPos.y, camPos.z,
              target.x, target.y, target.z,
              player.up.x, player.up.y, player.up.z);
}

void FlightController::drawPlane() {
    drawPlane(false);  // Default: no wing lights
}

void FlightController::drawPlane(bool showWingLights) {
    FlightController::loadModel("models/plane/mitsubishi_a6m2_zero_model_11.3ds"); // Ensure model is loaded
    
    if(isCrashed) glColor3f(1,0,0);
    else glColor3f(1,1,1);

    glPushMatrix();
    glTranslatef(player.position.x, player.position.y, player.position.z);
    
    float rotMatrix[16] = {
        player.right.x, player.right.y, player.right.z, 0,
        player.up.x,    player.up.y,    player.up.z,    0,
        -player.forward.x, -player.forward.y, -player.forward.z, 0,
        0,              0,              0,              1
    };
    
    glMultMatrixf(rotMatrix);
    glScalef(1, 1, 1); 
    glRotatef(90, 1, 0, 0);
    
    model.Draw();
    
    // Render wing lights if enabled (for night time)
    if (showWingLights) {
        renderWingLights();
    }
    
    glPopMatrix();
    glColor3f(1,1,1);
}

void FlightController::renderWingLights() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for glow
    
    // Wing light positions (relative to plane center in model space)
    // Note: Model is rotated 90 degrees around X axis, so:
    // Model X = World X (left/right)
    // Model Y = World -Z (forward/back in model becomes up/down after rotation)
    // Model Z = World Y (up/down in model becomes forward/back after rotation)
    
    // Wing tip positions - moved further out from plane body
    float wingSpan = 4.0f;     // Further out on wings (X axis - left/right)
    float wingY = 0.2f;        // Slightly below center (becomes Z after rotation)
    float wingZ = 0.0f;        // Center of wing chord (becomes -Y after rotation)
    
    // Blinking effect - on for 0.15s, off for 0.85s every second
    bool lightOn = (fmod(wingLightTimer, 1.0f) < 0.15f);
    
    // Strobe white light on tail (anti-collision) - different timing
    bool strobeOn = (fmod(wingLightTimer + 0.5f, 1.0f) < 0.1f);
    
    // Left wing tip - RED light (port side)
    if (lightOn) {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);  // Bright red
        glPushMatrix();
        glTranslatef(-wingSpan, wingY, wingZ);  // Left wing tip
        glutSolidSphere(0.25f, 8, 8);
        glPopMatrix();
        
        // Red glow halo
        glColor4f(1.0f, 0.0f, 0.0f, 0.4f);
        glPushMatrix();
        glTranslatef(-wingSpan, wingY, wingZ);
        glutSolidSphere(0.6f, 8, 8);
        glPopMatrix();
    }
    
    // Right wing tip - GREEN light (starboard side)
    if (lightOn) {
        glColor4f(0.0f, 1.0f, 0.0f, 1.0f);  // Bright green
        glPushMatrix();
        glTranslatef(wingSpan, wingY, wingZ);  // Right wing tip
        glutSolidSphere(0.25f, 8, 8);
        glPopMatrix();
        
        // Green glow halo
        glColor4f(0.0f, 1.0f, 0.0f, 0.4f);
        glPushMatrix();
        glTranslatef(wingSpan, wingY, wingZ);
        glutSolidSphere(0.6f, 8, 8);
        glPopMatrix();
    }
    
    // Tail strobe - WHITE anti-collision light (on top of vertical stabilizer)
    if (strobeOn) {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);  // Bright white
        glPushMatrix();
        glTranslatef(0.0f, -0.8f, -4.5f);  // Tail position - lowered
        glutSolidSphere(0.2f, 8, 8);
        glPopMatrix();
        
        // White glow halo
        glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
        glPushMatrix();
        glTranslatef(0.0f, -0.8f, -4.5f);
        glutSolidSphere(0.5f, 8, 8);
        glPopMatrix();
    }
    
    // Steady white navigation light on nose (always on at night) - lowered
    glColor4f(1.0f, 1.0f, 0.9f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 3.0f);  // Nose of plane - lowered
    glutSolidSphere(0.15f, 8, 8);
    glPopMatrix();
    
    glPopAttrib();
}

void FlightController::loadModel(char* path) {
    // Load model for THIS instance (not static to allow multiple FlightController instances)
    if(!modelLoaded) {
        model.Load(path);
        for (int i = 0; i < model.numMaterials; i++) {
            model.Materials[i].tex.Load("models/plane/mitsubishi_a6m2_zero_texture.bmp");
            model.Materials[i].textured = true;
        }
        modelLoaded = true;
    }
}

void FlightController::renderSmoke() {
    smokeSystem.render(player.position);
}