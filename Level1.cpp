#include "Level1.h"
#include "GameManager.h"
#include <glut.h>
#include <cmath>
#include <stdio.h>
#include <cstdlib>

extern void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

// Custom BMP loader that handles both 8-bit paletted and 24-bit BMPs
static bool loadGroundTexture(GLuint* texID, const char* filename) {
    FILE* file = NULL;
    fopen_s(&file, filename, "rb");
    if (!file) {
        return false;
    }
    
    unsigned char header[138];
    memset(header, 0, sizeof(header));
    if (fread(header, 1, 54, file) != 54 || header[0] != 'B' || header[1] != 'M') {
        fclose(file);
        return false;
    }
    
    int dataOffset = *(int*)&header[10];
    int headerSize = *(int*)&header[14];
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];
    int bitsPerPixel = *(short*)&header[28];
    
    if (width <= 0 || height <= 0) {
        fclose(file);
        return false;
    }
    
    bool topDown = height < 0;
    if (topDown) height = -height;
    
    unsigned char* rgbData = new unsigned char[width * height * 3];
    
    if (bitsPerPixel == 8) {
        fseek(file, 14 + headerSize, SEEK_SET);
        unsigned char palette[1024];
        if (fread(palette, 1, 1024, file) != 1024) {
            delete[] rgbData;
            fclose(file);
            return false;
        }
        fseek(file, dataOffset, SEEK_SET);
        int rowSize = ((width + 3) / 4) * 4;
        unsigned char* indexData = new unsigned char[rowSize * height];
        if (fread(indexData, 1, rowSize * height, file) != (size_t)(rowSize * height)) {
            delete[] indexData;
            delete[] rgbData;
            fclose(file);
            return false;
        }
        for (int y = 0; y < height; y++) {
            int srcY = topDown ? y : (height - 1 - y);
            for (int x = 0; x < width; x++) {
                unsigned char index = indexData[srcY * rowSize + x];
                int destIdx = (y * width + x) * 3;
                rgbData[destIdx + 0] = palette[index * 4 + 2];
                rgbData[destIdx + 1] = palette[index * 4 + 1];
                rgbData[destIdx + 2] = palette[index * 4 + 0];
            }
        }
        delete[] indexData;
    }
    else if (bitsPerPixel == 24) {
        fseek(file, dataOffset, SEEK_SET);
        int rowSize = ((width * 3 + 3) / 4) * 4;
        unsigned char* bmpData = new unsigned char[rowSize * height];
        if (fread(bmpData, 1, rowSize * height, file) != (size_t)(rowSize * height)) {
            delete[] bmpData;
            delete[] rgbData;
            fclose(file);
            return false;
        }
        for (int y = 0; y < height; y++) {
            int srcY = topDown ? y : (height - 1 - y);
            for (int x = 0; x < width; x++) {
                int srcIdx = srcY * rowSize + x * 3;
                int destIdx = (y * width + x) * 3;
                rgbData[destIdx + 0] = bmpData[srcIdx + 2];
                rgbData[destIdx + 1] = bmpData[srcIdx + 1];
                rgbData[destIdx + 2] = bmpData[srcIdx + 0];
            }
        }
        delete[] bmpData;
    }
    else {
        delete[] rgbData;
        fclose(file);
        return false;
    }
    
    fclose(file);
    
    glGenTextures(1, texID);
    glBindTexture(GL_TEXTURE_2D, *texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgbData);
    
    delete[] rgbData;
    return true;
}

Level1::Level1() : Level(), flightSim(nullptr), screenWidth(1280), screenHeight(720),
    collectedCount(0), collectableTimer(0.0f), ringsPassedCount(0), totalRings(10),
    ringTimer(0.0f), gameTimer(0.0f), maxGameTime(600.0f), score(0),
    gameOver(false), showGameOver(false), levelComplete(false), showLevelComplete(false),
    levelCompleteTimer(0.0f), tex_water(0), tex_concrete(0),
    rocketSpawnTimer(0.0f), rocketSpawnInterval(5.0f),
    waterLevel(-2.0f), portHeight(3.0f),
    spawnProtectionTimer(3.0f), hasSpawnProtection(true) {
    
    // Aircraft carrier positioned in water - raised up
    carrierPosition = Vector3f(0.0f, 50.0f, -100.0f);  // Raised above water
    carrierRotation = 0.0f;
    carrierScale = 0.001f;  // Appropriate scale
}

Level1::~Level1() {
    cleanup();
}

void Level1::init() {
    flightSim = new FlightController();
    
    // Start plane in the air, above and ahead of the carrier - ready to fly
    // Must set AFTER constructor since reset() is called in constructor
    flightSim->player.position = Vector3f(0.0f, 60.0f, 80.0f);  // Much higher altitude, further forward
    flightSim->player.velocity = Vector3f(0, 5.0f, 50.0f);  // Strong forward + slight upward velocity
    flightSim->player.forward = Vector3f(0, 0.1f, 1).unit();  // Slightly pitched up
    flightSim->player.up = Vector3f(0, 1, 0);
    flightSim->player.right = Vector3f(1, 0, 0);
    flightSim->player.throttle = 0.7f;  // Higher throttle to maintain speed
    flightSim->isGrounded = false;  // Start in flight
    flightSim->isCrashed = false;
    
    // Spawn protection - cannot crash for first 3 seconds
    spawnProtectionTimer = 3.0f;
    hasSpawnProtection = true;
    
    loadAssets();
    particleEffects.init();
    crashSystem.init();
    soundSystem.init();
    shadowSystem.init();
    shootingSystem.init();  // Initialize shooting system
    initRings();
    initToolkits();
    initRockets();
    
    gameTimer = maxGameTime;
    score = 0;
    gameOver = false;
    showGameOver = false;
    levelComplete = false;
    showLevelComplete = false;
}

