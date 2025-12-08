#include "Level2.h"
#include "PlaneSelectionLevel.h"
#include <glut.h>
#include <cmath>
#include <stdio.h>
#include <cstring>
#include "HUDRenderer.h"

extern void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

// Custom BMP loader that handles 8/16/24/32-bit BMPs (with V4/V5 headers)
static bool loadGroundTexture(GLuint* texID, const char* filename, bool useAlpha = false) {
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
    else if (bitsPerPixel == 16) {
        // 16-bit 565/555 BMP
        fseek(file, dataOffset, SEEK_SET);
        int rowSize = ((width * 2 + 3) / 4) * 4;
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
                int srcIdx = srcY * rowSize + x * 2;
                unsigned short pix = bmpData[srcIdx] | (bmpData[srcIdx + 1] << 8);
                // Assume 565; fallback to 555 by masking
                unsigned char r = (unsigned char)((pix >> 11) & 0x1F);
                unsigned char g = (unsigned char)((pix >> 5)  & 0x3F);
                unsigned char b = (unsigned char)(pix & 0x1F);
                // Expand to 8-bit per channel
                r = (r << 3) | (r >> 2);
                g = (g << 2) | (g >> 4);
                b = (b << 3) | (b >> 2);
                int destIdx = (y * width + x) * 3;
                rgbData[destIdx + 0] = r;
                rgbData[destIdx + 1] = g;
                rgbData[destIdx + 2] = b;
            }
        }
        delete[] bmpData;
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
    else if (bitsPerPixel == 32) {
        // 32-bit ARGB/BGRA BMP
        delete[] rgbData;  // We need RGBA instead
        unsigned char* rgbaData = new unsigned char[width * height * 4];
        
        fseek(file, dataOffset, SEEK_SET);
        int rowSize = width * 4;
        unsigned char* bmpData = new unsigned char[rowSize * height];
        
        if (fread(bmpData, 1, rowSize * height, file) != (size_t)(rowSize * height)) {
            delete[] bmpData;
            delete[] rgbaData;
            fclose(file);
            return false;
        }
        
        // Convert BGRA to RGBA
        for (int y = 0; y < height; y++) {
            int srcY = topDown ? y : (height - 1 - y);
            for (int x = 0; x < width; x++) {
                int srcIdx = srcY * rowSize + x * 4;
                int destIdx = (y * width + x) * 4;
                rgbaData[destIdx + 0] = bmpData[srcIdx + 2];  // R
                rgbaData[destIdx + 1] = bmpData[srcIdx + 1];  // G
                rgbaData[destIdx + 2] = bmpData[srcIdx + 0];  // B
                // If useAlpha is false, force alpha to 255 (opaque)
                // This fixes issues with 32-bit textures that have 0 alpha (XRGB)
                rgbaData[destIdx + 3] = useAlpha ? bmpData[srcIdx + 3] : 255;
            }
        }
        delete[] bmpData;
        
        fclose(file);
        
        // Create OpenGL texture with alpha
        glGenTextures(1, texID);
        glBindTexture(GL_TEXTURE_2D, *texID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgbaData);
        
        delete[] rgbaData;
        return true;
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
    gameTimer(0.0f), maxGameTime(300.0f), score(0), landingBonus(0),
    gameOver(false), showGameOver(false), runwayLightTimer(0.0f),
    tex_runway(0), tex_airportTerminal(0), runwayLength(700.0f), runwayWidth(50.0f),
    hasTouchedDown(false), touchdownLocalZ(0.0f), tex_grass(0),
    grassRenderDistance(80.0f), grassCellSize(20.0f) {
    runwayPosition = Vector3f(-800.0f, 0.05f, -800.0f);
    runwayRotation = 30.0f;
    terminalPosition = Vector3f(-1260.0f, 0.0f, -440.0f);  // Near runway
    terminalRotation = 30.0f;  // Match runway heading
    terminalScale = 0.01f;  // Much smaller size
}

Level2::~Level2() {
    cleanup();
}

void Level2::init() {
    flightSim = new FlightController();
    
    // Load the selected plane model and texture
    int selectedPlane = PlaneSelectionLevel::getSelectedPlane();
    if (selectedPlane == 1) {
        // Load plane 2
        flightSim->loadModelWithTexture("Models/plane 2/plane2.3ds", "Models/plane 2/Textures/Color.bmp");
    } else {
        // Load plane 1 (default)
        flightSim->loadModelWithTexture("models/plane/mitsubishi_a6m2_zero_model_11.3ds", "models/plane/mitsubishi_a6m2_zero_texture.bmp");
    }
    
    loadAssets();
    particleEffects.init();   // Initialize particle effects system (wind)
    crashSystem.init();       // Initialize unified crash system (explosion + smoke + sound)
    soundSystem.init();       // Initialize sound system (idle + flying sounds)
    shadowSystem.init();      // Initialize shadow system
    shootingSystem.init();    // Initialize shooting system
    initFuelContainers();     // Initialize fuel collectables
    initBuildings();          // Initialize building obstacles
    initAirport();            // Initialize airport landing target
    initTrees();              // Initialize cardboard tree forest

    // Initialize game timer and score
    gameTimer = maxGameTime;  // 300 seconds countdown (5 minutes for full day/night cycle)
    score = 0;
    gameOver = false;
    showGameOver = false;

    if (flightSim) {
        flightSim->player.position = Vector3f(-800.0f, 80.0f, -800.0f);
        flightSim->player.forward = Vector3f(0.707f, 0.0f, 0.707f);
        flightSim->player.forward.unit();
        flightSim->player.up = Vector3f(0, 1, 0);
        flightSim->player.right = flightSim->player.forward.cross(flightSim->player.up);
        flightSim->player.right.unit();
        flightSim->player.velocity = flightSim->player.forward * 35.0f;  
        flightSim->isGrounded = false;
        flightSim->isCrashed = false;
    }
}

void Level2::loadAssets() {
    // Try multiple relative roots so textures load regardless of working directory
    auto tryLoad = [&](GLuint* texID, const char* relativePath, bool useAlpha = false) -> bool {
        const char* prefixes[] = { "", "../", "../../" };
        char fullPath[260];
        for (int i = 0; i < 3; ++i) {
            sprintf_s(fullPath, sizeof(fullPath), "%s%s", prefixes[i], relativePath);
            if (loadGroundTexture(texID, fullPath, useAlpha)) {
                return true;
            }
        }
        return false;
    };

    model_house.Load("Models/house/house.3DS");
    model_tree.Load("Models/tree/Tree1.3ds");
    model_fuelContainer.Load("Models/fuel container/Container Gas  N250815.3DS");
    
    // Load fuel container texture using custom loader (handles more BMP formats)
    // Pass false for useAlpha to ensure opaque rendering even if 32-bit
    if (!tryLoad(&tex_fuelContainer, "models/fuel container/MetalBase0084_M.bmp", false)) {
        // Fallback to metallic gray texture if loading fails
        glGenTextures(1, &tex_fuelContainer);
        glBindTexture(GL_TEXTURE_2D, tex_fuelContainer);
        unsigned char metal[3] = { 120, 120, 130 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, metal);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Force the 3DS material to use the loaded fuel texture (some 3DS files omit map names)
    if (tex_fuelContainer != 0) {
        for (int m = 0; m < model_fuelContainer.numMaterials; ++m) {
            model_fuelContainer.Materials[m].tex.texture[0] = tex_fuelContainer;
            model_fuelContainer.Materials[m].textured = true;
        }
    }
    
    // Create simple green grass texture (procedural)
    glGenTextures(1, &tex_grass);
    glBindTexture(GL_TEXTURE_2D, tex_grass);
    unsigned char grassTex[16 * 16 * 4];
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int idx = (y * 16 + x) * 4;
            // Grass blade shape - transparent on sides, green in middle
            float centerDist = fabs(x - 7.5f) / 7.5f;
            float heightFade = (float)y / 15.0f;  // Fade at top
            bool isGrass = (centerDist < 0.3f + heightFade * 0.4f) && (rand() % 100 > 20);
            grassTex[idx + 0] = 40 + rand() % 40;   // R
            grassTex[idx + 1] = 120 + rand() % 60;  // G
            grassTex[idx + 2] = 30 + rand() % 30;   // B
            grassTex[idx + 3] = isGrass ? 255 : 0;  // A
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, grassTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    
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
    
    // Load runway texture
    // Pass false for useAlpha to ensure opaque rendering
    if (!tryLoad(&tex_runway, "textures/runway.bmp", false)) {
        // Create a dark gray fallback texture for runway
        glGenTextures(1, &tex_runway);
        glBindTexture(GL_TEXTURE_2D, tex_runway);
        unsigned char gray[3] = { 60, 60, 65 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, gray);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
    // Load airport terminal model and texture
    model_airportTerminal.Load("Models/airport terminal/3d-model.3ds");
    if (!tryLoad(&tex_airportTerminal, "models/airport terminal/AussenWand_C.bmp", false)) {
        tryLoad(&tex_airportTerminal, "Models/airport terminal/AussenWand_C.bmp", false);
    }
    
    // Load tree textures (32-bit ARGB with transparency)
    for (int i = 0; i < 3; i++) {
        char filename[64];
        sprintf_s(filename, sizeof(filename), "textures/Tree%s.bmp", i == 0 ? "" : (i == 1 ? "2" : "3"));
        
        // Pass true for useAlpha because trees need transparency
        if (!tryLoad(&tex_tree[i], filename, true)) {
            // Fallback: create a simple green texture
            glGenTextures(1, &tex_tree[i]);
            glBindTexture(GL_TEXTURE_2D, tex_tree[i]);
            unsigned char green[4] = { 40, 120, 30, 255 };
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, green);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
    }
    
    // Load ground texture using custom loader
    // Pass false for useAlpha to ensure opaque rendering
    if (!tryLoad(&tex_ground, "textures/grassGround.bmp", false)) {
        // Fallback to green if texture failed to load
        glGenTextures(1, &tex_ground);
        glBindTexture(GL_TEXTURE_2D, tex_ground);
        unsigned char green[3] = { 50, 150, 50 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, green);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
    skySystem.init();  // Initialize sky and lens flare system
}

void Level2::update(float deltaTime) {
    if (!active || !flightSim) return;
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    
    // Update arrow animation
    arrowBobOffset += deltaTime * 3.0f;
    
    // Update runway light animation timer
    runwayLightTimer += deltaTime;
    if (runwayLightTimer > 2.0f) runwayLightTimer -= 2.0f;  // Loop every 2 seconds
    
    // Update win message timer
    if (showWinMessage) {
        winMessageTimer += deltaTime;
    }
    
    // Update crash effects (explosion + smoke) if crashed
    if (flightSim->isCrashed) {
        crashSystem.update(deltaTime);
        return;
    }
    
    // Update game over display (time ran out - no crash)
    if (showGameOver) {
        return;
    }
    
    if (!hasLanded && !gameOver) {
        // Update game timer
        gameTimer -= deltaTime;
        if (gameTimer <= 0.0f) {
            gameTimer = 0.0f;
            gameOver = true;
            showGameOver = true;
        }
        
        // Track if we were crashed before update
        bool wasCrashedBefore = flightSim->isCrashed;
        
        flightSim->update(deltaTime);
        
        // Update sound system (idle/flying sounds based on state)
        // Returns true if touchdown detected (flying -> idle transition)
        bool touchdown = soundSystem.update(flightSim->isGrounded, flightSim->getSpeed());
        
        // Play touchdown sound when transitioning from flying to landed (anywhere)
        if (touchdown && !hasLanded && !flightSim->isCrashed) {
            soundSystem.playTouchdownSound();
        }
        
        // Update day/night cycle
        skySystem.update(deltaTime);
        
        // Check if FlightController detected a ground crash
        // If so, trigger our unified crash system
        if (flightSim->isCrashed && !wasCrashedBefore) {
            Vector3f crashPos = flightSim->player.position;
            crashPos.y = 1.0f;  // Slightly above ground for visibility
            crashSystem.triggerCrash(crashPos);
            soundSystem.playCrashSound();  // Stop engine sounds, play crash
            return;
        }
        
        // Update wind particle effects
        particleEffects.update(deltaTime, flightSim->player.position, 
                               flightSim->player.forward, flightSim->getSpeed());
        
        // Update shooting system
        shootingSystem.update(deltaTime);
        
        updateFuelContainers(deltaTime); // Update fuel containers
        checkFuelCollision(); // Check for collection
        checkBuildingCollision(); // Check for building crashes
        checkLandingCondition(); // Check if player landed at airport
    }
}

void Level2::render() {
    if (!active) return;
    
    glClearColor(0.35f, 0.45f, 0.65f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    
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
        // Render billboard clouds
        skySystem.renderClouds(flightSim->player.position);
    }
    
    GLfloat lightIntensity[] = { 0.7f, 0.7f, 0.7f, 1.0f };
    GLfloat lightPosition[] = { 0.0f, 100.0f, 0.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightIntensity);
    
    // Update shadow system light direction based on time of day
    DayLighting lighting = skySystem.getCurrentLighting();
    Vector3f shadowLightDir(0.3f, -0.9f, 0.2f);
    if (lighting.sunHeight > 0) {
        shadowLightDir.y = -lighting.sunHeight;
    }
    shadowSystem.setLightDirection(shadowLightDir);
    
    // Adjust shadow darkness based on time of day
    if (skySystem.isNightTime()) {
        shadowSystem.setShadowDarkness(0.15f);  // Very subtle shadows at night
    } else {
        shadowSystem.setShadowDarkness(0.4f);   // Normal shadows during day
    }
    
    renderGround();
    
    // Render optimized grass around camera
    renderGrass();
    
    // Render shadows first (before objects, onto ground)
    renderShadows();
    
    if (flightSim) {
        // Draw plane with wing lights at night
        flightSim->drawPlane(skySystem.isNightTime());
    }
    
    // Render Wind Strips using ParticleEffects system
    if (flightSim) {
        particleEffects.renderWind(flightSim->player.forward, flightSim->getSpeed());
    }
    
    // Render Fuel Containers
    renderFuelContainers();
    
    // Render Cardboard Trees
    renderTrees();
    
    // Render Buildings
    renderBuildings();
    
    // Render Airport Terminal
    renderAirportTerminal();
    
    // Render Runway (textured primitive with markings)
    renderAirport();
    renderRunwayMarkings();
    renderPAPI(skySystem.isNightTime());
    renderRunwayLights(skySystem.isNightTime());
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
    
    // Render Crash Effects (explosion + smoke) using unified CrashSystem
    if (flightSim) {
        crashSystem.render(flightSim->player.position);
    }
    
    // Render bullets and ground explosions from shooting system
    shootingSystem.renderBullets();
    if (flightSim) {
        shootingSystem.renderExplosions(flightSim->player.position);
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
    
    // Render game over screen if time ran out (not crash)
    if (showGameOver && !flightSim->isCrashed) {
        renderGameOverScreen();
    }
}

void Level2::renderGround() {
    glDisable(GL_LIGHTING);

    // Track previous cull state so we can restore it
    GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);  // Draw both sides to avoid upside-down black view
    
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
    if (tex_ground != 0) {
        glBindTexture(GL_TEXTURE_2D, tex_ground);
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
    glDisable(GL_TEXTURE_2D);   // Prevent texture bleed into plane rendering

    // Restore previous culling state
    if (wasCullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

    glEnable(GL_LIGHTING);
    glColor3f(1, 1, 1);
}

// Optimized grass rendering - uses simple billboard quads instead of 3D models
void Level2::renderGrass() {
    if (!flightSim) return;
    
    Vector3f camPos = flightSim->player.position;
    
    // Don't render grass if too high (optimization) - grass not visible from altitude
    if (camPos.y > 40.0f) return;
    
    // Dynamic render distance based on altitude
    float dynamicRenderDist = grassRenderDistance;
    if (camPos.y > 20.0f) {
        dynamicRenderDist *= 0.6f;
    }
    
    // Calculate grid bounds around camera
    int gridHalf = GRASS_GRID_SIZE / 2;
    int camCellX = (int)floor(camPos.x / grassCellSize);
    int camCellZ = (int)floor(camPos.z / grassCellSize);
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // Setup for billboard grass rendering
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_grass);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    
    // Get camera right vector for billboarding
    float modelview[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    Vector3f camRight(modelview[0], modelview[4], modelview[8]);
    
    int grassRendered = 0;
    const int MAX_GRASS = 80;  // Reduced limit for performance
    
    glBegin(GL_QUADS);  // Batch all grass in one draw call
    
    // Iterate through grid cells around camera
    for (int gz = -gridHalf; gz <= gridHalf && grassRendered < MAX_GRASS; gz++) {
        for (int gx = -gridHalf; gx <= gridHalf && grassRendered < MAX_GRASS; gx++) {
            int cellX = camCellX + gx;
            int cellZ = camCellZ + gz;
            
            // Cell world position (center)
            float cellWorldX = cellX * grassCellSize + grassCellSize * 0.5f;
            float cellWorldZ = cellZ * grassCellSize + grassCellSize * 0.5f;
            
            // Quick distance check for culling
            float dx = cellWorldX - camPos.x;
            float dz = cellWorldZ - camPos.z;
            float distSq = dx * dx + dz * dz;
            
            if (distSq > dynamicRenderDist * dynamicRenderDist) continue;
            
            // Use cell coordinates as seed for deterministic random placement
            unsigned int seed = (unsigned int)(cellX * 73856093) ^ (unsigned int)(cellZ * 19349663);
            
            // Render grass patches in this cell
            for (int i = 0; i < GRASS_PER_CELL && grassRendered < MAX_GRASS; i++) {
                // Deterministic pseudo-random position within cell
                seed = seed * 1103515245 + 12345;
                float offsetX = ((seed >> 16) & 0x7FFF) / 32767.0f * grassCellSize;
                seed = seed * 1103515245 + 12345;
                float offsetZ = ((seed >> 16) & 0x7FFF) / 32767.0f * grassCellSize;
                seed = seed * 1103515245 + 12345;
                float grassHeight = 1.5f + ((seed >> 16) & 0x7FFF) / 32767.0f * 1.0f;  // Height 1.5-2.5
                seed = seed * 1103515245 + 12345;
                float grassWidth = 0.8f + ((seed >> 16) & 0x7FFF) / 32767.0f * 0.6f;  // Width 0.8-1.4
                
                float grassX = cellX * grassCellSize + offsetX;
                float grassZ = cellZ * grassCellSize + offsetZ;
                
                // Skip grass on runway area
                float rdx = grassX - runwayPosition.x;
                float rdz = grassZ - runwayPosition.z;
                if (rdx * rdx + rdz * rdz < 14400.0f) continue;  // Skip within 120 units of runway
                
                // Distance-based alpha fade
                float dist = sqrt(distSq);
                float alpha = 1.0f;
                if (dist > dynamicRenderDist * 0.6f) {
                    alpha = 1.0f - (dist - dynamicRenderDist * 0.6f) / (dynamicRenderDist * 0.4f);
                }
                
                // Slight color variation
                float colorVar = 0.8f + ((seed >> 8) & 0xFF) / 255.0f * 0.4f;
                glColor4f(0.3f * colorVar, 0.6f * colorVar, 0.2f * colorVar, alpha);
                
                // Billboard quad vertices (cross pattern for 3D look)
                float hw = grassWidth * 0.5f;
                
                // First quad (aligned with X)
                glTexCoord2f(0, 0); glVertex3f(grassX - hw, 0.0f, grassZ);
                glTexCoord2f(1, 0); glVertex3f(grassX + hw, 0.0f, grassZ);
                glTexCoord2f(1, 1); glVertex3f(grassX + hw, grassHeight, grassZ);
                glTexCoord2f(0, 1); glVertex3f(grassX - hw, grassHeight, grassZ);
                
                // Second quad (aligned with Z) - creates X pattern
                glTexCoord2f(0, 0); glVertex3f(grassX, 0.0f, grassZ - hw);
                glTexCoord2f(1, 0); glVertex3f(grassX, 0.0f, grassZ + hw);
                glTexCoord2f(1, 1); glVertex3f(grassX, grassHeight, grassZ + hw);
                glTexCoord2f(0, 1); glVertex3f(grassX, grassHeight, grassZ - hw);
                
                grassRendered++;
            }
        }
    }
    
    glEnd();  // End batched grass quads
    glPopAttrib();
}

void Level2::handleKeyboard(unsigned char key, bool pressed) {
    if (!active) return;
    if (key == 27) exit(0);
    if (flightSim) flightSim->handleInput(key, pressed);
    // Reset key - full game reset including plane
    if ((key == 'r' || key == 'R') && pressed) {
        crashSystem.reset();
        soundSystem.reset();
        shootingSystem.reset();
        gameTimer = maxGameTime;
        score = 0;
        gameOver = false;
        showGameOver = false;
        hasLanded = false;
        showWinMessage = false;
        hasTouchedDown = false;
        touchdownLocalZ = 0.0f;
        winMessageTimer = 0.0f;

        // Reinitialize fuel containers
        initFuelContainers();

        if (flightSim) {
            flightSim->player.position = Vector3f(-800.0f, 80.0f, -800.0f);
            flightSim->player.forward = Vector3f(0.707f, 0.0f, 0.707f);
            flightSim->player.forward.unit();
            flightSim->player.up = Vector3f(0, 1, 0);
            flightSim->player.right = flightSim->player.forward.cross(flightSim->player.up);
            flightSim->player.right.unit();
            flightSim->player.velocity = flightSim->player.forward * 35.0f;
            flightSim->player.throttle = 0.7f;  
            flightSim->isGrounded = false;
            flightSim->isCrashed = false;
        }
    }
    
    // Shooting - Space key to fire
    if (key == ' ' && pressed && flightSim && !gameOver) {
        shootingSystem.fire(flightSim->player.position, flightSim->player.forward);
    }
    
    
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

    // Reset ALL game state when entering Level 2 (important for level transitions)
    gameTimer = maxGameTime;
    score = 0;
    gameOver = false;
    showGameOver = false;
    hasLanded = false;
    showWinMessage = false;
    hasTouchedDown = false;
    touchdownLocalZ = 0.0f;
    winMessageTimer = 0.0f;

    // Reset systems
    crashSystem.reset();
    soundSystem.reset();
    shootingSystem.reset();

    // Reinitialize collectables
    initFuelContainers();

    // Reload plane based on current selection and set flying state
    if (flightSim) {
        int selectedPlane = PlaneSelectionLevel::getSelectedPlane();
        printf("Level2 loading plane %d on enter\n", selectedPlane);
        flightSim->modelLoaded = false;  // allow reload
        if (selectedPlane == 1) {
            flightSim->loadModelWithTexture("Models/plane 2/plane2.3ds", "Models/plane 2/Textures/Color.bmp");
        } else {
            flightSim->loadModelWithTexture("models/plane/mitsubishi_a6m2_zero_model_11.3ds", "models/plane/mitsubishi_a6m2_zero_texture.bmp");
        }
        flightSim->isCrashed = false;
        // Set position at altitude
        flightSim->player.position = Vector3f(-800.0f, 80.0f, -800.0f);

        // Set forward direction
        flightSim->player.forward = Vector3f(0.707f, 0.0f, 0.707f);
        flightSim->player.forward.unit();

        // Set up vector
        flightSim->player.up = Vector3f(0, 1, 0);

        // Calculate right vector
        flightSim->player.right = flightSim->player.forward.cross(flightSim->player.up);
        flightSim->player.right.unit();

        // Set velocity
        flightSim->player.velocity = flightSim->player.forward * 35.0f;

        // Set throttle
        flightSim->player.throttle = 0.7f;

        // IMPORTANT: In the air, not grounded
        flightSim->isGrounded = false;
        flightSim->isCrashed = false;
    }
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
        
        // Enable texturing and bind fuel container texture
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex_fuelContainer);
        
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
            
            // Play coin collection sound
            soundSystem.playCoinSound();
            
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
    // Save current state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // Switch to 2D orthographic projection
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, screenWidth, 0, screenHeight);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    char buffer[64];
    
    // Draw Fuel Counter (top left)
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(10, screenHeight - 10);
    glVertex2f(200, screenHeight - 10);
    glVertex2f(200, screenHeight - 50);
    glVertex2f(10, screenHeight - 50);
    glEnd();
    
    glColor3f(0.2f, 0.8f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(20, screenHeight - 20);
    glVertex2f(40, screenHeight - 20);
    glVertex2f(40, screenHeight - 40);
    glVertex2f(20, screenHeight - 40);
    glEnd();
    
    glColor3f(1.0f, 1.0f, 1.0f);
    int total = (int)fuelContainers.size();
    sprintf_s(buffer, sizeof(buffer), "Fuel: %d / %d", collectedCount, total);
    glRasterPos2f(50, screenHeight - 35);
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
    
    if (gameTimer > 30.0f) {
        glColor3f(1.0f, 1.0f, 1.0f);
    } else if (gameTimer > 10.0f) {
        glColor3f(1.0f, 1.0f, 0.0f);
    } else {
        glColor3f(1.0f, 0.3f, 0.3f);
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
    
    glColor3f(1.0f, 0.9f, 0.2f);
    sprintf_s(buffer, sizeof(buffer), "Score: %d", score);
    glRasterPos2f(screenWidth - 170, screenHeight - 35);
    for (char* c = buffer; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Draw Speed indicator (bottom left)
    if (flightSim) {
        float speed = flightSim->getSpeed();
        
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glBegin(GL_QUADS);
        glVertex2f(10, 60);
        glVertex2f(180, 60);
        glVertex2f(180, 10);
        glVertex2f(10, 10);
        glEnd();
        
        if (speed < 30.0f) {
            glColor3f(1.0f, 0.3f, 0.3f);
        } else if (speed < 50.0f) {
            glColor3f(1.0f, 1.0f, 0.0f);
        } else if (speed < 80.0f) {
            glColor3f(0.0f, 1.0f, 0.0f);
        } else {
            glColor3f(0.0f, 0.8f, 1.0f);
        }
        
        sprintf_s(buffer, sizeof(buffer), "Speed: %.0f", speed);
        glRasterPos2f(20, 35);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
        
        float speedBarWidth = 150.0f;
        float speedBarFill = (speed / 120.0f) * speedBarWidth;
        if (speedBarFill > speedBarWidth) speedBarFill = speedBarWidth;
        
        glColor4f(0.3f, 0.3f, 0.3f, 0.8f);
        glBegin(GL_QUADS);
        glVertex2f(15, 28);
        glVertex2f(15 + speedBarWidth, 28);
        glVertex2f(15 + speedBarWidth, 22);
        glVertex2f(15, 22);
        glEnd();
        
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
    }
    
    // Draw Attitude Indicator (bottom right)
    if (flightSim) {
        float centerX = screenWidth - 120.0f;
        float centerY = 120.0f;
        float radius = 80.0f;
        
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glBegin(GL_QUADS);
        glVertex2f(screenWidth - 230, 220);
        glVertex2f(screenWidth - 10, 220);
        glVertex2f(screenWidth - 10, 20);
        glVertex2f(screenWidth - 230, 20);
        glEnd();
        
        Vector3f forward = flightSim->player.forward;
        Vector3f up = flightSim->player.up;
        Vector3f right = flightSim->player.right;
        
        float pitch = asin(forward.y) * 180.0f / 3.14159f;
        float roll = atan2(right.y, up.y) * 180.0f / 3.14159f;
        float yaw = atan2(forward.x, forward.z) * 180.0f / 3.14159f;
        if (yaw < 0) yaw += 360.0f;
        
        glPushMatrix();
        glTranslatef(centerX, centerY, 0);
        glRotatef(-roll, 0, 0, 1);
        
        glColor4f(0.3f, 0.5f, 0.8f, 0.7f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(0, 0);
        for (int i = 0; i <= 180; i += 10) {
            float angle = (float)i * 3.14159f / 180.0f;
            glVertex2f(cos(angle) * radius, sin(angle) * radius + pitch * 1.5f);
        }
        glEnd();
        
        glColor4f(0.4f, 0.3f, 0.2f, 0.7f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(0, 0);
        for (int i = 180; i <= 360; i += 10) {
            float angle = (float)i * 3.14159f / 180.0f;
            glVertex2f(cos(angle) * radius, sin(angle) * radius + pitch * 1.5f);
        }
        glEnd();
        
        glColor3f(1.0f, 1.0f, 1.0f);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glVertex2f(-radius, pitch * 1.5f);
        glVertex2f(radius, pitch * 1.5f);
        glEnd();
        glLineWidth(1.0f);
        
        for (int p = -30; p <= 30; p += 10) {
            if (p == 0) continue;
            float yOffset = (pitch - p) * 1.5f;
            if (fabs(yOffset) < radius) {
                glBegin(GL_LINES);
                glVertex2f(-20, yOffset);
                glVertex2f(20, yOffset);
                glEnd();
            }
        }
        
        glPopMatrix();
        
        glColor3f(1.0f, 1.0f, 0.0f);
        glLineWidth(3.0f);
        glBegin(GL_LINES);
        glVertex2f(centerX - 40, centerY);
        glVertex2f(centerX - 10, centerY);
        glVertex2f(centerX + 10, centerY);
        glVertex2f(centerX + 40, centerY);
        glVertex2f(centerX - 2, centerY);
        glVertex2f(centerX + 2, centerY);
        glEnd();
        glLineWidth(1.0f);
        
        glColor3f(0.8f, 0.8f, 0.8f);
        glLineWidth(2.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 360; i += 10) {
            float angle = (float)i * 3.14159f / 180.0f;
            glVertex2f(centerX + cos(angle) * radius, centerY + sin(angle) * radius);
        }
        glEnd();
        glLineWidth(1.0f);
        
        sprintf_s(buffer, sizeof(buffer), "Pitch: %.0f", pitch);
        glColor3f(1.0f, 1.0f, 1.0f);
        glRasterPos2f(screenWidth - 220, 45);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
        
        sprintf_s(buffer, sizeof(buffer), "Roll: %.0f", roll);
        glRasterPos2f(screenWidth - 220, 32);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
        
        sprintf_s(buffer, sizeof(buffer), "Heading: %.0f", yaw);
        glRasterPos2f(screenWidth - 105, 195);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
        }
    }
    
    // Altitude warning when flying too high
    if (flightSim && flightSim->player.position.y > 100.0f) {
        glColor4f(1.0f, 0.0f, 0.0f, 0.7f + 0.3f * sin(arrowBobOffset * 5.0f));
        sprintf_s(buffer, sizeof(buffer), "WARNING: Fly Lower!");
        float warnX = (screenWidth - 180) / 2.0f;
        glRasterPos2f(warnX, screenHeight - 80);
        for (char* c = buffer; *c != '\0'; c++) {
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
        }
    }
    
    // Restore matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}

// ============ BUILDING OBSTACLE FUNCTIONS ============

// --- AIRPORT TERMINAL SYSTEM ---
void Level2::renderAirportTerminal() {
    glPushMatrix();
    
    // Position terminal next to runway (offset to the side)
    // Terminal is placed to the left of the runway looking from approach direction
    float rotRad = runwayRotation * 3.14159f / 180.0f;
    float offsetX = -sin(rotRad) * 150.0f;  // 150 units to the left of runway (further away)
    float offsetZ = cos(rotRad) * 150.0f;
    
    glTranslatef(runwayPosition.x + offsetX, 0.0f, runwayPosition.z + offsetZ);
    glRotatef(runwayRotation, 0.0f, 1.0f, 0.0f);
    glScalef(terminalScale, terminalScale, terminalScale);
    
    // Bind terminal texture if loaded
    if (tex_airportTerminal != 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex_airportTerminal);
    }
    else {
        glDisable(GL_TEXTURE_2D);
    }
    
    model_airportTerminal.Draw();
    
    glPopMatrix();
}

void Level2::initBuildings() {
    buildings.clear();
    
    // Create buildings far from the center and airport (-800, 0, -800)
    // Buildings will be placed in outer ring around 400-700 units from center
    // but NOT in the direction of the airport (which is at -800, -800)
    const int numBuildings = 30;
    
    for (int i = 0; i < numBuildings; i++) {
        BuildingObstacle building;
        
        // Distribute buildings in a ring around the map, avoiding city center and airport
        float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
        
        // Check if this angle points towards the airport (southwest quadrant)
        // Airport is at -800, -800 which is around 225 degrees
        float angleDeg = angle * 180.0f / 3.14159f;
        
        // Skip the southwest quadrant (180-270 degrees) to keep clear path to airport
        if (angleDeg > 180.0f && angleDeg < 280.0f) {
            angle += 1.57f;  // Shift by 90 degrees
        }
        
        // Place buildings in outer ring (400-700 units from center)
        // This keeps them away from city at center and leaves space for flying
        float distance = 400.0f + (rand() % 300);  // 400-700 units from center
        
        building.position.x = cos(angle) * distance;
        building.position.y = 0.0f;  // On ground
        building.position.z = sin(angle) * distance;
        
        building.modelIndex = rand() % 10;  // Random building type
        building.rotation = (float)(rand() % 360);  // Random rotation
        building.scale = 1.5f + (float)(rand() % 150) / 100.0f;  // 0.3 to 0.6 scale
        
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
                // CRASH! Use unified crash system
                flightSim->isCrashed = true;
                flightSim->player.velocity = Vector3f(0, 0, 0);
                flightSim->player.throttle = 0;
                
                // Trigger unified crash (explosion + smoke + sound)
                crashSystem.triggerCrash(playerPos);
                soundSystem.playCrashSound();
                break;
            }
        }
    }
}

// --- REALISTIC RUNWAY SYSTEM ---
void Level2::initAirport() {
    // Runway positioned away from the city
    runwayPosition = Vector3f(-800.0f, 0.5f, -800.0f);  // Raise runway above ground to ensure visibility
    runwayRotation = 30.0f;   // Angled runway (heading 030)
    runwayLength = 700.0f;    // 700 units long (longer for easier landing)
    runwayWidth = 50.0f;      // 50 units wide
    hasLanded = false;
    showWinMessage = false;
    winMessageTimer = 0.0f;
}

void Level2::renderAirport() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    
    // Transform to runway position and rotation
    glTranslatef(runwayPosition.x, runwayPosition.y, runwayPosition.z);
    glRotatef(runwayRotation, 0.0f, 1.0f, 0.0f);
    
    // Enable texturing
    glDisable(GL_CULL_FACE); // Draw both sides; avoid cull state issues
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);  // Pull runway slightly toward camera to prevent z-fighting

    // Ensure we have a valid runway texture (fallback to gray if load failed)
    if (tex_runway == 0) {
        glGenTextures(1, &tex_runway);
        glBindTexture(GL_TEXTURE_2D, tex_runway);
        unsigned char gray[3] = { 80, 80, 80 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, gray);
    }
    glBindTexture(GL_TEXTURE_2D, tex_runway);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    float halfLength = runwayLength / 2.0f;
    float halfWidth = runwayWidth / 2.0f;
    
    // Main runway surface (dark asphalt)
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    // Texture repeats along runway length
    glTexCoord2f(0, 0);           glVertex3f(-halfWidth, 0, -halfLength);
    glTexCoord2f(1, 0);           glVertex3f(halfWidth, 0, -halfLength);
    glTexCoord2f(1, runwayLength/50.0f); glVertex3f(halfWidth, 0, halfLength);
    glTexCoord2f(0, runwayLength/50.0f); glVertex3f(-halfWidth, 0, halfLength);
    glEnd();
    
    // Runway shoulders (slightly lighter concrete)
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_POLYGON_OFFSET_FILL);
    float shoulderWidth = 8.0f;
    glColor3f(0.35f, 0.35f, 0.33f);
    
    // Left shoulder
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-halfWidth - shoulderWidth, 0.005f, -halfLength - 10);
    glVertex3f(-halfWidth, 0.005f, -halfLength - 10);
    glVertex3f(-halfWidth, 0.005f, halfLength + 10);
    glVertex3f(-halfWidth - shoulderWidth, 0.005f, halfLength + 10);
    glEnd();
    
    // Right shoulder
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(halfWidth, 0.005f, -halfLength - 10);
    glVertex3f(halfWidth + shoulderWidth, 0.005f, -halfLength - 10);
    glVertex3f(halfWidth + shoulderWidth, 0.005f, halfLength + 10);
    glVertex3f(halfWidth, 0.005f, halfLength + 10);
    glEnd();
    
    // Threshold areas (darker)
    glColor3f(0.15f, 0.15f, 0.15f);
    float thresholdLength = 20.0f;
    
    // Entry threshold
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-halfWidth, 0.02f, halfLength - thresholdLength);
    glVertex3f(halfWidth, 0.02f, halfLength - thresholdLength);
    glVertex3f(halfWidth, 0.02f, halfLength);
    glVertex3f(-halfWidth, 0.02f, halfLength);
    glEnd();
    
    // Exit threshold
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glVertex3f(-halfWidth, 0.02f, -halfLength);
    glVertex3f(halfWidth, 0.02f, -halfLength);
    glVertex3f(halfWidth, 0.02f, -halfLength + thresholdLength);
    glVertex3f(-halfWidth, 0.02f, -halfLength + thresholdLength);
    glEnd();
    
    glEnable(GL_CULL_FACE);
    glPopMatrix();
    glPopAttrib();
}

void Level2::renderRunwayMarkings() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    
    glTranslatef(runwayPosition.x, runwayPosition.y + 0.06f, runwayPosition.z); // lift markings to avoid z-fight
    glRotatef(runwayRotation, 0.0f, 1.0f, 0.0f);
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);          // Ensure markings are not culled
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);    // Pull markings toward camera
    
    float halfLength = runwayLength / 2.0f;
    float halfWidth = runwayWidth / 2.0f;
    
    // White runway markings
    glColor3f(0.95f, 0.95f, 0.95f);
    
    // === THRESHOLD MARKINGS (piano keys) ===
    int numThresholdBars = 8;
    float barWidth = 3.0f;
    float barLength = 30.0f;
    float barSpacing = (runwayWidth - barWidth) / (numThresholdBars - 1);
    
    // Entry threshold bars
    for (int i = 0; i < numThresholdBars; i++) {
        float x = -halfWidth + barWidth/2 + i * barSpacing;
        glBegin(GL_QUADS);
        glVertex3f(x - barWidth/2, 0, halfLength - 5);
        glVertex3f(x + barWidth/2, 0, halfLength - 5);
        glVertex3f(x + barWidth/2, 0, halfLength - 5 - barLength);
        glVertex3f(x - barWidth/2, 0, halfLength - 5 - barLength);
        glEnd();
    }
    
    // Exit threshold bars
    for (int i = 0; i < numThresholdBars; i++) {
        float x = -halfWidth + barWidth/2 + i * barSpacing;
        glBegin(GL_QUADS);
        glVertex3f(x - barWidth/2, 0, -halfLength + 5);
        glVertex3f(x + barWidth/2, 0, -halfLength + 5);
        glVertex3f(x + barWidth/2, 0, -halfLength + 5 + barLength);
        glVertex3f(x - barWidth/2, 0, -halfLength + 5 + barLength);
        glEnd();
    }
    
    // === CENTERLINE DASHES ===
    float dashLength = 15.0f;
    float dashGap = 10.0f;
    float dashWidth = 1.0f;
    float centerStart = halfLength - 50.0f;  // Start after threshold
    float centerEnd = -halfLength + 50.0f;
    
    for (float z = centerStart; z > centerEnd; z -= (dashLength + dashGap)) {
        glBegin(GL_QUADS);
        glVertex3f(-dashWidth/2, 0, z);
        glVertex3f(dashWidth/2, 0, z);
        glVertex3f(dashWidth/2, 0, z - dashLength);
        glVertex3f(-dashWidth/2, 0, z - dashLength);
        glEnd();
    }
    
    // === AIMING POINT MARKERS (big rectangles) ===
    float aimingPointZ = halfLength - 80.0f;  // 80 units from threshold
    float aimingWidth = 8.0f;
    float aimingLength = 45.0f;
    float aimingOffset = 12.0f;  // Distance from centerline
    
    // Left aiming point
    glBegin(GL_QUADS);
    glVertex3f(-aimingOffset - aimingWidth, 0, aimingPointZ);
    glVertex3f(-aimingOffset, 0, aimingPointZ);
    glVertex3f(-aimingOffset, 0, aimingPointZ - aimingLength);
    glVertex3f(-aimingOffset - aimingWidth, 0, aimingPointZ - aimingLength);
    glEnd();
    
    // Right aiming point
    glBegin(GL_QUADS);
    glVertex3f(aimingOffset, 0, aimingPointZ);
    glVertex3f(aimingOffset + aimingWidth, 0, aimingPointZ);
    glVertex3f(aimingOffset + aimingWidth, 0, aimingPointZ - aimingLength);
    glVertex3f(aimingOffset, 0, aimingPointZ - aimingLength);
    glEnd();
    
    // === TOUCHDOWN ZONE MARKERS ===
    float tzStart = halfLength - 60.0f;
    float tzBarWidth = 4.0f;
    float tzBarLength = 22.0f;
    float tzOffset = 8.0f;
    
    // Three pairs of touchdown zone markers
    for (int row = 0; row < 3; row++) {
        float z = tzStart - row * 40.0f;
        
        // Left bar
        glBegin(GL_QUADS);
        glVertex3f(-tzOffset - tzBarWidth, 0, z);
        glVertex3f(-tzOffset, 0, z);
        glVertex3f(-tzOffset, 0, z - tzBarLength);
        glVertex3f(-tzOffset - tzBarWidth, 0, z - tzBarLength);
        glEnd();
        
        // Right bar
        glBegin(GL_QUADS);
        glVertex3f(tzOffset, 0, z);
        glVertex3f(tzOffset + tzBarWidth, 0, z);
        glVertex3f(tzOffset + tzBarWidth, 0, z - tzBarLength);
        glVertex3f(tzOffset, 0, z - tzBarLength);
        glEnd();
    }
    
    // === EDGE LINES ===
    float edgeWidth = 1.5f;
    
    // Left edge
    glBegin(GL_QUADS);
    glVertex3f(-halfWidth + edgeWidth, 0, halfLength);
    glVertex3f(-halfWidth, 0, halfLength);
    glVertex3f(-halfWidth, 0, -halfLength);
    glVertex3f(-halfWidth + edgeWidth, 0, -halfLength);
    glEnd();
    
    // Right edge
    glBegin(GL_QUADS);
    glVertex3f(halfWidth - edgeWidth, 0, halfLength);
    glVertex3f(halfWidth, 0, halfLength);
    glVertex3f(halfWidth, 0, -halfLength);
    glVertex3f(halfWidth - edgeWidth, 0, -halfLength);
    glEnd();
    
    // === RUNWAY DESIGNATION NUMBERS (simplified - use blocks) ===
    // "03" at entry end (heading 030)
    float numZ = halfLength - 45.0f;
    float numScale = 3.0f;
    
    // Draw "0"
    float num0X = -6.0f;
    glBegin(GL_LINE_LOOP);
    glVertex3f(num0X - 2*numScale, 0, numZ);
    glVertex3f(num0X + 2*numScale, 0, numZ);
    glVertex3f(num0X + 2*numScale, 0, numZ - 8*numScale);
    glVertex3f(num0X - 2*numScale, 0, numZ - 8*numScale);
    glEnd();
    
    // Draw "3"
    float num3X = 6.0f;
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    glVertex3f(num3X - 2*numScale, 0, numZ);
    glVertex3f(num3X + 2*numScale, 0, numZ);
    glVertex3f(num3X + 2*numScale, 0, numZ - 4*numScale);
    glVertex3f(num3X - 1*numScale, 0, numZ - 4*numScale);
    glVertex3f(num3X + 2*numScale, 0, numZ - 4*numScale);
    glVertex3f(num3X + 2*numScale, 0, numZ - 8*numScale);
    glVertex3f(num3X - 2*numScale, 0, numZ - 8*numScale);
    glEnd();
    
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_CULL_FACE);
    glPopMatrix();
    glPopAttrib();
}

