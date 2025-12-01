#include "Level2.h"
#include <glut.h>
#include <cmath>

extern void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

Level2::Level2() : Level(), flightSim(nullptr), screenWidth(1280), screenHeight(720), collectedCount(0), collectableTimer(0.0f) {
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
    
    tex_ground.Load("Textures/ground.bmp");
    skySystem.init();  // Initialize sky and lens flare system
}

void Level2::update(float deltaTime) {
    if (!active || !flightSim) return;
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    
    flightSim->update(deltaTime);
    updateWindParticles(deltaTime); // Update wind
    updateFuelContainers(deltaTime); // Update fuel containers
    checkFuelCollision(); // Check for collection
    checkBuildingCollision(); // Check for building crashes
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
    
    // Render HUD (fuel count)
    renderHUD();
    
    glutSwapBuffers();
}

void Level2::renderGround() {
    glDisable(GL_LIGHTING);
    glColor3f(0.6f, 0.6f, 0.6f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_ground.texture[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPushMatrix();
    float px = 0, pz = 0;
    if (flightSim) {
        px = flightSim->player.position.x;
        pz = flightSim->player.position.z;
    }
    glTranslatef(px, 0, pz);
    float size = 2000.0f;
    float texScale = 0.05f;
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f((px - size) * texScale, (pz - size) * texScale); glVertex3f(-size, 0, -size);
    glTexCoord2f((px + size) * texScale, (pz - size) * texScale); glVertex3f(size, 0, -size);
    glTexCoord2f((px + size) * texScale, (pz + size) * texScale); glVertex3f(size, 0, size);
    glTexCoord2f((px - size) * texScale, (pz + size) * texScale); glVertex3f(-size, 0, size);
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
    
    // Draw semi-transparent background for text
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