void Level1::loadAssets() {
    // Load carrier model
    model_carrier.Load("Models/carrier/carrier.3ds");
    
    // Load wrench/toolkit model
    model_wrench.Load("Models/wrench/wrench.3ds");
    
    // Load textures
    if (!loadGroundTexture(&tex_water, "textures/water.bmp")) {
        loadGroundTexture(&tex_water, "../textures/water.bmp");
    }
    
    if (!loadGroundTexture(&tex_concrete, "textures/concert.bmp")) {
        loadGroundTexture(&tex_concrete, "../textures/concert.bmp");
    }
    
    // Fallback textures if loading fails
    if (tex_water == 0) {
        glGenTextures(1, &tex_water);
        glBindTexture(GL_TEXTURE_2D, tex_water);
        unsigned char blue[3] = { 30, 80, 150 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, blue);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
    if (tex_concrete == 0) {
        glGenTextures(1, &tex_concrete);
        glBindTexture(GL_TEXTURE_2D, tex_concrete);
        unsigned char gray[3] = { 128, 128, 128 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, gray);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Initialize sky system (loads lens flare textures, cloud data, etc.)
    skySystem.init();
}

void Level1::initRings() {
    rings.clear();
    ringsPassedCount = 0;
    
    // Create a series of rings forming a path in the sky
    // Starting from carrier, going up and around
    float startX = 0.0f;
    float startY = 30.0f;
    float startZ = 50.0f;
    
    for (int i = 0; i < totalRings; i++) {
        Ring ring;
        
        // Create a winding path through the sky
        float t = (float)i / (float)(totalRings - 1);
        float angle = t * 3.14159f * 2.0f;  // Full circle path
        
        ring.position.x = startX + sin(angle) * 150.0f + (rand() % 50 - 25);
        ring.position.y = startY + t * 80.0f + sin(t * 6.28f) * 20.0f;  // Gradually higher
        ring.position.z = startZ + i * 80.0f + cos(angle) * 100.0f;
        
        ring.radius = 15.0f + (rand() % 10);  // Ring radius 15-25
        ring.passed = false;
        ring.isGolden = (i == totalRings - 1);  // Last ring is golden
        ring.rotationAngle = (float)(rand() % 360);
        
        rings.push_back(ring);
    }
}

void Level1::initToolkits() {
    toolkits.clear();
    collectedCount = 0;
    
    // Place toolkits near rings but offset
    for (int i = 0; i < 8; i++) {
        Toolkit tk;
        
        float angle = (float)i * 0.8f;
        tk.position.x = sin(angle) * 120.0f + (rand() % 60 - 30);
        tk.position.y = 40.0f + i * 15.0f + (rand() % 20);
        tk.position.z = 100.0f + i * 70.0f + (rand() % 40 - 20);
        
        tk.collected = false;
        tk.bobOffset = (float)(rand() % 100) / 100.0f * 6.28f;
        tk.rotationAngle = 0.0f;
        
        toolkits.push_back(tk);
    }
}

void Level1::initRockets() {
    rockets.clear();
    rocketSpawnTimer = 3.0f;  // First rocket after 3 seconds
}

void Level1::update(float deltaTime) {
    if (!active || !flightSim) return;
    
    // Cap deltaTime to prevent physics explosions on first frame
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    
    // Update level complete timer and transition
    if (showLevelComplete) {
        levelCompleteTimer += deltaTime;
        if (levelCompleteTimer > 3.0f) {
            // Transition to Level 2
            GameManager::getInstance().switchToLevel("FlightSimulator");
        }
        return;
    }
    
    // Update spawn protection timer
    if (hasSpawnProtection) {
        spawnProtectionTimer -= deltaTime;
        if (spawnProtectionTimer <= 0.0f) {
            hasSpawnProtection = false;
            spawnProtectionTimer = 0.0f;
        }
        // During spawn protection, reset crash state if it gets set
        if (flightSim->isCrashed) {
            flightSim->isCrashed = false;
        }
        // Also keep plane above water during spawn protection
        if (flightSim->player.position.y < 10.0f) {
            flightSim->player.position.y = 10.0f;
            flightSim->player.velocity.y = 0.0f;
        }
    }
    
    // Handle crash state (only if not spawn protected)
    if (flightSim->isCrashed && !hasSpawnProtection) {
        crashSystem.update(deltaTime);
        return;
    }
    
    // Handle game over (time out)
    if (showGameOver) {
        return;
    }
    
    if (!levelComplete && !gameOver) {
        // Update game timer
        gameTimer -= deltaTime;
        if (gameTimer <= 0.0f) {
            gameTimer = 0.0f;
            gameOver = true;
            showGameOver = true;
        }
        
        bool wasCrashedBefore = flightSim->isCrashed;
        
        flightSim->update(deltaTime);
        
        // During spawn protection, prevent any crash from sticking
        if (hasSpawnProtection && flightSim->isCrashed) {
            flightSim->isCrashed = false;
        }
        
        // Update sounds
        soundSystem.update(flightSim->isGrounded, flightSim->getSpeed());
        
        // Update sky
        skySystem.update(deltaTime);
        
        // Check for crash (only if not spawn protected)
        if (flightSim->isCrashed && !wasCrashedBefore && !hasSpawnProtection) {
            Vector3f crashPos = flightSim->player.position;
            crashPos.y = 1.0f;
            crashSystem.triggerCrash(crashPos);
            soundSystem.playCrashSound();
            return;
        }
        
        // Update particle effects
        particleEffects.update(deltaTime, flightSim->player.position,
                               flightSim->player.forward, flightSim->getSpeed());
        
        // Update shooting system
        shootingSystem.update(deltaTime);
        
        // Update game elements
        updateRings(deltaTime);
        updateToolkits(deltaTime);
        updateRockets(deltaTime);
        
        // Check collisions (rocket collisions only if not spawn protected)
        checkRingPassage();
        checkToolkitCollision();
        checkBulletRocketCollision();  // Check if bullets hit rockets
        if (!hasSpawnProtection) {
            checkRocketCollision();
        }
        
        // Check win condition - all rings passed including golden ring
        if (ringsPassedCount >= totalRings) {
            levelComplete = true;
            showLevelComplete = true;
            levelCompleteTimer = 0.0f;
            score += (int)(gameTimer * 10);  // Bonus for remaining time
        }
    }
}

void Level1::updateRings(float deltaTime) {
    ringTimer += deltaTime;
    for (auto& ring : rings) {
        ring.rotationAngle += deltaTime * 30.0f;  // Slow rotation
        if (ring.rotationAngle > 360.0f) ring.rotationAngle -= 360.0f;
    }
}

void Level1::updateToolkits(float deltaTime) {
    collectableTimer += deltaTime;
    for (auto& tk : toolkits) {
        if (!tk.collected) {
            tk.rotationAngle += deltaTime * 90.0f;
            if (tk.rotationAngle > 360.0f) tk.rotationAngle -= 360.0f;
        }
    }
}

void Level1::updateRockets(float deltaTime) {
    // Spawn new rockets periodically
    rocketSpawnTimer -= deltaTime;
    if (rocketSpawnTimer <= 0.0f && !gameOver && !levelComplete) {
        spawnRocket();
        rocketSpawnTimer = rocketSpawnInterval;
        // Make rockets more frequent as time passes
        if (rocketSpawnInterval > 2.0f) {
            rocketSpawnInterval -= 0.2f;
        }
    }
    
    // Update existing rockets
    for (auto& rocket : rockets) {
        if (rocket.active) {
            rocket.position = rocket.position + rocket.velocity * deltaTime;
            rocket.lifetime -= deltaTime;
            
            // Deactivate if too old or too far
            if (rocket.lifetime <= 0.0f) {
                rocket.active = false;
            }
        }
    }
}

void Level1::spawnRocket() {
    if (!flightSim) return;
    
    Rocket rocket;
    
    // Spawn from random direction towards player
    float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
    float distance = 150.0f + (rand() % 100);
    
    rocket.position.x = flightSim->player.position.x + cos(angle) * distance;
    rocket.position.y = flightSim->player.position.y + (rand() % 40) - 20;
    rocket.position.z = flightSim->player.position.z + sin(angle) * distance;
    
    // Aim towards player with some prediction
    Vector3f toPlayer = flightSim->player.position - rocket.position;
    toPlayer = toPlayer.unit();
    
    float speed = 40.0f + (rand() % 20);
    rocket.velocity = toPlayer * speed;
    
    rocket.active = true;
    rocket.lifetime = 8.0f;
    
    rockets.push_back(rocket);
}

void Level1::checkRingPassage() {
    if (!flightSim) return;
    
    Vector3f planePos = flightSim->player.position;
    
    for (auto& ring : rings) {
        if (ring.passed) continue;
        
        // Check if plane passed through ring
        float dx = planePos.x - ring.position.x;
        float dy = planePos.y - ring.position.y;
        float dz = planePos.z - ring.position.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        
        // Must be close enough and within ring radius
        if (distSq < ring.radius * ring.radius * 2.0f) {
            // Check if within ring plane (simple check)
            float distFromCenter = sqrt(dx * dx + dy * dy);
            if (distFromCenter < ring.radius) {
                ring.passed = true;
                ringsPassedCount++;
                score += ring.isGolden ? 500 : 100;
                soundSystem.playCoinSound();
            }
        }
    }
}

void Level1::checkToolkitCollision() {
    if (!flightSim) return;
    
    Vector3f planePos = flightSim->player.position;
    float collectRadius = 8.0f;
    
    for (auto& tk : toolkits) {
        if (tk.collected) continue;
        
        float dx = planePos.x - tk.position.x;
        float dy = planePos.y - tk.position.y;
        float dz = planePos.z - tk.position.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        
        if (distSq < collectRadius * collectRadius) {
            tk.collected = true;
            collectedCount++;
            score += 50;
            soundSystem.playCoinSound();
        }
    }
}

void Level1::checkRocketCollision() {
    if (!flightSim || flightSim->isCrashed) return;
    
    Vector3f planePos = flightSim->player.position;
    float hitRadius = 5.0f;
    
    for (auto& rocket : rockets) {
        if (!rocket.active) continue;
        
        float dx = planePos.x - rocket.position.x;
        float dy = planePos.y - rocket.position.y;
        float dz = planePos.z - rocket.position.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        
        if (distSq < hitRadius * hitRadius) {
            // Hit by rocket - crash!
            rocket.active = false;
            flightSim->isCrashed = true;
            crashSystem.triggerCrash(planePos);
            soundSystem.playCrashSound();
            gameOver = true;
            showGameOver = true;
            return;
        }
    }
}

void Level1::checkBulletRocketCollision() {
    // Check if any bullets hit any rockets - if so, explode the rocket
    float hitRadius = 8.0f;  // Collision radius for bullet-rocket
    
    // Get bullets from shooting system (need to access internal data)
    // Since ShootingSystem doesn't expose bullets directly, we'll check manually
    // by iterating over rockets and checking distance to recent bullet positions
    
    for (auto& rocket : rockets) {
        if (!rocket.active) continue;
        
        // Check against shooting system's bullets
        // The shooting system handles its own bullet lifecycle, so we spawn an explosion
        // when a rocket is hit and deactivate it
        
        // For simplicity, we use a raycast approximation based on player position and forward
        if (flightSim && shootingSystem.getActiveBulletCount() > 0) {
            // Get approximate bullet positions (bullets travel fast in forward direction)
            Vector3f bulletStart = flightSim->player.position;
            Vector3f bulletDir = flightSim->player.forward;
            
            // Check several positions along bullet path
            for (float t = 0; t < 200.0f; t += 10.0f) {
                Vector3f bulletPos = bulletStart + bulletDir * t;
                
                float dx = bulletPos.x - rocket.position.x;
                float dy = bulletPos.y - rocket.position.y;
                float dz = bulletPos.z - rocket.position.z;
                float distSq = dx * dx + dy * dy + dz * dz;
                
                if (distSq < hitRadius * hitRadius) {
                    // Bullet hit rocket - explode!
                    rocket.active = false;
                    score += 200;  // Bonus for shooting down rocket
                    // Trigger explosion at rocket position
                    crashSystem.triggerCrash(rocket.position);
                    break;
                }
            }
        }
    }
}

void Level1::renderPortLights() {
    // Only render lights at night
    if (!skySystem.isNightTime()) return;
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for glow
    
    float portX = 400.0f;
    float lightY = portHeight + 5.0f;
    
    // Pulsing effect
    float pulse = 0.7f + 0.3f * sin(ringTimer * 2.0f);
    
    // Place lights along the port edge
    for (float z = -600.0f; z <= 600.0f; z += 100.0f) {
        // Light post (simple cylinder approximation)
        glColor3f(0.3f, 0.3f, 0.35f);
        glPushMatrix();
        glTranslatef(portX + 10.0f, portHeight, z);
        glScalef(1.0f, 5.0f, 1.0f);
        glutSolidCube(2.0f);
        glPopMatrix();
        
        // Light glow (yellow/orange)
        glColor4f(1.0f, 0.9f, 0.5f, 0.8f * pulse);
        glPushMatrix();
        glTranslatef(portX + 10.0f, lightY, z);
        glutSolidSphere(2.0f, 8, 8);
        glPopMatrix();
        
        // Light halo
        glColor4f(1.0f, 0.8f, 0.4f, 0.3f * pulse);
        glPushMatrix();
        glTranslatef(portX + 10.0f, lightY, z);
        glutSolidSphere(5.0f, 8, 8);
        glPopMatrix();
    }
    
    // Also add some lights on the carrier
    glColor4f(0.8f, 0.2f, 0.2f, 0.9f * pulse);  // Red warning lights
    for (float offset = -20.0f; offset <= 20.0f; offset += 10.0f) {
        glPushMatrix();
        glTranslatef(carrierPosition.x + offset, carrierPosition.y + 8.0f, carrierPosition.z);
        glutSolidSphere(1.5f, 6, 6);
        glPopMatrix();
    }
    
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

bool Level1::isOnCarrierDeck(const Vector3f& pos) {
    // Carrier deck bounds (adjusted for new scale and position)
    // With scale 0.015, the carrier is much smaller
    float deckMinX = carrierPosition.x - 5.0f;
    float deckMaxX = carrierPosition.x + 5.0f;
    float deckMinZ = carrierPosition.z - 30.0f;
    float deckMaxZ = carrierPosition.z + 30.0f;
    float deckY = carrierPosition.y + 2.0f;  // Deck height
    
    return (pos.x >= deckMinX && pos.x <= deckMaxX &&
            pos.z >= deckMinZ && pos.z <= deckMaxZ &&
            pos.y <= deckY + 2.0f && pos.y >= deckY - 1.0f);
}

void Level1::render() {
    if (!active) return;
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    if (flightSim) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)screenWidth / (float)(screenHeight > 0 ? screenHeight : 1);
        gluPerspective(flightSim->getFOV(), aspect, 0.1, 2000.0);
    }
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    if (flightSim) {
        flightSim->setupCamera();
    }
    
    // Render sky
    if (flightSim) {
        skySystem.renderSky(flightSim->player.position);
        skySystem.renderClouds(flightSim->player.position);
    }
    
    GLfloat lightIntensity[] = { 0.7f, 0.7f, 0.7f, 1.0f };
    GLfloat lightPosition[] = { 0.0f, 100.0f, 0.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightIntensity);
    
    // Update shadow system
    DayLighting lighting = skySystem.getCurrentLighting();
    Vector3f shadowLightDir(0.3f, -0.9f, 0.2f);
    if (lighting.sunHeight > 0) {
        shadowLightDir.y = -lighting.sunHeight;
    }
    shadowSystem.setLightDirection(shadowLightDir);
    
    if (skySystem.isNightTime()) {
        shadowSystem.setShadowDarkness(0.15f);
    } else {
        shadowSystem.setShadowDarkness(0.4f);
    }
    
    // Render ground elements
    renderWater();
    renderPort();
    renderCarrier();
    
    // Render shadows
    renderShadows();
    
    // Render plane
    if (flightSim) {
        flightSim->drawPlane(skySystem.isNightTime());
    }
    
    // Render wind particles
    if (flightSim) {
        particleEffects.renderWind(flightSim->player.forward, flightSim->getSpeed());
    }
    
    // Render game elements
    renderRings();
    renderToolkits();
    renderRockets();
    
    // Render bullets and explosions from shooting system
    shootingSystem.renderBullets();
    if (flightSim) {
        shootingSystem.renderExplosions(flightSim->player.position);
    }
    
    // Render port lights (turn on at night)
    renderPortLights();
    
    // Render crash effects
    if (flightSim) {
        crashSystem.render(flightSim->player.position);
    }
    
    // Render lens flare
    if (flightSim) {
        skySystem.renderLensFlare(flightSim->player.position, flightSim->player.forward,
                                   screenWidth, screenHeight);
    }
    
    // Render HUD
    renderHUD();
    
    // Render level complete screen
    if (showLevelComplete) {
        renderLevelCompleteScreen();
    }
    
    // Render game over screen
    if (showGameOver) {
        renderGameOverScreen();
    }
    
    glutSwapBuffers();
}

void Level1::renderWater() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_water);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    float size = 2000.0f;
    float texScale = 0.003f;
    
    // Animated texture offset - slides the water texture
    float timeOffset = ringTimer * 0.05f;  // Slow sliding motion
    float waveOffset = sin(ringTimer * 0.3f) * 0.02f;  // Gentle wave motion
    
    // Multiple water layers for depth effect
    // Layer 1: Deep water (darker, slower)
    glColor4f(0.1f, 0.25f, 0.5f, 1.0f);
    glBegin(GL_QUADS);
    float offset1 = timeOffset * 0.3f;
    glTexCoord2f(offset1, offset1 + waveOffset);
    glVertex3f(-size, waterLevel - 0.5f, -size);
    glTexCoord2f(size * texScale * 2 + offset1, offset1 + waveOffset);
    glVertex3f(size, waterLevel - 0.5f, -size);
    glTexCoord2f(size * texScale * 2 + offset1, size * texScale * 2 + offset1 - waveOffset);
    glVertex3f(size, waterLevel - 0.5f, size);
    glTexCoord2f(offset1, size * texScale * 2 + offset1 - waveOffset);
    glVertex3f(-size, waterLevel - 0.5f, size);
    glEnd();
    
    // Layer 2: Main water surface (medium blue, main animation)
    glColor4f(0.15f, 0.4f, 0.7f, 0.9f);
    glBegin(GL_QUADS);
    float offset2 = timeOffset + waveOffset;
    glTexCoord2f(offset2, -offset2 * 0.5f);
    glVertex3f(-size, waterLevel, -size);
    glTexCoord2f(size * texScale * 2 + offset2, -offset2 * 0.5f);
    glVertex3f(size, waterLevel, -size);
    glTexCoord2f(size * texScale * 2 + offset2, size * texScale * 2 - offset2 * 0.5f);
    glVertex3f(size, waterLevel, size);
    glTexCoord2f(offset2, size * texScale * 2 - offset2 * 0.5f);
    glVertex3f(-size, waterLevel, size);
    glEnd();
    
    // Layer 3: Surface highlights (lighter, faster, more transparent)
    glColor4f(0.3f, 0.55f, 0.85f, 0.4f);
    glBegin(GL_QUADS);
    float offset3 = timeOffset * 1.5f - waveOffset * 2.0f;
    float texScale2 = texScale * 1.5f;  // Different scale for variety
    glTexCoord2f(-offset3, offset3 * 0.7f);
    glVertex3f(-size, waterLevel + 0.1f, -size);
    glTexCoord2f(size * texScale2 * 2 - offset3, offset3 * 0.7f);
    glVertex3f(size, waterLevel + 0.1f, -size);
    glTexCoord2f(size * texScale2 * 2 - offset3, size * texScale2 * 2 + offset3 * 0.7f);
    glVertex3f(size, waterLevel + 0.1f, size);
    glTexCoord2f(-offset3, size * texScale2 * 2 + offset3 * 0.7f);
    glVertex3f(-size, waterLevel + 0.1f, size);
    glEnd();
    
    // Foam/whitecap layer near carrier (extra detail)
    if (flightSim) {
        float px = flightSim->player.position.x;
        float pz = flightSim->player.position.z;
        float foamSize = 300.0f;
        float foamPulse = 0.3f + 0.1f * sin(ringTimer * 2.0f);
        
        glColor4f(0.7f, 0.8f, 0.9f, foamPulse);
        glBegin(GL_QUADS);
        float foamOffset = timeOffset * 2.0f;
        glTexCoord2f(foamOffset, foamOffset);
        glVertex3f(px - foamSize, waterLevel + 0.2f, pz - foamSize);
        glTexCoord2f(foamOffset + 2.0f, foamOffset);
        glVertex3f(px + foamSize, waterLevel + 0.2f, pz - foamSize);
        glTexCoord2f(foamOffset + 2.0f, foamOffset + 2.0f);
        glVertex3f(px + foamSize, waterLevel + 0.2f, pz + foamSize);
        glTexCoord2f(foamOffset, foamOffset + 2.0f);
        glVertex3f(px - foamSize, waterLevel + 0.2f, pz + foamSize);
        glEnd();
    }
    
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