void Level2::renderPAPI(bool isNight) {
    // PAPI - Precision Approach Path Indicator
    // 4 lights on each side of runway threshold
    // Shows glide slope: 2 red/2 white = on glide path
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    
    glTranslatef(runwayPosition.x, runwayPosition.y, runwayPosition.z);
    glRotatef(runwayRotation, 0.0f, 1.0f, 0.0f);
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    
    float halfLength = runwayLength / 2.0f;
    float halfWidth = runwayWidth / 2.0f;
    float papiZ = halfLength + 15.0f;  // Just before runway
    float papiX = halfWidth + 20.0f;   // To the left of runway
    float papiSpacing = 5.0f;
    
    // Brightness based on time (brighter at night)
    float brightness = isNight ? 1.0f : 0.6f;
    float glowSize = isNight ? 2.5f : 1.5f;
    
    // Calculate glide slope for player
    float glideAngle = 3.0f;  // Standard 3 degree glide slope
    
    for (int i = 0; i < 4; i++) {
        float x = -papiX + i * papiSpacing;
        float y = 1.0f;  // Elevated on stands
        
        // Alternate colors for visual effect (in reality based on viewing angle)
        bool isRed = (i < 2);  // Simple: first 2 red, last 2 white
        
        if (isRed) {
            glColor4f(1.0f * brightness, 0.1f * brightness, 0.1f * brightness, brightness);
        } else {
            glColor4f(1.0f * brightness, 1.0f * brightness, 1.0f * brightness, brightness);
        }
        
        // Light housing
        glPushMatrix();
        glTranslatef(x, y, papiZ);
        glutSolidSphere(0.8f, 8, 8);
        glPopMatrix();
        
        // Glow effect at night
        if (isNight) {
            if (isRed) {
                glColor4f(1.0f, 0.2f, 0.2f, 0.4f);
            } else {
                glColor4f(1.0f, 1.0f, 0.9f, 0.4f);
            }
            glPushMatrix();
            glTranslatef(x, y, papiZ);
            glutSolidSphere(glowSize, 8, 8);
            glPopMatrix();
        }
    }
    
    glPopMatrix();
    glPopAttrib();
}

