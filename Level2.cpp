#include "Level2.h"
#include <glut.h>
#include <cmath>
#include <stdio.h>

extern void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

// Custom BMP loader that handles both 8-bit paletted and 24-bit BMPs
static bool loadGroundTexture(GLuint* texID, const char* filename) {
    FILE* file = NULL;
    fopen_s(&file, filename, "rb");
    if (!file) {
        return false;
    }
    
    // Read BMP header (54 bytes minimum, but BITMAPV4/V5 can be larger)
    unsigned char header[138];  // Large enough for BITMAPV4HEADER
    memset(header, 0, sizeof(header));
    if (fread(header, 1, 54, file) != 54 || header[0] != 'B' || header[1] != 'M') {
        fclose(file);
        return false;
    }
    
    // Extract info
    int dataOffset = *(int*)&header[10];
    int headerSize = *(int*)&header[14];  // DIB header size
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];
    int bitsPerPixel = *(short*)&header[28];
    int compression = *(int*)&header[30];
    
    if (width <= 0 || height <= 0) {
        fclose(file);
        return false;
    }
    
    // Handle negative height (top-down bitmap)
    bool topDown = height < 0;
    if (topDown) height = -height;
    
    unsigned char* rgbData = new unsigned char[width * height * 3];
    
    if (bitsPerPixel == 8) {
        // 8-bit paletted BMP
        // Read the color palette (located right after the DIB header)
        fseek(file, 14 + headerSize, SEEK_SET);  // 14 = BMP file header size
        
        unsigned char palette[1024];  // 256 colors * 4 bytes (BGRA)
        if (fread(palette, 1, 1024, file) != 1024) {
            delete[] rgbData;
            fclose(file);
            return false;
        }
        
        // Seek to pixel data
        fseek(file, dataOffset, SEEK_SET);
        
        // Calculate row size with padding (rows are 4-byte aligned)
        int rowSize = ((width + 3) / 4) * 4;
        unsigned char* indexData = new unsigned char[rowSize * height];
        
        if (fread(indexData, 1, rowSize * height, file) != (size_t)(rowSize * height)) {
            delete[] indexData;
            delete[] rgbData;
            fclose(file);
            return false;
        }
        
        // Convert indexed to RGB using palette
        for (int y = 0; y < height; y++) {
            int srcY = topDown ? y : (height - 1 - y);
            for (int x = 0; x < width; x++) {
                unsigned char index = indexData[srcY * rowSize + x];
                int destIdx = (y * width + x) * 3;
                rgbData[destIdx + 0] = palette[index * 4 + 2];  // R (palette is BGRA)
                rgbData[destIdx + 1] = palette[index * 4 + 1];  // G
                rgbData[destIdx + 2] = palette[index * 4 + 0];  // B
            }
        }
        delete[] indexData;
    }
    else if (bitsPerPixel == 24) {
        // 24-bit RGB BMP
        fseek(file, dataOffset, SEEK_SET);
        
        int rowSize = ((width * 3 + 3) / 4) * 4;
        unsigned char* bmpData = new unsigned char[rowSize * height];
        
        if (fread(bmpData, 1, rowSize * height, file) != (size_t)(rowSize * height)) {
            delete[] bmpData;
            delete[] rgbData;
            fclose(file);
            return false;
        }
        
        // Convert BGR to RGB
        for (int y = 0; y < height; y++) {
            int srcY = topDown ? y : (height - 1 - y);
            for (int x = 0; x < width; x++) {
                int srcIdx = srcY * rowSize + x * 3;
                int destIdx = (y * width + x) * 3;
                rgbData[destIdx + 0] = bmpData[srcIdx + 2];  // R
                rgbData[destIdx + 1] = bmpData[srcIdx + 1];  // G
                rgbData[destIdx + 2] = bmpData[srcIdx + 0];  // B
            }
        }
        delete[] bmpData;
    }
    else {
        // Unsupported format
        delete[] rgbData;
        fclose(file);
        return false;
    }
    
    fclose(file);
    
    // Create OpenGL texture
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