void Level1::renderPort() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    glDisable(GL_LIGHTING);
    
    float size = 800.0f;
    float portX = 400.0f;  // Port is on the right side
    
    // Port/dock platform (concrete)
    glColor3f(0.6f, 0.6f, 0.65f);
    glBegin(GL_QUADS);
    float texScale = 0.01f;
    
    // Main port concrete area
    glTexCoord2f(0, 0);
    glVertex3f(portX, portHeight, -size);
    glTexCoord2f(size * texScale, 0);
    glVertex3f(portX + size, portHeight, -size);
    glTexCoord2f(size * texScale, size * texScale * 2);
    glVertex3f(portX + size, portHeight, size);
    glTexCoord2f(0, size * texScale * 2);
    glVertex3f(portX, portHeight, size);
    glEnd();
    
    // Port edge wall (between water and port)
    glColor3f(0.4f, 0.4f, 0.45f);
    glBegin(GL_QUADS);
    glVertex3f(portX, waterLevel, -size);
    glVertex3f(portX, portHeight, -size);
    glVertex3f(portX, portHeight, size);
    glVertex3f(portX, waterLevel, size);
    glEnd();
    
    glEnable(GL_LIGHTING);
}

void Level1::renderCarrier() {
    glPushMatrix();
    
    glTranslatef(carrierPosition.x, carrierPosition.y, carrierPosition.z);
    glRotatef(carrierRotation, 0, 1, 0);
    glScalef(carrierScale, carrierScale, carrierScale);
    
    // Apply gray concrete-like color to carrier
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    glColor3f(0.5f, 0.5f, 0.55f);  // Gray color
    
    // Draw the carrier model
    model_carrier.Draw();
    
    glPopMatrix();
}