void Level2::renderRunwayLights(bool isNight) {
    // Runway lights are more visible at night but also visible during day
    float baseBrightness = isNight ? 1.0f : 0.3f;
    if (!isNight) baseBrightness = 0.0f;  // Only show at night for now
    if (baseBrightness < 0.01f) return;
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    
    glTranslatef(runwayPosition.x, runwayPosition.y, runwayPosition.z);
    glRotatef(runwayRotation, 0.0f, 1.0f, 0.0f);
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for emissive glow
    
    float halfLength = runwayLength / 2.0f;
    float halfWidth = runwayWidth / 2.0f;
    
    // Pulsing effect - smooth sine wave
    float pulse = 0.7f + 0.3f * sin(runwayLightTimer * 3.14159f);
    
    // Sequential chase effect along runway (approach direction)
    float chaseOffset = fmod(runwayLightTimer * 1.5f, 1.0f);
    
    // === EDGE LIGHTS (white/yellow) ===
    const int numEdgeLights = 16;
    float edgeLightSpacing = runwayLength / (float)(numEdgeLights - 1);
    
    for (int i = 0; i < numEdgeLights; i++) {
        float t = (float)i / (float)(numEdgeLights - 1);
        float z = halfLength - i * edgeLightSpacing;
        
        // Chase brightness - creates flowing light effect
        float chaseT = fmod(chaseOffset + t, 1.0f);
        float chaseBrightness = 0.5f + 0.5f * (1.0f - fabs(chaseT - 0.5f) * 2.0f);
        float brightness = baseBrightness * pulse * chaseBrightness;
        
        // Left edge light (warm white/amber)
        float leftX = -halfWidth - 2.0f;
        glColor4f(1.0f * brightness, 0.9f * brightness, 0.6f * brightness, brightness);
        glPushMatrix();
        glTranslatef(leftX, 0.8f, z);
        glutSolidSphere(0.6f, 6, 6);
        glPopMatrix();
        
        // Left glow halo
        glColor4f(1.0f, 0.9f, 0.6f, 0.25f * brightness);
        glPushMatrix();
        glTranslatef(leftX, 0.8f, z);
        glutSolidSphere(1.8f, 6, 6);
        glPopMatrix();
        
        // Right edge light
        float rightX = halfWidth + 2.0f;
        glColor4f(1.0f * brightness, 0.9f * brightness, 0.6f * brightness, brightness);
        glPushMatrix();
        glTranslatef(rightX, 0.8f, z);
        glutSolidSphere(0.6f, 6, 6);
        glPopMatrix();
        
        // Right glow halo
        glColor4f(1.0f, 0.9f, 0.6f, 0.25f * brightness);
        glPushMatrix();
        glTranslatef(rightX, 0.8f, z);
        glutSolidSphere(1.8f, 6, 6);
        glPopMatrix();
    }
    
    // === THRESHOLD LIGHTS (green at entry) ===
    float thresholdPulse = 0.7f + 0.3f * sin(runwayLightTimer * 6.28f);
    const int numThresholdLights = 10;
    float thresholdSpacing = runwayWidth / (float)(numThresholdLights - 1);
    
    for (int i = 0; i < numThresholdLights; i++) {
        float x = -halfWidth + i * thresholdSpacing;
        float z = halfLength + 3.0f;
        float brightness = baseBrightness * thresholdPulse;
        
        // Green threshold light
        glColor4f(0.2f * brightness, 1.0f * brightness, 0.3f * brightness, brightness);
        glPushMatrix();
        glTranslatef(x, 0.6f, z);
        glutSolidSphere(0.7f, 6, 6);
        glPopMatrix();
        
        // Green glow
        glColor4f(0.2f, 1.0f, 0.3f, 0.35f * brightness);
        glPushMatrix();
        glTranslatef(x, 0.6f, z);
        glutSolidSphere(2.0f, 6, 6);
        glPopMatrix();
    }
    
    // === END OF RUNWAY LIGHTS (red warning) ===
    for (int i = 0; i < numThresholdLights; i++) {
        float x = -halfWidth + i * thresholdSpacing;
        float z = -halfLength - 3.0f;
        float brightness = baseBrightness * thresholdPulse;
        
        // Red end light
        glColor4f(1.0f * brightness, 0.15f * brightness, 0.15f * brightness, brightness);
        glPushMatrix();
        glTranslatef(x, 0.6f, z);
        glutSolidSphere(0.7f, 6, 6);
        glPopMatrix();
        
        // Red glow
        glColor4f(1.0f, 0.15f, 0.15f, 0.35f * brightness);
        glPushMatrix();
        glTranslatef(x, 0.6f, z);
        glutSolidSphere(2.0f, 6, 6);
        glPopMatrix();
    }
    
    // === CENTERLINE LIGHTS (white, sequenced) ===
    const int numCenterLights = 12;
    float centerSpacing = (runwayLength - 60) / (float)(numCenterLights + 1);
    
    for (int i = 1; i <= numCenterLights; i++) {
        float z = halfLength - 30 - i * centerSpacing;
        
        // Sequential flash for centerline (approach guidance)
        float flashPhase = fmod(runwayLightTimer * 4.0f + (float)i * 0.15f, 1.0f);
        float flashBrightness = (flashPhase < 0.25f) ? 1.0f : 0.35f;
        float brightness = baseBrightness * flashBrightness;
        
        // White centerline light
        glColor4f(brightness, brightness, brightness * 0.95f, brightness);
        glPushMatrix();
        glTranslatef(0, 0.3f, z);
        glutSolidSphere(0.5f, 6, 6);
        glPopMatrix();
        
        // Subtle glow
        if (flashPhase < 0.25f) {
            glColor4f(1.0f, 1.0f, 0.95f, 0.2f * brightness);
            glPushMatrix();
            glTranslatef(0, 0.3f, z);
            glutSolidSphere(1.2f, 6, 6);
            glPopMatrix();
        }
    }
    
    // === APPROACH LIGHTS (ALSF-style) - lead-in lights before runway ===
    const int numApproachLights = 8;
    float approachSpacing = 15.0f;
    
    for (int i = 0; i < numApproachLights; i++) {
        float z = halfLength + 20.0f + i * approachSpacing;
        
        // Sequential strobe effect (chasing towards runway)
        float strobePhase = fmod(runwayLightTimer * 6.0f - (float)i * 0.12f, 1.0f);
        float strobeBrightness = (strobePhase < 0.15f) ? 1.0f : 0.2f;
        float brightness = baseBrightness * strobeBrightness;
        
        // Center approach light (bright white strobe)
        glColor4f(brightness, brightness, brightness, brightness);
        glPushMatrix();
        glTranslatef(0, 1.5f, z);
        glutSolidSphere(0.8f, 6, 6);
        glPopMatrix();
        
        // Strobe flash glow
        if (strobePhase < 0.15f) {
            glColor4f(1.0f, 1.0f, 1.0f, 0.5f * brightness);
            glPushMatrix();
            glTranslatef(0, 1.5f, z);
            glutSolidSphere(3.0f, 6, 6);
            glPopMatrix();
        }
        
        // Side bar lights (wider pattern further from runway)
        float sideSpread = 5.0f + (float)i * 2.0f;
        float sideBrightness = brightness * 0.6f;
        
        glColor4f(sideBrightness, sideBrightness * 0.95f, sideBrightness * 0.8f, sideBrightness);
        
        // Left side
        glPushMatrix();
        glTranslatef(-sideSpread, 1.0f, z);
        glutSolidSphere(0.5f, 6, 6);
        glPopMatrix();
        
        // Right side
        glPushMatrix();
        glTranslatef(sideSpread, 1.0f, z);
        glutSolidSphere(0.5f, 6, 6);
        glPopMatrix();
    }
    
    glPopMatrix();
    glPopAttrib();
}