Level2::Level2() : Level(), flightSim(nullptr), screenWidth(1280), screenHeight(720), 
    collectedCount(0), collectableTimer(0.0f), arrowBobOffset(0.0f), 
    hasLanded(false), showWinMessage(false), winMessageTimer(0.0f),
    gameTimer(0.0f), maxGameTime(120.0f), score(0), landingBonus(0),
    gameOver(false), showGameOver(false) {
    airportPosition = Vector3f(0, 0, 0);
    airportRotation = 0.0f;
    airportScale = 1.0f;
}

Level2::~Level2() {
    cleanup();
}

void Level2::init() {
    flightSim = new FlightController();
    loadAssets();
    initWindParticles(); // Initialize wind
    initFuelContainers(); // Initialize fuel collectables
    initBuildings();      // Initialize building obstacles
    initAirport();        // Initialize airport landing target
    
    // Initialize game timer and score
    gameTimer = maxGameTime;  // 120 seconds countdown
    score = 0;
    gameOver = false;
    showGameOver = false;
}

// --- WIND PARTICLE SYSTEM ---
void Level2::initWindParticles() {
    windParticles.resize(50); // 50 wind strips
    for (auto& p : windParticles) {
        p.active = false;
    }
}

void Level2::updateWindParticles(float deltaTime) {
    if (!flightSim) return;
    
    float speed = flightSim->getSpeed();
    Vector3f camPos = flightSim->player.position;
    Vector3f forward = flightSim->player.forward;
    
    // Only show wind if moving fast
    if (speed < 30.0f) return;

    for (auto& p : windParticles) {
        if (!p.active) {
            // Respawn chance based on speed
            if (rand() % 100 < (speed / 5.0f)) {
                p.active = true;
                // Spawn ahead of player in a random radius
                float range = 15.0f;
                float rX = ((rand() % 100) / 50.0f - 1.0f) * range;
                float rY = ((rand() % 100) / 50.0f - 1.0f) * range;
                float rZ = ((rand() % 100) / 50.0f - 1.0f) * range;
                
                p.position = camPos + (forward * 40.0f) + Vector3f(rX, rY, rZ);
                p.length = 2.0f + (speed / 20.0f); // Longer strips at higher speed
                p.life = 1.0f;
            }
        } else {
            // Move particle opposite to flight direction
            p.position = p.position - (forward * (speed * 1.5f * deltaTime));
            p.life -= deltaTime;
            
            // Deactivate if behind camera or dead
            Vector3f toParticle = p.position - camPos;
            float distSq = toParticle.x*toParticle.x + toParticle.y*toParticle.y + toParticle.z*toParticle.z;
            
            if (p.life <= 0 || (toParticle.x * forward.x + toParticle.y * forward.y + toParticle.z * forward.z) < -5.0f) {
                p.active = false;
            }
        }
    }
}

void Level2::renderWindParticles() {
    if (!flightSim || flightSim->getSpeed() < 30.0f) return;

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    
    Vector3f forward = flightSim->player.forward;
    
    for (const auto& p : windParticles) {
        if (p.active) {
            // Fade in/out based on life
            float alpha = (flightSim->getSpeed() / 100.0f) * 0.5f; 
            if (alpha > 0.5f) alpha = 0.5f;
            
            glColor4f(1.0f, 1.0f, 1.0f, 0.0f); // Tail (Transparent)
            Vector3f tail = p.position + (forward * p.length);
            glVertex3f(tail.x, tail.y, tail.z);
            
            glColor4f(1.0f, 1.0f, 1.0f, alpha); // Head (Visible)
            glVertex3f(p.position.x, p.position.y, p.position.z);
        }
    }
    
    glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}
// ----------------------------