void Level1::renderRings() {
    for (const auto& ring : rings) {
        renderRing(ring);
    }
}

void Level1::renderRing(const Ring& ring) {
    glPushMatrix();
    glTranslatef(ring.position.x, ring.position.y, ring.position.z);
    glRotatef(ring.rotationAngle, 0, 0, 1);  // Rotate around forward axis
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Ring color
    if (ring.passed) {
        glColor4f(0.3f, 0.3f, 0.3f, 0.5f);  // Dim gray for passed rings
    } else if (ring.isGolden) {
        // Golden ring - pulsing glow
        float pulse = 0.7f + 0.3f * sin(ringTimer * 3.0f);
        glColor4f(1.0f * pulse, 0.84f * pulse, 0.0f, 0.9f);
    } else {
        // Regular ring - cyan/blue
        glColor4f(0.0f, 0.8f, 1.0f, 0.8f);
    }
    
    // Draw ring using torus
    int segments = 32;
    int tubeSegments = 12;
    float tubeRadius = ring.isGolden ? 2.0f : 1.5f;
    
    for (int i = 0; i < segments; i++) {
        float theta1 = (float)i / segments * 2.0f * 3.14159f;
        float theta2 = (float)(i + 1) / segments * 2.0f * 3.14159f;
        
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= tubeSegments; j++) {
            float phi = (float)j / tubeSegments * 2.0f * 3.14159f;
            
            for (int k = 0; k < 2; k++) {
                float theta = (k == 0) ? theta1 : theta2;
                
                float x = (ring.radius + tubeRadius * cos(phi)) * cos(theta);
                float y = (ring.radius + tubeRadius * cos(phi)) * sin(theta);
                float z = tubeRadius * sin(phi);
                
                glVertex3f(x, y, z);
            }
        }
        glEnd();
    }
    
    // Inner glow for unpassed rings
    if (!ring.passed) {
        if (ring.isGolden) {
            glColor4f(1.0f, 0.9f, 0.3f, 0.3f);
        } else {
            glColor4f(0.0f, 0.5f, 1.0f, 0.2f);
        }
        glutSolidSphere(ring.radius * 0.8f, 16, 16);
    }
    
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void Level1::renderToolkits() {
    for (const auto& tk : toolkits) {
        if (tk.collected) continue;
        
        glPushMatrix();
        
        // Bob up and down like fuel containers
        float bob = sin(collectableTimer * 2.0f + tk.bobOffset) * 3.0f;
        glTranslatef(tk.position.x, tk.position.y + bob, tk.position.z);
        
        // Rotate around Y axis (spinning)
        float spin = tk.rotationAngle + collectableTimer * 60.0f;  // Continuous spin
        glRotatef(spin, 0, 1, 0);
        
        // Larger scale for better visibility
        glScalef(2.5f, 2.5f, 2.5f);
        
        // Add glow effect for visibility
        float glowPulse = 0.5f + 0.5f * sin(collectableTimer * 3.0f + tk.bobOffset);
        float glowColor[] = { 1.0f * glowPulse, 0.8f * glowPulse, 0.2f * glowPulse, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, glowColor);
        
        model_wrench.Draw();
        
        // Reset emission
        float noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
        
        glPopMatrix();
    }
}