void Level2::renderTargetArrow() {
    if (hasLanded) return;  // Don't show arrow after landing
    
    // Animated bobbing arrow above the runway
    float bobHeight = 80.0f + sin(arrowBobOffset) * 10.0f;
    
    glPushMatrix();
    glTranslatef(runwayPosition.x, runwayPosition.y + bobHeight, runwayPosition.z);
    
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
    
    // Simple distance check to airport/runway area
    float dx = playerPos.x - runwayPosition.x;
    float dz = playerPos.z - runwayPosition.z;
    float distanceToAirport = sqrt(dx * dx + dz * dz);
    
    // SUPER SIMPLE WIN CONDITION:
    // Just be on the ground anywhere near the airport (within 500 units)
    // That's it! No speed check, no complex rotation math, just land near the airport.
    
    if (distanceToAirport < 500.0f &&   // Within 500 units of airport
        flightSim->isGrounded) {         // On the ground
        
        // YOU WIN!
        hasLanded = true;
        showWinMessage = true;
        winMessageTimer = 0.0f;
        
        // Play landing sound
        soundSystem.playLandingSound();
        
        // Random landing bonus (100-500 points) - rewards early/lucky landings
        int runwayBonus = 100 + (rand() % 401);
        
        // Time bonus based on remaining time (up to 1000 points)
        landingBonus = (int)(gameTimer / maxGameTime * 1000.0f) + runwayBonus;
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

void Level2::renderShadows() {
    if (!flightSim) return;
    
    // Render plane shadow (oval shaped, follows plane orientation)
    float planeHeight = flightSim->player.position.y;
    if (planeHeight < 150.0f) {  // Only render shadow if plane is not too high
        shadowSystem.renderOvalShadow(
            flightSim->player.position,
            flightSim->player.forward,
            8.0f,   // Length (plane is longer than wide)
            4.0f,   // Width
            planeHeight,
            150.0f  // Max height for visible shadow
        );
    }
    
    // Render shadows for fuel containers
    for (size_t i = 0; i < fuelContainers.size(); i++) {
        if (fuelContainers[i].collected) continue;
        
        float bobHeight = sin(fuelContainers[i].bobOffset) * 3.0f;
        Vector3f containerPos = fuelContainers[i].position;
        float height = containerPos.y + bobHeight;
        
        shadowSystem.renderBlobShadow(containerPos, 3.0f, height, 120.0f);
    }
    
    // Render ambient occlusion at building bases
    for (size_t i = 0; i < buildings.size(); i++) {
        BuildingObstacle& b = buildings[i];
        shadowSystem.renderBaseAO(b.position, b.width * 1.5f, 0.4f);
    }
    
    // Render shadow/AO for trees and houses (static objects)
    shadowSystem.renderBaseAO(Vector3f(10.0f, 0.0f, 0.0f), 5.0f, 0.3f);  // Tree
    shadowSystem.renderBaseAO(Vector3f(0.0f, 0.0f, 0.0f), 8.0f, 0.35f);  // House
    
    // Runway shadow/AO (subtle under the runway area)
    shadowSystem.renderBaseAO(runwayPosition, runwayWidth, 0.15f);
}

// ============ CARDBOARD TREE SYSTEM ============

void Level2::initTrees() {
    trees.clear();
    
    // Forest density and coverage area (realistic forest cover)
    const int numTrees = 500;  // Dense forest coverage
    const float forestSize = 1500.0f;  // Cover a large area
    const float minDistFromAirport = 300.0f;
    const float minDistFromRunway = 200.0f;
    const float minDistFromBuilding = 30.0f;
    const float minTreeSpacing = 8.0f;  // Trees shouldn't be too close to each other
    
    int attempts = 0;
    const int maxAttempts = numTrees * 10;  // Try up to 10x the target number
    
    while (trees.size() < numTrees && attempts < maxAttempts) {
        attempts++;
        
        CardboardTree tree;
        
        // Random position in large area, avoiding center (where airport is)
        float angle = (rand() % 360) * 3.14159f / 180.0f;
        float distance = minDistFromAirport + (rand() % (int)(forestSize - minDistFromAirport));
        
        tree.position.x = runwayPosition.x + cos(angle) * distance;
        tree.position.y = 0.0f;  // Ground level
        tree.position.z = runwayPosition.z + sin(angle) * distance;
        
        // Check if position is clear
        if (!isPositionClearForTree(tree.position)) {
            continue;
        }
        
        // Check spacing from other trees
        bool tooClose = false;
        for (size_t i = 0; i < trees.size(); i++) {
            float dx = trees[i].position.x - tree.position.x;
            float dz = trees[i].position.z - tree.position.z;
            float dist = sqrt(dx * dx + dz * dz);
            if (dist < minTreeSpacing) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) continue;
        
        // Random tree variation
        tree.textureVariant = rand() % 3;  // 0, 1, or 2
        tree.scale = 15.0f + (rand() % 10);  // 15-25 units tall
        tree.rotation = (rand() % 360);  // Random rotation
        
        trees.push_back(tree);
    }
    
}

bool Level2::isPositionClearForTree(const Vector3f& pos) {
    // Check distance from runway
    float dx = pos.x - runwayPosition.x;
    float dz = pos.z - runwayPosition.z;
    float distFromRunway = sqrt(dx * dx + dz * dz);
    if (distFromRunway < 200.0f) {
        return false;
    }
    
    // Check distance from airport terminal
    float tdx = pos.x - terminalPosition.x;
    float tdz = pos.z - terminalPosition.z;
    float distFromTerminal = sqrt(tdx * tdx + tdz * tdz);
    if (distFromTerminal < 100.0f) {
        return false;
    }
    
    // Check distance from buildings
    for (size_t i = 0; i < buildings.size(); i++) {
        float bx = pos.x - buildings[i].position.x;
        float bz = pos.z - buildings[i].position.z;
        float distFromBuilding = sqrt(bx * bx + bz * bz);
        if (distFromBuilding < 30.0f) {
            return false;
        }
    }
    
    return true;
}

void Level2::renderTrees() {
    if (!flightSim) return;
    
    Vector3f cameraPos = flightSim->player.position;
    float renderDistance = 800.0f;  // Only render trees within this distance
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f);
    glEnable(GL_TEXTURE_2D);
    
    glDisable(GL_CULL_FACE);  // Render both sides of the cross
    glDepthMask(GL_TRUE);
    
    for (size_t i = 0; i < trees.size(); i++) {
        CardboardTree& tree = trees[i];
        
        // Distance culling
        float dx = tree.position.x - cameraPos.x;
        float dz = tree.position.z - cameraPos.z;
        float dist = sqrt(dx * dx + dz * dz);
        if (dist > renderDistance) continue;
        
        // Bind appropriate tree texture
        glBindTexture(GL_TEXTURE_2D, tex_tree[tree.textureVariant]);
        
        glPushMatrix();
        glTranslatef(tree.position.x, tree.position.y, tree.position.z);
        
        // Draw two crossed quads (like Minecraft grass/flowers)
        float halfSize = tree.scale * 0.5f;
        float height = tree.scale;
        
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        
        // First quad (along X axis)
        glPushMatrix();
        glRotatef(tree.rotation, 0, 1, 0);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfSize, 0.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfSize, 0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfSize, height, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfSize, height, 0.0f);
        glEnd();
        glPopMatrix();
        
        // Second quad (perpendicular, along Z axis)
        glPushMatrix();
        glRotatef(tree.rotation + 90.0f, 0, 1, 0);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-halfSize, 0.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f(halfSize, 0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f(halfSize, height, 0.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-halfSize, height, 0.0f);
        glEnd();
        glPopMatrix();
        
        glPopMatrix();
    }
    
    glEnable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);
}