void Level2::loadAssets() {
    model_house.Load("Models/house/house.3DS");
    model_tree.Load("Models/tree/Tree1.3ds");
    model_fuelContainer.Load("Models/fuel container/Container Gas  N250815.3DS");
    
    // Load building models
    model_buildings[0].Load("Models/buildings/Residential Buildings 001.3ds");
    model_buildings[1].Load("Models/buildings/Residential Buildings 002.3ds");
    model_buildings[2].Load("Models/buildings/Residential Buildings 003.3ds");
    model_buildings[3].Load("Models/buildings/Residential Buildings 004.3ds");
    model_buildings[4].Load("Models/buildings/Residential Buildings 005.3ds");
    model_buildings[5].Load("Models/buildings/Residential Buildings 006.3ds");
    model_buildings[6].Load("Models/buildings/Residential Buildings 007.3ds");
    model_buildings[7].Load("Models/buildings/Residential Buildings 008.3ds");
    model_buildings[8].Load("Models/buildings/Residential Buildings 009.3ds");
    model_buildings[9].Load("Models/buildings/Residential Buildings 010.3ds");
    
    // Load airport model
    model_airport.Load("Models/newairport/Airport Model.3ds");
    
    // Try to load ground texture using custom loader
    if (!loadGroundTexture(&tex_ground.texture[0], "../textures/grassGround.bmp")) {
        // Fallback: try without ../ in case working dir is project root
        if (!loadGroundTexture(&tex_ground.texture[0], "textures/grassGround.bmp")) {
            // Last resort: create a green fallback texture
            tex_ground.BuildColorTexture(60, 120, 60);
        }
    }
    
    skySystem.init();  // Initialize sky and lens flare system
}

void Level2::update(float deltaTime) {
    if (!active || !flightSim) return;
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    
    // Update arrow animation
    arrowBobOffset += deltaTime * 3.0f;
    
    // Update win message timer
    if (showWinMessage) {
        winMessageTimer += deltaTime;
    }
    
    // Update game over display
    if (showGameOver) {
        return;  // Stop updating when game over
    }
    
    if (!hasLanded && !gameOver) {
        // Update game timer
        gameTimer -= deltaTime;
        if (gameTimer <= 0.0f) {
            gameTimer = 0.0f;
            gameOver = true;
            showGameOver = true;
        }
        
        flightSim->update(deltaTime);
        updateWindParticles(deltaTime); // Update wind
        updateFuelContainers(deltaTime); // Update fuel containers
        checkFuelCollision(); // Check for collection
        checkBuildingCollision(); // Check for building crashes
        checkLandingCondition(); // Check if player landed at airport
    }
}

void Level2::render() {
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
    
    // Render sky using shared SkySystem
    if (flightSim) {
        skySystem.renderSky(flightSim->player.position);
    }
    
    GLfloat lightIntensity[] = { 0.7f, 0.7f, 0.7f, 1.0f };
    GLfloat lightPosition[] = { 0.0f, 100.0f, 0.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightIntensity);
    
    renderGround();
    
    if (flightSim) {
        flightSim->drawPlane();
    }
    
    // Render Wind Strips
    renderWindParticles();
    
    // Render Fuel Containers
    renderFuelContainers();
    
    // Render Buildings
    renderBuildings();
    
    // Render Airport and Target Arrow
    renderAirport();
    renderTargetArrow();
    
    glPushMatrix();
    glTranslatef(10, 0, 0);
    glScalef(0.7f, 0.7f, 0.7f);
    model_tree.Draw();
    glPopMatrix();
    
    glPushMatrix();
    glRotatef(90.f, 1, 0, 0);
    model_house.Draw();
    glPopMatrix();
    
    // Render Smoke Particles (handled by FlightController)
    if (flightSim) {
        flightSim->renderSmoke();
    }
    
    // Render Lens Flare using shared SkySystem
    if (flightSim) {
        skySystem.renderLensFlare(flightSim->player.position, flightSim->player.forward, 
                                   screenWidth, screenHeight);
    }
    
    // Render HUD (fuel count, timer, score)
    renderHUD();
    
    // Render win screen if landed
    if (showWinMessage) {
        renderWinScreen();
    }
    
    // Render game over screen if time ran out
    if (showGameOver) {
        renderGameOverScreen();
    }
    
    glutSwapBuffers();
}