void Level1::renderRockets() {
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    
    for (const auto& rocket : rockets) {
        if (!rocket.active) continue;
        
        glPushMatrix();
        glTranslatef(rocket.position.x, rocket.position.y, rocket.position.z);
        
        // Point rocket in direction of travel
        Vector3f dir = rocket.velocity.unit();
        float yaw = atan2(dir.x, dir.z) * 180.0f / 3.14159f;
        float pitch = asin(-dir.y) * 180.0f / 3.14159f;
        
        glRotatef(yaw, 0, 1, 0);
        glRotatef(pitch, 1, 0, 0);
        
        // Rocket body (red/orange)
        glColor3f(0.8f, 0.2f, 0.1f);
        glPushMatrix();
        glScalef(0.5f, 0.5f, 2.0f);
        glutSolidCube(2.0f);
        glPopMatrix();
        
        // Rocket nose cone
        glColor3f(0.3f, 0.3f, 0.3f);
        glPushMatrix();
        glTranslatef(0, 0, 2.0f);
        glRotatef(-90, 1, 0, 0);
        glutSolidCone(0.5f, 1.5f, 8, 4);
        glPopMatrix();
        
        // Rocket exhaust flame
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glColor4f(1.0f, 0.6f, 0.1f, 0.8f);
        glPushMatrix();
        glTranslatef(0, 0, -2.5f);
        glRotatef(90, 1, 0, 0);
        glutSolidCone(0.6f, 2.0f, 8, 4);
        glPopMatrix();
        glDisable(GL_BLEND);
        
        glPopMatrix();
    }
    
    glEnable(GL_LIGHTING);
}

void Level1::renderShadows() {
    if (!flightSim) return;
    
    // Simple blob shadow under plane
    Vector3f planePos = flightSim->player.position;
    float shadowY = 0.1f;  // Just above ground/water
    
    if (planePos.y < 100.0f) {
        float shadowScale = 1.0f - (planePos.y / 100.0f) * 0.5f;
        float shadowAlpha = 0.3f * shadowScale;
        
        glPushMatrix();
        glTranslatef(planePos.x, shadowY, planePos.z);
        glScalef(4.0f * shadowScale, 0.1f, 6.0f * shadowScale);
        
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0, 0, 0, shadowAlpha);
        
        glutSolidSphere(1.0f, 12, 6);
        
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
}

void Level1::renderHUD() {
    // Save current matrices and states
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // Switch to orthographic projection for 2D HUD
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, screenWidth, 0, screenHeight);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Disable lighting and depth test for HUD
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    
    char buffer[64];
    
    // Draw semi-transparent background for Rings counter (top left)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(10, screenHeight - 10);
    glVertex2f(200, screenHeight - 10);
    glVertex2f(200, screenHeight - 70);
    glVertex2f(10, screenHeight - 70);
    glEnd();
    
    // Draw ring icon (simple circle)
    glColor4f(0.0f, 0.8f, 1.0f, 1.0f);  // Cyan color
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 16; i++) {
        float angle = (float)i / 16.0f * 6.28318f;
        glVertex2f(30 + cos(angle) * 10, screenHeight - 30 + sin(angle) * 10);
    }
    glEnd();
    
    // Draw text for ring count
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(50, screenHeight - 35);
    sprintf_s(buffer, sizeof(buffer), "Rings: %d / %d", ringsPassedCount, totalRings);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Draw toolkit count
    glColor3f(1.0f, 0.8f, 0.2f);  // Gold color
    glRasterPos2f(50, screenHeight - 55);
    sprintf_s(buffer, sizeof(buffer), "Toolkits: %d", collectedCount);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Draw Timer (top center)
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    float timerBoxX = (screenWidth - 150) / 2.0f;
    glBegin(GL_QUADS);
    glVertex2f(timerBoxX, screenHeight - 10);
    glVertex2f(timerBoxX + 150, screenHeight - 10);
    glVertex2f(timerBoxX + 150, screenHeight - 50);
    glVertex2f(timerBoxX, screenHeight - 50);
    glEnd();
    
    // Timer color changes as time runs low
    if (gameTimer > 30.0f) {
        glColor3f(1.0f, 1.0f, 1.0f);  // White
    } else if (gameTimer > 10.0f) {
        glColor3f(1.0f, 1.0f, 0.0f);  // Yellow
    } else {
        glColor3f(1.0f, 0.3f, 0.3f);  // Red
    }
    
    int minutes = (int)gameTimer / 60;
    int seconds = (int)gameTimer % 60;
    sprintf_s(buffer, sizeof(buffer), "Time: %d:%02d", minutes, seconds);
    glRasterPos2f(timerBoxX + 25, screenHeight - 35);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Draw Score (top right)
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(screenWidth - 180, screenHeight - 10);
    glVertex2f(screenWidth - 10, screenHeight - 10);
    glVertex2f(screenWidth - 10, screenHeight - 50);
    glVertex2f(screenWidth - 180, screenHeight - 50);
    glEnd();
    
    glColor3f(1.0f, 0.9f, 0.2f);  // Gold color
    sprintf_s(buffer, sizeof(buffer), "Score: %d", score);
    glRasterPos2f(screenWidth - 170, screenHeight - 35);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Draw Speed indicator (bottom left)
    if (flightSim) {
        float speed = flightSim->getSpeed();
        
        // Background box
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glBegin(GL_QUADS);
        glVertex2f(10, 60);
        glVertex2f(180, 60);
        glVertex2f(180, 10);
        glVertex2f(10, 10);
        glEnd();
        
        // Speed color changes based on value
        if (speed < 30.0f) {
            glColor3f(1.0f, 0.3f, 0.3f);  // Red (slow/stalling)
        } else if (speed < 50.0f) {
            glColor3f(1.0f, 1.0f, 0.0f);  // Yellow (takeoff speed)
        } else if (speed < 80.0f) {
            glColor3f(0.0f, 1.0f, 0.0f);  // Green (good speed)
        } else {
            glColor3f(0.0f, 0.8f, 1.0f);  // Cyan (high speed)
        }
        
        sprintf_s(buffer, sizeof(buffer), "Speed: %.0f", speed);
        glRasterPos2f(20, 35);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
        
        // Draw speed bar
        float speedBarWidth = 150.0f;
        float speedBarFill = (speed / 120.0f) * speedBarWidth;  // Max speed 120 for display
        if (speedBarFill > speedBarWidth) speedBarFill = speedBarWidth;
        
        // Speed bar background
        glColor4f(0.3f, 0.3f, 0.3f, 0.8f);
        glBegin(GL_QUADS);
        glVertex2f(15, 28);
        glVertex2f(15 + speedBarWidth, 28);
        glVertex2f(15 + speedBarWidth, 22);
        glVertex2f(15, 22);
        glEnd();
        
        // Speed bar fill (same color as text)
        if (speed < 30.0f) {
            glColor4f(1.0f, 0.3f, 0.3f, 0.9f);
        } else if (speed < 50.0f) {
            glColor4f(1.0f, 1.0f, 0.0f, 0.9f);
        } else if (speed < 80.0f) {
            glColor4f(0.0f, 1.0f, 0.0f, 0.9f);
        } else {
            glColor4f(0.0f, 0.8f, 1.0f, 0.9f);
        }
        
        glBegin(GL_QUADS);
        glVertex2f(15, 28);
        glVertex2f(15 + speedBarFill, 28);
        glVertex2f(15 + speedBarFill, 22);
        glVertex2f(15, 22);
        glEnd();
        
        // Draw takeoff speed marker (50.0 units)
        float takeoffMarkerX = 15 + (50.0f / 120.0f) * speedBarWidth;
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINES);
        glVertex2f(takeoffMarkerX, 28);
        glVertex2f(takeoffMarkerX, 19);
        glEnd();
    }
    
    // Draw Attitude Indicator (Pitch, Roll, Yaw) - Bottom Right
    if (flightSim) {
        float centerX = screenWidth - 120.0f;
        float centerY = 120.0f;
        float radius = 80.0f;
        
        // Background box
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glBegin(GL_QUADS);
        glVertex2f(screenWidth - 230, 220);
        glVertex2f(screenWidth - 10, 220);
        glVertex2f(screenWidth - 10, 20);
        glVertex2f(screenWidth - 230, 20);
        glEnd();
        
        // Calculate pitch, roll, yaw from plane's orientation vectors
        Vector3f forward = flightSim->player.forward;
        Vector3f up = flightSim->player.up;
        Vector3f right = flightSim->player.right;
        
        // Pitch: angle between forward vector and horizontal plane (in degrees)
        float pitch = asin(forward.y) * 180.0f / 3.14159f;
        
        // Roll: angle of tilt left/right (in degrees)
        float roll = atan2(right.y, up.y) * 180.0f / 3.14159f;
        
        // Yaw: compass heading (in degrees) - angle in XZ plane
        float yaw = atan2(forward.x, forward.z) * 180.0f / 3.14159f;
        if (yaw < 0) yaw += 360.0f;
        
        // Draw artificial horizon (Roll and Pitch indicator)
        glPushMatrix();
        glTranslatef(centerX, centerY, 0);
        
        // Draw sky/ground horizon
        glRotatef(-roll, 0, 0, 1);  // Rotate by roll angle
        
        // Sky (upper half)
        glColor4f(0.3f, 0.5f, 0.8f, 0.7f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(0, 0);
        for (int i = 0; i <= 180; i += 10) {
            float angle = (float)i * 3.14159f / 180.0f;
            glVertex2f(cos(angle) * radius, sin(angle) * radius + pitch * 1.5f);
        }
        glEnd();
        
        // Ground (lower half)
        glColor4f(0.4f, 0.3f, 0.2f, 0.7f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(0, 0);
        for (int i = 180; i <= 360; i += 10) {
            float angle = (float)i * 3.14159f / 180.0f;
            glVertex2f(cos(angle) * radius, sin(angle) * radius + pitch * 1.5f);
        }
        glEnd();
        
        // Horizon line
        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(-radius, pitch * 1.5f);
        glVertex2f(radius, pitch * 1.5f);
        glEnd();
        glLineWidth(1.0f);
        
        // Pitch ladder marks (every 10 degrees)
        glColor3f(1.0f, 1.0f, 1.0f);
        for (int p = -30; p <= 30; p += 10) {
            if (p == 0) continue;  // Skip zero (horizon)
            float yOffset = (pitch - p) * 1.5f;
            if (fabs(yOffset) < radius) {
                glBegin(GL_LINES);
                glVertex2f(-20, yOffset);
                glVertex2f(20, yOffset);
                glEnd();
            }
        }
        
        glPopMatrix();
        
        // Center reference mark (fixed airplane symbol)
        glColor3f(1.0f, 1.0f, 0.0f);
        glLineWidth(3.0f);
        glBegin(GL_LINES);
        // Left wing
        glVertex2f(centerX - 40, centerY);
        glVertex2f(centerX - 10, centerY);
        // Right wing
        glVertex2f(centerX + 10, centerY);
        glVertex2f(centerX + 40, centerY);
        // Center dot
        glVertex2f(centerX - 2, centerY);
        glVertex2f(centerX + 2, centerY);
        glEnd();
        glLineWidth(1.0f);
        
        // Outer circle border
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 360; i += 5) {
            float angle = (float)i * 3.14159f / 180.0f;
            glVertex2f(centerX + cos(angle) * radius, centerY + sin(angle) * radius);
        }
        glEnd();
        
        // Roll indicator triangle at top
        glPushMatrix();
        glTranslatef(centerX, centerY, 0);
        glRotatef(-roll, 0, 0, 1);
        glColor3f(1.0f, 0.0f, 0.0f);
        glBegin(GL_TRIANGLES);
        glVertex2f(0, radius + 5);
        glVertex2f(-5, radius - 5);
        glVertex2f(5, radius - 5);
        glEnd();
        glPopMatrix();
        
        // Display numerical values
        glColor3f(1.0f, 1.0f, 1.0f);
        
        // Pitch value
        sprintf_s(buffer, sizeof(buffer), "Pitch: %.0f", pitch);
        glRasterPos2f(screenWidth - 220, 200);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
        
        // Roll value
        sprintf_s(buffer, sizeof(buffer), "Roll: %.0f", roll);
        glRasterPos2f(screenWidth - 220, 185);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
        
        // Yaw (heading) value
        sprintf_s(buffer, sizeof(buffer), "Heading: %.0f", yaw);
        glRasterPos2f(screenWidth - 220, 30);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
    }
    
    // Draw altitude warning if flying too high
    if (flightSim && flightSim->player.position.y > 150.0f) {
        glColor4f(1.0f, 0.0f, 0.0f, 0.7f + 0.3f * sin(ringTimer * 5.0f));
        sprintf_s(buffer, sizeof(buffer), "WARNING: Fly Lower!");
        float warnX = (screenWidth - 180) / 2.0f;
        glRasterPos2f(warnX, screenHeight - 80);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
    }
    
    // Draw spawn protection indicator
    if (hasSpawnProtection) {
        // Flashing cyan "PROTECTED" text
        float alpha = 0.5f + 0.5f * sin(ringTimer * 8.0f);
        glColor4f(0.0f, 1.0f, 1.0f, alpha);
        sprintf_s(buffer, sizeof(buffer), "SPAWN PROTECTION: %.1fs", spawnProtectionTimer);
        float protX = (screenWidth - 200) / 2.0f;
        glRasterPos2f(protX, screenHeight - 100);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
    }
    
    // Restore matrices and states
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glPopAttrib();
}