void Level2::renderGround() {
    glDisable(GL_LIGHTING);
    
    float px = 0, pz = 0;
    if (flightSim) {
        px = flightSim->player.position.x;
        pz = flightSim->player.position.z;
    }
    
    float size = 2000.0f;
    float texScale = 0.02f;  // Controls texture density (smaller = more repetitions)
    
    glPushMatrix();
    glTranslatef(px, 0, pz);  // Ground follows player
    
    // Ensure proper texture state
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    
    // Check if texture loaded - use green fallback if not
    if (tex_ground.texture[0] != 0) {
        glBindTexture(GL_TEXTURE_2D, tex_ground.texture[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor3f(1.0f, 1.0f, 1.0f);
    } else {
        // Fallback to green if texture failed to load
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.2f, 0.5f, 0.2f);
    }
    
    // Use world coordinates for texture mapping so texture stays fixed in world space
    // This prevents the texture from sliding as the camera moves
    float worldMinX = px - size;
    float worldMaxX = px + size;
    float worldMinZ = pz - size;
    float worldMaxZ = pz + size;
    
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(worldMinX * texScale, worldMinZ * texScale); glVertex3f(-size, 0, -size);
    glTexCoord2f(worldMaxX * texScale, worldMinZ * texScale); glVertex3f(size, 0, -size);
    glTexCoord2f(worldMaxX * texScale, worldMaxZ * texScale); glVertex3f(size, 0, size);
    glTexCoord2f(worldMinX * texScale, worldMaxZ * texScale); glVertex3f(-size, 0, size);
    glEnd();
    
    glPopMatrix();
    glEnable(GL_LIGHTING);
    glColor3f(1, 1, 1);
}

void Level2::handleKeyboard(unsigned char key, bool pressed) {
    if (!active) return;
    if (key == 27) exit(0);
    if (flightSim) flightSim->handleInput(key, pressed);
}

void Level2::handleKeyboardUp(unsigned char key) {
    if (!active || !flightSim) return;
    flightSim->handleInput(key, false);
}

void Level2::handleMouse(int x, int y) {
    if (!active || !flightSim) return;
    int centerX = screenWidth / 2;
    int centerY = screenHeight / 2;
    flightSim->handleMouse(x, y, centerX, centerY);
    if (screenWidth > 0 && screenHeight > 0) glutWarpPointer(centerX, centerY);
}

void Level2::onEnter() {
    Level::onEnter();
    screenWidth = glutGet(GLUT_WINDOW_WIDTH);
    screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    glutSetCursor(GLUT_CURSOR_NONE);
}

void Level2::onExit() {
    Level::onExit();
    glutSetCursor(GLUT_CURSOR_INHERIT);
}

void Level2::cleanup() {
    if (flightSim) {
        delete flightSim;
        flightSim = nullptr;
    }
}

// ============ FUEL CONTAINER FUNCTIONS ============

void Level2::initFuelContainers() {
    fuelContainers.clear();
    collectedCount = 0;
    
    // Spawn fuel containers at various locations around the map
    const int numContainers = 15;
    float spawnRadius = 300.0f;
    
    for (int i = 0; i < numContainers; i++) {
        FuelCollectable container;
        // Distribute containers in a circular pattern
        float angle = (float)i / numContainers * 2.0f * 3.14159f;
        float distance = 50.0f + (rand() % (int)spawnRadius);
        container.position.x = cos(angle) * distance;
        container.position.y = 20.0f + (rand() % 80);  // Random altitude between 20-100
        container.position.z = sin(angle) * distance;
        container.collected = false;
        container.bobOffset = (float)(rand() % 628) / 100.0f;  // Random starting phase
        container.rotationAngle = (float)(rand() % 360);
        container.glowIntensity = 1.0f;
        fuelContainers.push_back(container);
    }
}

void Level2::updateFuelContainers(float deltaTime) {
    collectableTimer += deltaTime;
    
    for (size_t i = 0; i < fuelContainers.size(); i++) {
        if (fuelContainers[i].collected) continue;
        
        // Rotate the container
        fuelContainers[i].rotationAngle += 90.0f * deltaTime;  // 90 degrees per second
        if (fuelContainers[i].rotationAngle >= 360.0f) {
            fuelContainers[i].rotationAngle -= 360.0f;
        }
        
        // Update bob offset for up/down motion
        fuelContainers[i].bobOffset += 2.0f * deltaTime;
        if (fuelContainers[i].bobOffset >= 6.28318f) {
            fuelContainers[i].bobOffset -= 6.28318f;
        }
        
        // Pulsing glow effect
        fuelContainers[i].glowIntensity = 0.7f + 0.3f * sin(collectableTimer * 3.0f + fuelContainers[i].bobOffset);
    }
}

void Level2::renderFuelContainers() {
    for (size_t i = 0; i < fuelContainers.size(); i++) {
        if (fuelContainers[i].collected) continue;
        
        FuelCollectable& fc = fuelContainers[i];
        
        // Calculate bobbing offset (up/down motion)
        float bobHeight = sin(fc.bobOffset) * 3.0f;  // 3 units up/down
        
        glPushMatrix();
        
        // Position the container
        glTranslatef(fc.position.x, fc.position.y + bobHeight, fc.position.z);
        
        // Rotate around Y axis
        glRotatef(fc.rotationAngle, 0.0f, 1.0f, 0.0f);
        
        // Scale the model appropriately (smaller size)
        glScalef(0.02f, 0.02f, 0.02f);
        
        // Glow effect using emission
        float glowColor[] = { 0.2f * fc.glowIntensity, 0.8f * fc.glowIntensity, 0.2f * fc.glowIntensity, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, glowColor);
        
        // Draw the fuel container model
        model_fuelContainer.Draw();
        
        // Reset emission
        float noEmission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
        
        glPopMatrix();
    }
}

void Level2::checkFuelCollision() {
    if (!flightSim) return;
    
    Vector3f playerPos = flightSim->player.position;
    float collisionRadius = 25.0f;  // Distance to collect (generous for gameplay)
    
    for (size_t i = 0; i < fuelContainers.size(); i++) {
        if (fuelContainers[i].collected) continue;
        
        // Calculate bobbing offset for accurate collision
        float bobHeight = sin(fuelContainers[i].bobOffset) * 3.0f;
        Vector3f containerPos = fuelContainers[i].position;
        containerPos.y += bobHeight;
        
        // Distance check
        float dx = playerPos.x - containerPos.x;
        float dy = playerPos.y - containerPos.y;
        float dz = playerPos.z - containerPos.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        
        if (distSq < collisionRadius * collisionRadius) {
            fuelContainers[i].collected = true;
            collectedCount++;
            
            // Add score for collecting fuel (100 points per container)
            score += 100;
            
            // Bonus points if flying low (under 50 units altitude)
            if (playerPos.y < 50.0f) {
                score += 50;  // Low altitude bonus
            }
        }
    }
}

void Level2::renderHUD() {
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
    
    // Draw semi-transparent background for fuel
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(10, screenHeight - 10);
    glVertex2f(200, screenHeight - 10);
    glVertex2f(200, screenHeight - 50);
    glVertex2f(10, screenHeight - 50);
    glEnd();
    
    // Draw fuel icon (simple rectangle representing canister)
    glColor4f(0.2f, 0.8f, 0.2f, 1.0f);  // Green color
    glBegin(GL_QUADS);
    glVertex2f(20, screenHeight - 20);
    glVertex2f(40, screenHeight - 20);
    glVertex2f(40, screenHeight - 40);
    glVertex2f(20, screenHeight - 40);
    glEnd();
    
    // Draw text for collected count
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(50, screenHeight - 35);
    
    char buffer[64];
    int total = (int)fuelContainers.size();
    sprintf_s(buffer, sizeof(buffer), "Fuel: %d / %d", collectedCount, total);
    
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
    
    // Draw altitude warning if flying too high
    if (flightSim && flightSim->player.position.y > 100.0f) {
        glColor4f(1.0f, 0.0f, 0.0f, 0.7f + 0.3f * sin(arrowBobOffset * 5.0f));
        sprintf_s(buffer, sizeof(buffer), "WARNING: Fly Lower!");
        float warnX = (screenWidth - 180) / 2.0f;
        glRasterPos2f(warnX, screenHeight - 80);
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

// ============ BUILDING OBSTACLE FUNCTIONS ============

void Level2::initBuildings() {
    buildings.clear();
    
    // Create a city-like layout with buildings
    const int numBuildings = 30;
    
    for (int i = 0; i < numBuildings; i++) {
        BuildingObstacle building;
        
        // Distribute buildings in clusters around the map
        float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
        float distance = 150.0f + (rand() % 400);  // 150-550 units from center
        
        building.position.x = cos(angle) * distance;
        building.position.y = 0.0f;  // On ground
        building.position.z = sin(angle) * distance;
        
        building.modelIndex = rand() % 10;  // Random building type
        building.rotation = (float)(rand() % 360);  // Random rotation
        building.scale = 0.3f + (float)(rand() % 30) / 100.0f;  // 0.3 to 0.6 scale
        
        // Collision box dimensions (approximate for buildings)
        building.width = 15.0f * building.scale;
        building.height = 40.0f * building.scale + (float)(rand() % 30);  // Varying heights
        building.depth = 15.0f * building.scale;
        
        buildings.push_back(building);
    }
}

void Level2::renderBuildings() {
    for (size_t i = 0; i < buildings.size(); i++) {
        BuildingObstacle& b = buildings[i];
        
        glPushMatrix();
        
        // Position the building
        glTranslatef(b.position.x, b.position.y, b.position.z);
        
        // Rotate around Y axis
        glRotatef(b.rotation, 0.0f, 1.0f, 0.0f);
        
        // Scale the building
        glScalef(b.scale, b.scale, b.scale);
        
        // Draw the building model
        model_buildings[b.modelIndex].Draw();
        
        glPopMatrix();
    }
}

void Level2::checkBuildingCollision() {
    if (!flightSim || flightSim->isCrashed) return;
    
    Vector3f playerPos = flightSim->player.position;
    float playerRadius = 5.0f;  // Approximate plane collision radius
    
    for (size_t i = 0; i < buildings.size(); i++) {
        BuildingObstacle& b = buildings[i];
        
        // Simple AABB collision check
        float halfWidth = b.width / 2.0f + playerRadius;
        float halfDepth = b.depth / 2.0f + playerRadius;
        
        // Check if player is within building bounds (X and Z)
        float dx = playerPos.x - b.position.x;
        float dz = playerPos.z - b.position.z;
        
        if (fabs(dx) < halfWidth && fabs(dz) < halfDepth) {
            // Check height - player must be below building top
            if (playerPos.y < b.height && playerPos.y > 0) {
                // CRASH!
                flightSim->isCrashed = true;
                flightSim->player.velocity = Vector3f(0, 0, 0);
                flightSim->player.throttle = 0;
                break;
            }
        }
    }
}

// --- AIRPORT LANDING TARGET SYSTEM ---
void Level2::initAirport() {
    // Place airport far from the city (buildings are at 150-550 from center)
    // Airport is placed at -800, 0, -800 so it's clearly outside the city
    airportPosition = Vector3f(-800.0f, 0.0f, -800.0f);
    airportRotation = 30.0f;  // Angled runway
    airportScale = 0.8f;      // Slightly larger for visibility
    hasLanded = false;
    showWinMessage = false;
    winMessageTimer = 0.0f;
}

void Level2::renderAirport() {
    glPushMatrix();
    
    glTranslatef(airportPosition.x, airportPosition.y, airportPosition.z);
    glRotatef(airportRotation, 0.0f, 1.0f, 0.0f);
    glScalef(airportScale, airportScale, airportScale);
    
    model_airport.Draw();
    
    glPopMatrix();
}

void Level2::renderTargetArrow() {
    if (hasLanded) return;  // Don't show arrow after landing
    
    // Animated bobbing arrow above the airport
    float bobHeight = 80.0f + sin(arrowBobOffset) * 10.0f;
    
    glPushMatrix();
    glTranslatef(airportPosition.x, airportPosition.y + bobHeight, airportPosition.z);
    
    // Rotate to always face somewhat visible
    glRotatef(arrowBobOffset * 30.0f, 0.0f, 1.0f, 0.0f);
    
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    
    // Draw a 3D arrow pointing down
    float arrowSize = 15.0f;
    
    // Arrow color - bright green/yellow for visibility
    glColor3f(0.2f, 1.0f, 0.3f);
    
    // Arrow shaft (pointing down)
    glBegin(GL_QUADS);
    // Front face
    glVertex3f(-2.0f, 0.0f, -2.0f);
    glVertex3f(2.0f, 0.0f, -2.0f);
    glVertex3f(2.0f, -arrowSize, -2.0f);
    glVertex3f(-2.0f, -arrowSize, -2.0f);
    // Back face
    glVertex3f(-2.0f, 0.0f, 2.0f);
    glVertex3f(2.0f, 0.0f, 2.0f);
    glVertex3f(2.0f, -arrowSize, 2.0f);
    glVertex3f(-2.0f, -arrowSize, 2.0f);
    // Left face
    glVertex3f(-2.0f, 0.0f, -2.0f);
    glVertex3f(-2.0f, 0.0f, 2.0f);
    glVertex3f(-2.0f, -arrowSize, 2.0f);
    glVertex3f(-2.0f, -arrowSize, -2.0f);
    // Right face
    glVertex3f(2.0f, 0.0f, -2.0f);
    glVertex3f(2.0f, 0.0f, 2.0f);
    glVertex3f(2.0f, -arrowSize, 2.0f);
    glVertex3f(2.0f, -arrowSize, -2.0f);
    glEnd();
    
    // Arrow head (triangle pointing down)
    glColor3f(0.0f, 1.0f, 0.2f);
    glBegin(GL_TRIANGLES);
    // Front triangle
    glVertex3f(-8.0f, -arrowSize, 0.0f);
    glVertex3f(8.0f, -arrowSize, 0.0f);
    glVertex3f(0.0f, -arrowSize - 12.0f, 0.0f);
    // Back triangle
    glVertex3f(-8.0f, -arrowSize, 0.0f);
    glVertex3f(8.0f, -arrowSize, 0.0f);
    glVertex3f(0.0f, -arrowSize - 12.0f, 0.0f);
    glEnd();
    
    // Arrow head sides (3D effect)
    glBegin(GL_QUADS);
    glVertex3f(-8.0f, -arrowSize, -3.0f);
    glVertex3f(-8.0f, -arrowSize, 3.0f);
    glVertex3f(0.0f, -arrowSize - 12.0f, 3.0f);
    glVertex3f(0.0f, -arrowSize - 12.0f, -3.0f);
    
    glVertex3f(8.0f, -arrowSize, -3.0f);
    glVertex3f(8.0f, -arrowSize, 3.0f);
    glVertex3f(0.0f, -arrowSize - 12.0f, 3.0f);
    glVertex3f(0.0f, -arrowSize - 12.0f, -3.0f);
    glEnd();
    
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void Level2::checkLandingCondition() {
    if (!flightSim || hasLanded || flightSim->isCrashed || gameOver) return;
    
    Vector3f playerPos = flightSim->player.position;
    float speed = flightSim->getSpeed();
    
    // Check if player is near the airport
    float dx = playerPos.x - airportPosition.x;
    float dz = playerPos.z - airportPosition.z;
    float distanceXZ = sqrt(dx*dx + dz*dz);
    
    // Landing zone radius
    float landingRadius = 80.0f;
    
    // Landing conditions:
    // 1. Close to airport (within landing zone)
    // 2. Low altitude (near ground)
    // 3. Low speed (not crashing)
    
    if (distanceXZ < landingRadius && 
        playerPos.y < 15.0f && 
        playerPos.y > 0.0f &&
        speed < 50.0f) {
        
        // Successful landing!
        hasLanded = true;
        showWinMessage = true;
        winMessageTimer = 0.0f;
        
        // Calculate landing bonus based on remaining time
        // More time left = higher bonus (up to 1000 points)
        landingBonus = (int)(gameTimer / maxGameTime * 1000.0f);
        score += landingBonus;
        
        // Bonus for fuel collected
        score += collectedCount * 50;
        
        // Stop the plane
        flightSim->player.velocity = Vector3f(0, 0, 0);
        flightSim->player.throttle = 0;
    }
}

void Level2::renderWinScreen() {
    // Setup 2D rendering
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
    float alpha = (winMessageTimer < 1.0f) ? winMessageTimer : 1.0f;
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f * alpha);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f((GLfloat)screenWidth, 0);
    glVertex2f((GLfloat)screenWidth, (GLfloat)screenHeight);
    glVertex2f(0, (GLfloat)screenHeight);
    glEnd();
    
    // Win message box
    float boxWidth = 400.0f;
    float boxHeight = 150.0f;
    float boxX = (screenWidth - boxWidth) / 2.0f;
    float boxY = (screenHeight - boxHeight) / 2.0f;
    
    // Box background
    glColor4f(0.1f, 0.4f, 0.1f, 0.9f * alpha);
    glBegin(GL_QUADS);
    glVertex2f(boxX, boxY);
    glVertex2f(boxX + boxWidth, boxY);
    glVertex2f(boxX + boxWidth, boxY + boxHeight);
    glVertex2f(boxX, boxY + boxHeight);
    glEnd();
    
    // Box border
    glColor4f(0.3f, 1.0f, 0.3f, alpha);
    glLineWidth(3.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(boxX, boxY);
    glVertex2f(boxX + boxWidth, boxY);
    glVertex2f(boxX + boxWidth, boxY + boxHeight);
    glVertex2f(boxX, boxY + boxHeight);
    glEnd();
    
    // Draw "YOU WIN!" text
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    char buffer[128];
    
    // Title
    sprintf_s(buffer, sizeof(buffer), "MISSION COMPLETE!");
    float textX = boxX + 100;
    glRasterPos2f(textX, boxY + boxHeight - 30);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Score breakdown
    glColor4f(0.9f, 0.9f, 0.5f, alpha);
    sprintf_s(buffer, sizeof(buffer), "Landing Bonus: +%d", landingBonus);
    glRasterPos2f(textX - 30, boxY + boxHeight - 60);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    sprintf_s(buffer, sizeof(buffer), "Fuel Collected: %d", collectedCount);
    glRasterPos2f(textX - 30, boxY + boxHeight - 85);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    glColor4f(1.0f, 1.0f, 0.2f, alpha);
    sprintf_s(buffer, sizeof(buffer), "FINAL SCORE: %d", score);
    glRasterPos2f(textX - 20, boxY + boxHeight - 120);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Draw a star pattern as celebration
    float starX = boxX + 50;
    float starY = boxY + boxHeight / 2.0f;
    float starSize = 20.0f + sin(winMessageTimer * 5.0f) * 3.0f;
    
    glColor4f(1.0f, 1.0f, 0.2f, alpha);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(starX, starY);
    for (int i = 0; i <= 10; i++) {
        float angle = (float)i * 3.14159f / 5.0f - 3.14159f / 2.0f;
        float r = (i % 2 == 0) ? starSize : starSize * 0.4f;
        glVertex2f(starX + cos(angle) * r, starY + sin(angle) * r);
    }
    glEnd();
    
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Level2::renderGameOverScreen() {
    // Setup 2D rendering
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
    
    // Red-tinted overlay
    glColor4f(0.3f, 0.0f, 0.0f, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f((GLfloat)screenWidth, 0);
    glVertex2f((GLfloat)screenWidth, (GLfloat)screenHeight);
    glVertex2f(0, (GLfloat)screenHeight);
    glEnd();
    
    // Game over box
    float boxWidth = 350.0f;
    float boxHeight = 150.0f;
    float boxX = (screenWidth - boxWidth) / 2.0f;
    float boxY = (screenHeight - boxHeight) / 2.0f;
    
    // Box background
    glColor4f(0.2f, 0.0f, 0.0f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(boxX, boxY);
    glVertex2f(boxX + boxWidth, boxY);
    glVertex2f(boxX + boxWidth, boxY + boxHeight);
    glVertex2f(boxX, boxY + boxHeight);
    glEnd();
    
    // Box border
    glColor4f(1.0f, 0.3f, 0.3f, 1.0f);
    glLineWidth(3.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(boxX, boxY);
    glVertex2f(boxX + boxWidth, boxY);
    glVertex2f(boxX + boxWidth, boxY + boxHeight);
    glVertex2f(boxX, boxY + boxHeight);
    glEnd();
    
    // Game Over text
    char buffer[128];
    
    glColor3f(1.0f, 0.3f, 0.3f);
    sprintf_s(buffer, sizeof(buffer), "TIME'S UP!");
    float textX = boxX + 120;
    glRasterPos2f(textX, boxY + boxHeight - 35);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    glColor3f(1.0f, 1.0f, 1.0f);
    sprintf_s(buffer, sizeof(buffer), "You ran out of time!");
    glRasterPos2f(textX - 30, boxY + boxHeight - 65);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    sprintf_s(buffer, sizeof(buffer), "Fuel Collected: %d", collectedCount);
    glRasterPos2f(textX - 20, boxY + boxHeight - 95);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    glColor4f(0.8f, 0.8f, 0.3f, 1.0f);
    sprintf_s(buffer, sizeof(buffer), "Final Score: %d", score);
    glRasterPos2f(textX - 10, boxY + boxHeight - 125);
    for (char* c = buffer; *c != '\0'; c++) {
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