void Level1::renderGameOverScreen() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, screenWidth, 0, screenHeight);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Dark overlay
    glColor4f(0, 0, 0, 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(screenWidth, 0);
    glVertex2f(screenWidth, screenHeight);
    glVertex2f(0, screenHeight);
    glEnd();
    
    // Game Over text
    glColor3f(1.0f, 0.2f, 0.2f);
    const char* gameOverText = "GAME OVER";
    glRasterPos2f(screenWidth / 2 - 60, screenHeight / 2 + 30);
    for (const char* c = gameOverText; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);
    }
    
    // Reason
    glColor3f(0.8f, 0.8f, 0.8f);
    const char* reason = flightSim->isCrashed ? "Hit by rocket!" : "Time ran out!";
    glRasterPos2f(screenWidth / 2 - 50, screenHeight / 2);
    for (const char* c = reason; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Score
    char scoreText[64];
    sprintf_s(scoreText, "Final Score: %d", score);
    glRasterPos2f(screenWidth / 2 - 60, screenHeight / 2 - 30);
    for (char* c = scoreText; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Restart prompt
    glColor3f(0.6f, 0.8f, 1.0f);
    const char* restartText = "Press R to restart";
    glRasterPos2f(screenWidth / 2 - 70, screenHeight / 2 - 70);
    for (const char* c = restartText; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Level1::renderLevelCompleteScreen() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, screenWidth, 0, screenHeight);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Semi-transparent overlay
    glColor4f(0, 0.1f, 0.2f, 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(screenWidth, 0);
    glVertex2f(screenWidth, screenHeight);
    glVertex2f(0, screenHeight);
    glEnd();
    
    // Level Complete text
    glColor3f(0.2f, 1.0f, 0.4f);
    const char* completeText = "LEVEL COMPLETE!";
    glRasterPos2f(screenWidth / 2 - 80, screenHeight / 2 + 40);
    for (const char* c = completeText; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);
    }
    
    // Score
    glColor3f(1.0f, 0.9f, 0.3f);
    char scoreText[64];
    sprintf_s(scoreText, "Score: %d", score);
    glRasterPos2f(screenWidth / 2 - 50, screenHeight / 2);
    for (char* c = scoreText; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Next level prompt
    glColor3f(0.8f, 0.8f, 1.0f);
    const char* nextText = "Proceeding to Level 2...";
    glRasterPos2f(screenWidth / 2 - 80, screenHeight / 2 - 40);
    for (const char* c = nextText; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Level1::handleKeyboard(unsigned char key, bool pressed) {
    if (!active) return;
    if (key == 27) exit(0);  // ESC
    
    // Reset key
    if ((key == 'r' || key == 'R') && pressed) {
        crashSystem.reset();
        soundSystem.reset();
        gameTimer = maxGameTime;
        score = 0;
        gameOver = false;
        showGameOver = false;
        levelComplete = false;
        showLevelComplete = false;
        rocketSpawnTimer = 3.0f;
        rocketSpawnInterval = 5.0f;
        
        // Reinitialize game elements
        initRings();
        initToolkits();
        initRockets();
        shootingSystem.reset();  // Reset shooting system
        
        // Reset plane - start in the air
        if (flightSim) {
            flightSim->player.position = Vector3f(0.0f, 40.0f, 50.0f);
            flightSim->player.velocity = Vector3f(0, 0, 30.0f);
            flightSim->player.forward = Vector3f(0, 0, 1);
            flightSim->player.up = Vector3f(0, 1, 0);
            flightSim->player.right = Vector3f(1, 0, 0);
            flightSim->player.throttle = 0.5f;
            flightSim->isGrounded = false;
            flightSim->isCrashed = false;
        }
    }
    
    // Shooting - Space key to fire
    if (key == ' ' && pressed && flightSim && !gameOver && !levelComplete) {
        shootingSystem.fire(flightSim->player.position, flightSim->player.forward);
    }
    
    if (flightSim) flightSim->handleInput(key, pressed);
}

void Level1::handleKeyboardUp(unsigned char key) {
    if (flightSim) flightSim->handleInput(key, false);
}

void Level1::handleMouse(int x, int y) {
    if (!active || !flightSim) return;
    screenWidth = glutGet(GLUT_WINDOW_WIDTH);
    screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    int centerX = screenWidth / 2;
    int centerY = screenHeight / 2;
    flightSim->handleMouse(x, y, centerX, centerY);
    if (screenWidth > 0 && screenHeight > 0) glutWarpPointer(centerX, centerY);
}

void Level1::onEnter() {
    active = true;
    screenWidth = glutGet(GLUT_WINDOW_WIDTH);
    screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    glutSetCursor(GLUT_CURSOR_NONE);
    // Reset spawn protection when entering/re-entering level
    spawnProtectionTimer = 3.0f;
    hasSpawnProtection = true;
    if (flightSim) {
        flightSim->isCrashed = false;
    }
}

void Level1::onExit() {
    active = false;
    glutSetCursor(GLUT_CURSOR_INHERIT);
}

void Level1::cleanup() {
    if (flightSim) {
        delete flightSim;
        flightSim = nullptr;
    }
    
    if (tex_water) {
        glDeleteTextures(1, &tex_water);
        tex_water = 0;
    }
    if (tex_concrete) {
        glDeleteTextures(1, &tex_concrete);
        tex_concrete = 0;
    }
    
    // Systems are cleaned up by their destructors
}

void Level1::renderGround() {
    // Ground rendering is handled by renderWater and renderPort
}
