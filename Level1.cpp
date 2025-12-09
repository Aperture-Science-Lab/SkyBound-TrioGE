#include "Level1.h"
#include "GameManager.h"
#include "PlaneSelectionLevel.h"
#include <glut.h>
#include <cmath>
#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include "HUDRenderer.h"

extern void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

// Custom BMP loader that handles both 8-bit paletted and 24-bit BMPs
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

Level1::Level1() : Level(), flightSim(nullptr), screenWidth(1280), screenHeight(720),
    collectedCount(0), collectableTimer(0.0f), ringsPassedCount(0), totalRings(10),
    ringTimer(0.0f), gameTimer(0.0f), maxGameTime(600.0f), score(0),
    gameOver(false), showGameOver(false), levelComplete(false), showLevelComplete(false),
    levelCompleteTimer(0.0f), tex_water(0), tex_concrete(0), tex_carrier(0), tex_rings(0),
    tex_rocket(0), tex_container_red(0), tex_container_blue(0), tex_container_yellow(0),
    tex_helipad_metal(0), tex_tent(0),
    tex_tank_rubber(0), tex_tank1(0), tex_tank2(0), tex_tank3(0), tex_tank4(0),
    rocketSpawnTimer(0.0f), rocketSpawnInterval(5.0f),
    waterLevel(-2.0f), portHeight(3.0f),
    spawnProtectionTimer(3.0f), hasSpawnProtection(true) {
    
    // Aircraft carrier positioned in water at surface level
    carrierPosition = Vector3f(0.0f, waterLevel + 50.0f, -100.0f);  // At water surface
    carrierRotation = 0.0f;
    carrierScale = 0.001f;  // Appropriate scale
}

Level1::~Level1() {
    cleanup();
}

void Level1::init() {
    flightSim = new FlightController();
    
    // Plane loading moved to onEnter to support switching planes dynamically
    // flightSim->loadModelWithTexture is called in onEnter()
    
    // Start plane on the carrier deck - stationary, ready for takeoff
    // Position plane at the back of the carrier deck
    flightSim->player.position = Vector3f(carrierPosition.x, carrierPosition.y + 3.0f, carrierPosition.z - 25.0f);
    flightSim->player.velocity = Vector3f(0, 0, 0);  // Stationary
    flightSim->player.forward = Vector3f(0, 0, 1);  // Facing forward along carrier
    flightSim->player.up = Vector3f(0, 1, 0);
    flightSim->player.right = Vector3f(1, 0, 0);
    flightSim->player.throttle = 0.0f;  // Engine off
    flightSim->isGrounded = true;  // Start on ground
    flightSim->isCrashed = false;
    
    // Spawn protection - cannot crash for first 3 seconds
    spawnProtectionTimer = 5.0f;
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
    initBoats();
    
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
    
    // Load port crane model
    model_crane.Load("Models/port crane/crane.3ds");
    
    // Load shipping container model
    model_container.Load("Models/containor/containor.3ds");
    
    // Load helipad model
    model_helipad.Load("Models/helipad/helipad.3ds");
    
    // Load tents model
    model_tents.Load("Models/tents/tents.3ds");
    
    // Load tank model
    model_tank.Load("Models/tank/tiger_tank.3DS");
    
    // Load truck model
    model_truck.Load("Models/truck/truck.3DS");
    
    // Load rocket model
    model_rocket.Load("Models/rocket/Rocket.3ds");

    // Load boat model
    model_boat.Load("Models/boat/Boat.3ds");

    // Load humvee model
    model_humvee.Load("Models/humvees/HUMVEE M242.3ds");

    // Load rocket texture
    if (!loadGroundTexture(&tex_rocket, "Models/rocket/Military Rocket Textures/Military Rocket_mat_BaseColor.bmp")) {
        printf("Failed to load rocket texture, using fallback\n");
        glGenTextures(1, &tex_rocket);
        glBindTexture(GL_TEXTURE_2D, tex_rocket);
        unsigned char gray[3] = { 160, 160, 160 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, gray);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
    // Load boat texture
    if (!loadGroundTexture(&tex_boat, "Models/boat/MEtal Boat.bmp")) {
        printf("Failed to load boat texture, using fallback\n");
        glGenTextures(1, &tex_boat);
        glBindTexture(GL_TEXTURE_2D, tex_boat);
        unsigned char grayBoat[3] = { 120, 120, 130 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, grayBoat);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Force boat materials to use boat texture
    if (tex_boat != 0) {
        for (int m = 0; m < model_boat.numMaterials; ++m) {
            model_boat.Materials[m].tex.texture[0] = tex_boat;
            model_boat.Materials[m].textured = true;
        }
    }

    // Load humvee texture
    if (!loadGroundTexture(&tex_humvee, "Models/humvees/texture.bmp")) {
        printf("Failed to load humvee texture, using fallback\n");
        glGenTextures(1, &tex_humvee);
        glBindTexture(GL_TEXTURE_2D, tex_humvee);
        unsigned char olive[3] = { 85, 90, 70 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, olive);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Force humvee materials to use humvee texture
    if (tex_humvee != 0) {
        for (int m = 0; m < model_humvee.numMaterials; ++m) {
            model_humvee.Materials[m].tex.texture[0] = tex_humvee;
            model_humvee.Materials[m].textured = true;
        }
    }

    // Load container textures
    if (!loadGroundTexture(&tex_container_red, "Models/containor/red-corrugated-surface.bmp")) {
        printf("Failed to load red container texture\n");
    }
    if (!loadGroundTexture(&tex_container_blue, "Models/containor/blue-corrugated-surface.bmp")) {
        printf("Failed to load blue container texture\n");
    }
    if (!loadGroundTexture(&tex_container_yellow, "Models/containor/yellow-corrugated-surface.bmp")) {
        printf("Failed to load yellow container texture\n");
    }
    
    // Load helipad texture
    if (!loadGroundTexture(&tex_helipad_metal, "Models/helipad/heli pad metal.bmp")) {
        printf("Failed to load helipad metal texture\n");
    }
    
    // Load tent texture
    if (!loadGroundTexture(&tex_tent, "Models/tents/Tent.bmp")) {
        printf("Failed to load tent texture\n");
    }
    
    // Load Lighthouse Textures (New)
    // Use loadGroundTexture which is more robust than loadBMP
    if (!loadGroundTexture(&tex_lighthouse_wall, "textures/concert.bmp")) {
         printf("Failed to load lighthouse wall, using fallback\n");
         glGenTextures(1, &tex_lighthouse_wall);
         glBindTexture(GL_TEXTURE_2D, tex_lighthouse_wall);
         unsigned char white[3] = { 255, 255, 255 };
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
    if (!loadGroundTexture(&tex_lighthouse_top, "models/containor/red-corrugated-surface.bmp")) {
         printf("Failed to load lighthouse top, using fallback\n");
         glGenTextures(1, &tex_lighthouse_top);
         glBindTexture(GL_TEXTURE_2D, tex_lighthouse_top);
         unsigned char red[3] = { 200, 50, 50 };
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, red);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Load tank textures
    auto loadTankTex = [&](GLuint* dst, const char* p1, const char* p2) {
        if (!loadGroundTexture(dst, p1)) {
            if (!loadGroundTexture(dst, p2)) {
                printf("Failed to load %s and %s\n", p1, p2);
            }
        }
    };

    loadTankTex(&tex_tank4, "Models/tank/tank4.bmp", "../Models/tank/tank4.bmp");
    if (tex_tank4 == 0) {
        printf("tank4 texture missing; creating fallback color texture\n");
        unsigned char green[3] = { 90, 120, 80 };
        glGenTextures(1, &tex_tank4);
        glBindTexture(GL_TEXTURE_2D, tex_tank4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, green);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    // Force all tank textures to use the single camo texture (tank4.bmp or fallback)
    tex_tank1 = tex_tank4;
    tex_tank2 = tex_tank4;
    tex_tank3 = tex_tank4;
    tex_tank_rubber = tex_tank4;
    
    // Load carrier texture using custom loader (handles more BMP formats)
    printf("Loading carrier texture...\n");
    if (loadGroundTexture(&tex_carrier, "textures/concert.bmp")) {
        printf("Carrier texture loaded successfully! ID: %d\n", tex_carrier);
    } else {
        printf("Failed to load carrier texture, using fallback\n");
        // Fallback to gray texture if loading fails
        glGenTextures(1, &tex_carrier);
        glBindTexture(GL_TEXTURE_2D, tex_carrier);
        unsigned char gray[3] = { 80, 80, 85 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, gray);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Force carrier model materials to use the loaded carrier texture
    if (tex_carrier != 0) {
        for (int m = 0; m < model_carrier.numMaterials; ++m) {
            model_carrier.Materials[m].tex.texture[0] = tex_carrier;
            model_carrier.Materials[m].textured = true;
        }
    }
    
    // Force helipad model materials to use the loaded metal texture
    if (tex_helipad_metal != 0) {
        for (int m = 0; m < model_helipad.numMaterials; ++m) {
            model_helipad.Materials[m].tex.texture[0] = tex_helipad_metal;
            model_helipad.Materials[m].textured = true;
        }
    }
    
    // Force tents model materials to use the loaded tent texture
    if (tex_tent != 0) {
        for (int m = 0; m < model_tents.numMaterials; ++m) {
            model_tents.Materials[m].tex.texture[0] = tex_tent;
            model_tents.Materials[m].textured = true;
        }
    }

    // Force rocket model materials to use the loaded rocket texture
    if (tex_rocket != 0) {
        for (int m = 0; m < model_rocket.numMaterials; ++m) {
            model_rocket.Materials[m].tex.texture[0] = tex_rocket;
            model_rocket.Materials[m].textured = true;
        }
    }
    
    // Force tank model materials to use the loaded tank textures (fallback to gray if load failed)
    if (tex_tank1 == 0) {
        // Create a simple gray texture so materials are not white/untextured
        unsigned char gray[3] = { 160, 160, 160 };
        glGenTextures(1, &tex_tank1);
        glBindTexture(GL_TEXTURE_2D, tex_tank1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, gray);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    if (tex_tank2 == 0) tex_tank2 = tex_tank1;
    if (tex_tank3 == 0) tex_tank3 = tex_tank1;
    if (tex_tank4 == 0) tex_tank4 = tex_tank1;
    if (tex_tank_rubber == 0) tex_tank_rubber = tex_tank1;

    // Apply single camo texture (tank4.bmp) to every tank material
    for (int m = 0; m < model_tank.numMaterials; ++m) {
        model_tank.Materials[m].tex.texture[0] = tex_tank4;
        model_tank.Materials[m].tex.texturename = (char*)"tank4"; // non-null name
        model_tank.Materials[m].textured = true;
        model_tank.Materials[m].tex.Use();
    }

    // Ensure all tank objects are marked textured
    for (int o = 0; o < model_tank.numObjects; ++o) {
        model_tank.Objects[o].textured = true;
    }
    
    // Load rings and rockets texture
    if (!loadGroundTexture(&tex_rings, "textures/Tiles_G_200cm.bmp")) {
        // Fallback to cyan texture if loading fails
        glGenTextures(1, &tex_rings);
        glBindTexture(GL_TEXTURE_2D, tex_rings);
        unsigned char cyan[3] = { 0, 200, 255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, cyan);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    
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

    
    float startX = 0.0f;
    float startY = 70.0f;  
    float startZ = 200.0f;  

    float spacingZ = 300.0f;  
    float heightIncrement = 10.0f;  

    for (int i = 0; i < totalRings; i++) {
        Ring ring;

        // Straight line along Z axis
        ring.position.x = startX;
        ring.position.y = startY + (i * heightIncrement);  // Gradually increase height
        ring.position.z = startZ + (i * spacingZ);  // Space rings evenly along Z

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

void Level1::initBoats() {
    boats.clear();

    auto addBoat = [&](const Vector3f& start, const Vector3f& end, float speed,
               bool moving, float yawDeg, float phase, float bobSpeed, float bobAmount) {
    BoatInstance boat;
    boat.startPosition = start;
    boat.endPosition = moving ? end : start;
    boat.position = start;
    boat.phase = phase;
    boat.bobSpeed = bobSpeed;
    boat.bobAmount = bobAmount;
    boat.isMoving = moving;
    boat.movingForward = true;
    boat.moveSpeed = moving ? speed : 0.0f;

    float yawRad = yawDeg * 3.14159f / 180.0f;
    boat.forwardDir = Vector3f(cos(yawRad), 0.0f, sin(yawRad));

    Vector3f path = boat.endPosition - boat.startPosition;
    boat.pathLength = sqrt(path.x * path.x + path.y * path.y + path.z * path.z);
    if (boat.pathLength > 0.001f) {
        boat.forwardDir = Vector3f(path.x / boat.pathLength, path.y / boat.pathLength, path.z / boat.pathLength);
    }

    boats.push_back(boat);
    };

    float baseY = waterLevel + 8.0f;

    // Stationary boats near port (away from carrier)
    addBoat(Vector3f(360.0f, baseY, -300.0f),
        Vector3f(360.0f, baseY, -300.0f),
        0.0f, false, 90.0f, 1.2f, 1.2f, 0.6f);

    addBoat(Vector3f(320.0f, baseY, 240.0f),
        Vector3f(320.0f, baseY, 240.0f),
        0.0f, false, 45.0f, 2.1f, 1.6f, 0.65f);

    // Moving boats heading toward rings (rings start at z=200 and go to z~3000)
    // Carrier is at x=0, z=-100, so keep boats away from that area
    // Boats on different X lanes to avoid collision

    // Boat 1: Left lane, heading toward rings
    addBoat(Vector3f(-180.0f, baseY, -400.0f),
        Vector3f(-180.0f, baseY, 2800.0f),
        20.0f, true, 0.0f, 0.0f, 1.0f, 0.5f);

    // Boat 2: Far left lane, heading toward rings
    addBoat(Vector3f(-320.0f, baseY, -600.0f),
        Vector3f(-320.0f, baseY, 2600.0f),
        18.0f, true, 0.0f, 1.5f, 1.1f, 0.55f);

    // Boat 3: Right side, heading toward rings
    addBoat(Vector3f(180.0f, baseY, -500.0f),
        Vector3f(180.0f, baseY, 2700.0f),
        22.0f, true, 0.0f, 0.8f, 0.9f, 0.5f);

    // Boat 4: Far right, heading toward rings
    addBoat(Vector3f(280.0f, baseY, -300.0f),
        Vector3f(280.0f, baseY, 2500.0f),
        16.0f, true, 0.0f, 2.0f, 1.2f, 0.6f);

    // Boat 5: Center-left diagonal path
    addBoat(Vector3f(-100.0f, baseY, -700.0f),
        Vector3f(-50.0f, baseY, 2400.0f),
        19.0f, true, 0.0f, 0.5f, 1.0f, 0.55f);
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
            GameManager::getInstance().switchToLevel("level2");
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
        // Keep plane on carrier deck during spawn protection
        if (isOnCarrierDeck(flightSim->player.position)) {
            float deckY = carrierPosition.y + 3.0f;
            if (flightSim->player.position.y < deckY) {
                flightSim->player.position.y = deckY;
                flightSim->player.velocity.y = 0.0f;
            }
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
        
        // Store old position for carrier deck collision
        Vector3f oldPos = flightSim->player.position;
        
        flightSim->update(deltaTime);
        
        // Carrier deck collision detection - check if plane is on carrier
        if (isOnCarrierDeck(flightSim->player.position)) {
            float deckY = carrierPosition.y + 3.0f;  // Deck height
            
            // If plane is at or below deck level
            if (flightSim->player.position.y <= deckY + 0.5f) {
                flightSim->player.position.y = deckY + 0.5f;  // Place on deck
                
                // Check if landing or already on deck
                if (!flightSim->isGrounded) {
                    float verticalSpeed = flightSim->player.velocity.y;
                    float speed = flightSim->getSpeed();
                    
                    // Hard crash: Falling too fast or hitting deck nose first
                    if (verticalSpeed < -10.0f || (speed > 40.0f && flightSim->player.forward.y < -0.2f)) {
                        if (!hasSpawnProtection) {
                            flightSim->isCrashed = true;
                            flightSim->player.throttle = 0;
                        }
                    } else {
                        // Successful landing on carrier
                        flightSim->isGrounded = true;
                        flightSim->player.velocity.y = 0;
                        
                        // Flatten out pitch slightly on landing
                        if (flightSim->player.forward.y < 0) {
                            flightSim->player.forward.y = 0;
                            flightSim->player.forward = flightSim->player.forward.unit();
                        }
                    }
                } else {
                    // Already on carrier deck
                    flightSim->player.velocity.y = 0;  // Cancel gravity
                    
                    // Allow takeoff from carrier
                    float speed = flightSim->getSpeed();
                    if (speed > 50.0f && flightSim->player.forward.y > 0.1f) {  // TAKEOFF_SPEED = 50.0f
                        flightSim->isGrounded = false;  // Liftoff!
                        flightSim->player.position.y += 0.1f;  // Unstick from deck
                    }
                }
            }
        }
        
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
                               
        // Jet Trail for Plane 3 (Stealth Jet - selection index 2)
        if (flightSim && PlaneSelectionLevel::getSelectedPlane() == 2 && flightSim->getSpeed() > 5.0f) {
           Vector3f offset = flightSim->player.forward * -2.0f; // Behind plane
           particleEffects.emitJetTrail(flightSim->player.position + offset, Vector3f(0,0,0));
        }
        
        // Update shooting system
        shootingSystem.update(deltaTime);
        
        // Update game elements
        updateRings(deltaTime);
        updateToolkits(deltaTime);
        updateRockets(deltaTime);
        updateBoats(deltaTime);
        
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

void Level1::updateBoats(float deltaTime) {
    float time = ringTimer;

    for (auto& boat : boats) {
        float baseY = boat.startPosition.y;
        float bob = sin(time * boat.bobSpeed + boat.phase) * boat.bobAmount;
        boat.position.y = baseY + bob;

        if (boat.isMoving && boat.pathLength > 0.001f) {
            Vector3f dir = boat.forwardDir * (boat.movingForward ? 1.0f : -1.0f);
            boat.position = boat.position + dir * (boat.moveSpeed * deltaTime);

            float traveled = (boat.position.x - boat.startPosition.x) * boat.forwardDir.x +
                             (boat.position.y - boat.startPosition.y) * boat.forwardDir.y +
                             (boat.position.z - boat.startPosition.z) * boat.forwardDir.z;

            if (boat.movingForward && traveled >= boat.pathLength) {
                boat.position = boat.endPosition;
                boat.movingForward = false;
            } else if (!boat.movingForward && traveled <= 0.0f) {
                boat.position = boat.startPosition;
                boat.movingForward = true;
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

    // Find the next ring that needs to be passed (first unpassed ring in order)
    int nextRingIndex = -1;
    for (int i = 0; i < (int)rings.size(); i++) {
        if (!rings[i].passed) {
            nextRingIndex = i;
            break;
        }
    }
    
    // If all rings are passed, we're done
    if (nextRingIndex == -1) return;
    
    // Only check collision with the next ring in sequence
    Ring& ring = rings[nextRingIndex];
    
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

            // Check if this is the golden ring
            if (ring.isGolden) {
                // All rings passed including golden ring - switch to Level 2
                score += 500;  // Golden ring bonus
                score += (int)(gameTimer * 10);  // Time bonus
                soundSystem.playCoinSound();

                // Trigger level switch immediately to Level 2
                levelComplete = true;
				showLevelComplete = true;
                GameManager::getInstance().switchToLevel("level2");
                return;
            }
            else {
                score += 100;
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
    // Increased collision radius significantly (hurt box) to make rockets deadly
    float hitRadius = 25.0f; 
    
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
    float hitRadius = 20.0f;  // Collision radius for bullet-rocket (Increased as requested)
    
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
    
    float portX = 450.0f;  // Updated to match new port position
    float lightY = portHeight + 5.0f;
    
    // Pulsing effect
    float pulse = 0.7f + 0.3f * sin(ringTimer * 2.0f);
    
    // Place lights along the expanded port edge
    for (float z = -1400.0f; z <= 1400.0f; z += 120.0f) {
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
    // Carrier deck bounds (adjusted for scale 0.001)
    // The carrier model at scale 0.001 is much larger than before
    // Typical carrier deck dimensions after scaling
    float deckMinX = carrierPosition.x - 25.0f;  // Wider deck
    float deckMaxX = carrierPosition.x + 25.0f;
    float deckMinZ = carrierPosition.z - 40.0f;  // Longer deck for takeoff
    float deckMaxZ = carrierPosition.z + 40.0f;
    float deckY = carrierPosition.y + 3.0f;  // Deck height above carrier base
    
    return (pos.x >= deckMinX && pos.x <= deckMaxX &&
            pos.z >= deckMinZ && pos.z <= deckMaxZ &&
            pos.y <= deckY + 3.0f && pos.y >= deckY - 1.0f);
}

void Level1::render() {
    if (!active) return;
    
    static int renderCount = 0;
    if (renderCount < 100) {
        printf("  Level1::render() #%d, active=%d\n", renderCount++, active);
    }
    
    glClearColor(0.35f, 0.45f, 0.65f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0f);
    
    // ===== ENHANCED GRAPHICS SETTINGS =====
    glShadeModel(GL_SMOOTH);  // Smooth Gouraud shading
    glEnable(GL_NORMALIZE);   // Normalize normals for proper lighting after scaling
    glEnable(GL_COLOR_MATERIAL);  // Use glColor with lighting
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);  // Better specular
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);  // One-sided lighting
    
    // Anti-aliasing hints
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    
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
    
    // Render sky
    if (flightSim) {
        skySystem.renderSky(flightSim->player.position);
        skySystem.renderClouds(flightSim->player.position);
    }
    
    // ===== AAA LIGHTING SYSTEM =====
    // Get time-of-day lighting from sky system
    DayLighting lighting = skySystem.getCurrentLighting();
    bool isNight = skySystem.isNightTime();
    
    // GL_LIGHT0: Directional Sun/Moon Light
    GLfloat sunDirection[] = { 0.3f, -0.7f, -0.5f, 0.0f };  // Directional (w=0)
    GLfloat sunAmbient[] = { 0.3f, 0.3f, 0.35f, 1.0f };
    GLfloat sunDiffuse[] = { 0.9f, 0.85f, 0.7f, 1.0f };  // Warm daylight
    GLfloat sunSpecular[] = { 1.0f, 0.95f, 0.8f, 1.0f };
    
    // Dynamic time-of-day adjustment
    if (isNight) {
        // Cool blue moonlight
        sunAmbient[0] = 0.1f; sunAmbient[1] = 0.12f; sunAmbient[2] = 0.18f;
        sunDiffuse[0] = 0.2f; sunDiffuse[1] = 0.25f; sunDiffuse[2] = 0.4f;
        sunSpecular[0] = 0.3f; sunSpecular[1] = 0.35f; sunSpecular[2] = 0.5f;
    } else if (lighting.sunHeight < 0.3f) {
        // Sunrise/sunset - orange/red tint
        float t = lighting.sunHeight / 0.3f;
        sunDiffuse[0] = 1.0f;
        sunDiffuse[1] = 0.5f + t * 0.35f;
        sunDiffuse[2] = 0.3f + t * 0.4f;
    }
    
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, sunDirection);
    glLightfv(GL_LIGHT0, GL_AMBIENT, sunAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sunDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, sunSpecular);
    
    // GL_LIGHT1: Port Area ROTATING Searchlight (Lighthouse effect) - MEETS LIGHT ANIMATION CRITERIA
    // Port is at x=450, extends from z=-1500 to z=1500
    
    // Shared ambient for port area lights
    GLfloat portAmbient[] = { 0.05f, 0.05f, 0.05f, 1.0f };
    
    if (isNight) {
        float time = ringTimer * 2.0f; // Rotation speed
        // Positioned high up
        GLfloat portLightPos[] = { 500.0f, 60.0f, 0.0f, 1.0f };
        
        // Rotating direction
        GLfloat spotDir[] = { cos(time), -0.5f, sin(time) };
        
        GLfloat portDiffuse[] = { 1.0f, 0.9f, 0.7f, 1.0f };  // Stronger beam
        GLfloat portSpecular[] = { 1.0f, 1.0f, 0.9f, 1.0f };
        
        glEnable(GL_LIGHT1);
        glLightfv(GL_LIGHT1, GL_POSITION, portLightPos);
        glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, spotDir);
        glLightfv(GL_LIGHT1, GL_AMBIENT, portAmbient);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, portDiffuse);
        glLightfv(GL_LIGHT1, GL_SPECULAR, portSpecular);
        
        // Make it a spotlight
        glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 30.0f);
        glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 20.0f);
        
        // Attenuation for realistic falloff
        glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.01f);
        glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.001f);
    } else {
        glDisable(GL_LIGHT1);
    }
    
    // GL_LIGHT2: Carrier Deck Lights
    GLfloat carrierLightPos[] = { carrierPosition.x, carrierPosition.y + 15.0f, carrierPosition.z, 1.0f };
    GLfloat carrierDiffuse[] = { 0.9f, 0.9f, 0.95f, 1.0f };  // Bright white deck lights
    GLfloat carrierSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    
    if (isNight) {
        glEnable(GL_LIGHT2);
        glLightfv(GL_LIGHT2, GL_POSITION, carrierLightPos);
        glLightfv(GL_LIGHT2, GL_AMBIENT, portAmbient);
        glLightfv(GL_LIGHT2, GL_DIFFUSE, carrierDiffuse);
        glLightfv(GL_LIGHT2, GL_SPECULAR, carrierSpecular);
        glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.015f);
        glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.003f);
    } else {
        glDisable(GL_LIGHT2);
    }
    
    // GL_LIGHT3: Dynamic Plane Landing Lights (forward spotlight)
    if (flightSim && flightSim->player.throttle > 0.3f) {
        Vector3f lightPos = flightSim->player.position + flightSim->player.forward * 5.0f;
        GLfloat planeLightPos[] = { lightPos.x, lightPos.y, lightPos.z, 1.0f };
        GLfloat planeLightDir[] = { flightSim->player.forward.x, flightSim->player.forward.y, flightSim->player.forward.z };
        GLfloat planeDiffuse[] = { 1.0f, 1.0f, 0.95f, 1.0f };
        
        glEnable(GL_LIGHT3);
        glLightfv(GL_LIGHT3, GL_POSITION, planeLightPos);
        glLightfv(GL_LIGHT3, GL_SPOT_DIRECTION, planeLightDir);
        glLightfv(GL_LIGHT3, GL_DIFFUSE, planeDiffuse);
        glLightfv(GL_LIGHT3, GL_SPECULAR, planeDiffuse);
        glLightf(GL_LIGHT3, GL_SPOT_CUTOFF, 25.0f);  // 25 degree cone
        glLightf(GL_LIGHT3, GL_SPOT_EXPONENT, 15.0f);  // Focused beam
        glLightf(GL_LIGHT3, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(GL_LIGHT3, GL_LINEAR_ATTENUATION, 0.05f);
        glLightf(GL_LIGHT3, GL_QUADRATIC_ATTENUATION, 0.01f);
    } else {
        glDisable(GL_LIGHT3);
    }
    
    // GL_LIGHT4: Secondary Fill Light (opposite side of sun for softer shadows)
    GLfloat fillLightPos[] = { -0.5f, 0.3f, 0.5f, 0.0f };  // Directional fill
    GLfloat fillDiffuse[] = { 0.25f, 0.28f, 0.35f, 1.0f };  // Cool blue fill
    GLfloat fillSpecular[] = { 0.1f, 0.1f, 0.15f, 1.0f };
    glEnable(GL_LIGHT4);
    glLightfv(GL_LIGHT4, GL_POSITION, fillLightPos);
    glLightfv(GL_LIGHT4, GL_DIFFUSE, fillDiffuse);
    glLightfv(GL_LIGHT4, GL_SPECULAR, fillSpecular);
    GLfloat noAmbient[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glLightfv(GL_LIGHT4, GL_AMBIENT, noAmbient);
    
    // GL_LIGHT5: Rim/Back Light for better object definition
    GLfloat rimLightPos[] = { 0.0f, 0.8f, -1.0f, 0.0f };  // From behind/above
    GLfloat rimDiffuse[] = { 0.3f, 0.32f, 0.4f, 1.0f };   // Subtle rim light
    GLfloat rimSpecular[] = { 0.5f, 0.5f, 0.6f, 1.0f };   // Stronger specular for rim
    glEnable(GL_LIGHT5);
    glLightfv(GL_LIGHT5, GL_POSITION, rimLightPos);
    glLightfv(GL_LIGHT5, GL_DIFFUSE, rimDiffuse);
    glLightfv(GL_LIGHT5, GL_SPECULAR, rimSpecular);
    glLightfv(GL_LIGHT5, GL_AMBIENT, noAmbient);
    
    // GL_LIGHT6: Water Reflection Light (bounced light from ocean)
    if (!isNight) {
        GLfloat waterReflectPos[] = { 0.0f, -1.0f, 0.0f, 0.0f };  // From below (water reflection)
        GLfloat waterReflectDiffuse[] = { 0.15f, 0.2f, 0.25f, 1.0f };  // Blue-ish water bounce
        glEnable(GL_LIGHT6);
        glLightfv(GL_LIGHT6, GL_POSITION, waterReflectPos);
        glLightfv(GL_LIGHT6, GL_DIFFUSE, waterReflectDiffuse);
        glLightfv(GL_LIGHT6, GL_SPECULAR, noAmbient);  // No specular from water
        glLightfv(GL_LIGHT6, GL_AMBIENT, noAmbient);
    } else {
        glDisable(GL_LIGHT6);
    }
    
    // Set global ambient light
    GLfloat globalAmbient[] = { 0.2f, 0.2f, 0.25f, 1.0f };
    if (isNight) {
        globalAmbient[0] = 0.05f; globalAmbient[1] = 0.05f; globalAmbient[2] = 0.08f;
    }
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);
    
    // DISABLE FOG for Level 1 to match user request and fix "flat blue" water
    glDisable(GL_FOG);
    
    /* ===== ATMOSPHERIC FOG FOR DEPTH =====
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);  // Exponential fog for realistic atmosphere
    
    // Dynamic fog color based on time of day
    GLfloat fogColor[4];
    if (isNight) {
        fogColor[0] = 0.02f; fogColor[1] = 0.03f; fogColor[2] = 0.08f; fogColor[3] = 1.0f;  // Dark blue night
    } else if (lighting.sunHeight < 0.3f) {
        // Sunrise/sunset - golden haze
        float t = lighting.sunHeight / 0.3f;
        fogColor[0] = 0.8f; fogColor[1] = 0.5f + t * 0.2f; fogColor[2] = 0.4f + t * 0.3f; fogColor[3] = 1.0f;
    } else {
        fogColor[0] = 0.6f; fogColor[1] = 0.7f; fogColor[2] = 0.85f; fogColor[3] = 1.0f;  // Hazy blue sky
    }
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, 0.0008f);  // Subtle distant fog
    glHint(GL_FOG_HINT, GL_NICEST);
    */
    
    // Update shadow system
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
    renderBoats();
    
    // Render shadows
    renderShadows();
    
    // Render plane
    if (flightSim) {
        flightSim->drawPlane(skySystem.isNightTime());
    }
    
    // Render wind particles
    if (flightSim) {
        particleEffects.renderWind(flightSim->player.forward, flightSim->getSpeed());
        
        // Render jet trails
        particleEffects.renderJetTrail();
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
}

void Level1::renderWater() {
    // Reset material state to prevent metallic appearance
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG); // Ensure fog is disabled for water
    glDisable(GL_COLOR_MATERIAL);
    // Reset material to white
    GLfloat white[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, white);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_water);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); // CHANGED TO MODULATE for Navy Blue tint
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    float size = 6000.0f;
    float texScale = 0.003f;
    
    // Animated texture offset - slides the water texture
    float timeOffset = ringTimer * 0.05f;  // Slow sliding motion
    float waveOffset = sin(ringTimer * 0.3f) * 0.02f;  // Gentle wave motion
    
    // Multiple water layers for depth effect
    // Layer 1: Deep water (darker, slower) - ADJUSTED LIGHTER
    glColor4f(0.05f, 0.1f, 0.35f, 1.0f); // Medium deep blue
    glBegin(GL_QUADS);
    float offset1 = timeOffset * 0.3f;
    glTexCoord2f(offset1, offset1 + waveOffset);
    glVertex3f(-size, waterLevel - 1.0f, -size);
    glTexCoord2f(size * texScale * 2 + offset1, offset1 + waveOffset);
    glVertex3f(size, waterLevel - 1.0f, -size);
    glTexCoord2f(size * texScale * 2 + offset1, size * texScale * 2 + offset1 - waveOffset);
    glVertex3f(size, waterLevel - 1.0f, size);
    glTexCoord2f(offset1, size * texScale * 2 + offset1 - waveOffset);
    glVertex3f(-size, waterLevel - 1.0f, size);
    glEnd();
    
    // Layer 2: Main water surface (medium blue, main animation) - ADJUSTED LIGHTER
    glColor4f(0.1f, 0.25f, 0.55f, 0.9f); // Lighter blue
    glBegin(GL_QUADS);
    float offset2 = timeOffset + waveOffset;
    glTexCoord2f(offset2, -offset2 * 0.5f);
    glVertex3f(-size, waterLevel - 0.2f, -size);
    glTexCoord2f(size * texScale * 2 + offset2, -offset2 * 0.5f);
    glVertex3f(size, waterLevel - 0.2f, -size);
    glTexCoord2f(size * texScale * 2 + offset2, size * texScale * 2 - offset2 * 0.5f);
    glVertex3f(size, waterLevel - 0.2f, size);
    glTexCoord2f(offset2, size * texScale * 2 - offset2 * 0.5f);
    glVertex3f(-size, waterLevel - 0.2f, size);
    glEnd();
    
    // Layer 3: Surface highlights (lighter, faster, more transparent) - UPDATED SUBTLER
    glColor4f(0.1f, 0.2f, 0.4f, 0.25f);
    glBegin(GL_QUADS);
    float offset3 = timeOffset * 1.5f - waveOffset * 2.0f;
    float texScale2 = texScale * 1.5f;  // Different scale for variety
    glTexCoord2f(-offset3, offset3 * 0.7f);
    glVertex3f(-size, waterLevel + 0.05f, -size);
    glTexCoord2f(size * texScale2 * 2 - offset3, offset3 * 0.7f);
    glVertex3f(size, waterLevel + 0.05f, -size);
    glTexCoord2f(size * texScale2 * 2 - offset3, size * texScale2 * 2 + offset3 * 0.7f);
    glVertex3f(size, waterLevel + 0.05f, size);
    glTexCoord2f(-offset3, size * texScale2 * 2 + offset3 * 0.7f);
    glVertex3f(-size, waterLevel + 0.05f, size);
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
    
    // Sea foam where water meets port edge and carrier (MORE PROMINENT)
    glDisable(GL_TEXTURE_2D);
    
    float portX = 450.0f;
    float foamWidth = 15.0f;  // Wider foam strip
    float foamSize = 1500.0f;
    
    // Foam along port edge - brighter and more visible
    glColor4f(1.0f, 1.0f, 1.0f, 0.85f + 0.15f * sin(ringTimer * 2.0f));
    glBegin(GL_QUAD_STRIP);
    for (float z = -foamSize; z <= foamSize; z += 40.0f) {
        float wave = sin(z * 0.01f + ringTimer) * 2.5f;
        glVertex3f(portX + wave, waterLevel + 0.4f, z);
        glVertex3f(portX + foamWidth + wave, waterLevel + 0.4f, z);
    }
    glEnd();
    
    // Additional inner foam layer for more visibility
    glColor4f(0.95f, 0.98f, 1.0f, 0.6f + 0.3f * sin(ringTimer * 3.0f));
    glBegin(GL_QUAD_STRIP);
    for (float z = -foamSize; z <= foamSize; z += 40.0f) {
        float wave = sin(z * 0.015f + ringTimer * 1.5f) * 2.0f;
        glVertex3f(portX + foamWidth + wave, waterLevel + 0.45f, z);
        glVertex3f(portX + foamWidth * 2.0f + wave, waterLevel + 0.45f, z);
    }
    glEnd();
    
    // Foam around carrier (circular pattern) - more prominent
    Vector3f carrierCenter = carrierPosition;
    float carrierRadius = 30.0f;
    
    // Outer foam ring
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
    glVertex3f(carrierCenter.x, waterLevel + 0.4f, carrierCenter.z);
    for (int i = 0; i <= 32; i++) {
        float angle = (float)i / 32.0f * 6.28318f;
        float wave = sin(angle * 3.0f + ringTimer * 2.0f) * 3.0f;
        float x = carrierCenter.x + cos(angle) * (carrierRadius + wave);
        float z = carrierCenter.z + sin(angle) * (carrierRadius + wave);
        glColor4f(1.0f, 1.0f, 1.0f, 0.75f + 0.15f * sin(angle * 2.0f + ringTimer));
        glVertex3f(x, waterLevel + 0.4f, z);
    }
    glEnd();
    
    // Inner foam ring for carrier
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.95f, 0.98f, 1.0f, 0.8f);
    glVertex3f(carrierCenter.x, waterLevel + 0.45f, carrierCenter.z);
    for (int i = 0; i <= 32; i++) {
        float angle = (float)i / 32.0f * 6.28318f;
        float wave = sin(angle * 4.0f + ringTimer * 2.5f) * 2.0f;
        float x = carrierCenter.x + cos(angle) * (carrierRadius + 10.0f + wave);
        float z = carrierCenter.z + sin(angle) * (carrierRadius + 10.0f + wave);
        glColor4f(0.95f, 0.98f, 1.0f, 0.6f + 0.2f * sin(angle * 3.0f + ringTimer * 1.5f));
        glVertex3f(x, waterLevel + 0.45f, z);
    }
    glEnd();
    
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // CRITICAL FIX: Restore texture environment to default (MODULATE)
    // This prevents the DECAL mode from leaking to other objects (explosions, lens flare, etc.)
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void Level1::renderBoats() {
    if (boats.empty()) return;

    // Set material properties for boats (metal hull)
    GLfloat boatAmbient[] = { 0.25f, 0.25f, 0.3f, 1.0f };
    GLfloat boatDiffuse[] = { 0.6f, 0.6f, 0.65f, 1.0f };
    GLfloat boatSpecular[] = { 0.7f, 0.7f, 0.75f, 1.0f };
    GLfloat boatShininess = 40.0f;  // Shiny metal hull
    glMaterialfv(GL_FRONT, GL_AMBIENT, boatAmbient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, boatDiffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, boatSpecular);
    glMaterialf(GL_FRONT, GL_SHININESS, boatShininess);

    for (const auto& boat : boats) {
        glPushMatrix();
        glTranslatef(boat.position.x, boat.position.y, boat.position.z);

        Vector3f dir = boat.forwardDir * (boat.movingForward ? 1.0f : -1.0f);
        float yaw = atan2(dir.x, dir.z) * 180.0f / 3.14159f;
        glRotatef(yaw + 180.0f, 0, 1, 0);

        float roll = sin(ringTimer * boat.bobSpeed * 0.8f + boat.phase) * 2.0f;
        glRotatef(roll, 0, 0, 1);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex_boat);
        glColor3f(1.0f, 1.0f, 1.0f);
        glScalef(10.5f, 10.5f, 10.5f);
        model_boat.Draw();
        glPopMatrix();

        // Wake and foam around hull
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Foam ring around hull
        float foamAlpha = boat.isMoving ? 0.5f : 0.55f;
        float foamRadius = boat.isMoving ? 18.0f : 20.0f;
        glColor4f(0.95f, 0.98f, 1.0f, foamAlpha);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(boat.position.x, waterLevel + 0.5f, boat.position.z);
        for (int i = 0; i <= 24; i++) {
            float angle = (float)i / 24.0f * 6.28318f;
            float wave = sin(angle * 3.0f + ringTimer * 2.0f + boat.phase) * 1.8f;
            glVertex3f(
                boat.position.x + cos(angle) * (foamRadius + wave),
                waterLevel + 0.5f,
                boat.position.z + sin(angle) * (foamRadius + wave)
            );
        }
        glEnd();

        // Wake effects for moving boats
        if (boat.isMoving && boat.pathLength > 0.001f) {
            Vector3f dirNorm = boat.forwardDir * (boat.movingForward ? 1.0f : -1.0f);
            dirNorm = dirNorm.unit();
            Vector3f right(dirNorm.z, 0.0f, -dirNorm.x);
            float rightLen = sqrt(right.x * right.x + right.z * right.z);
            if (rightLen > 0.0001f) {
                right = right * (1.0f / rightLen);

                // Bow wave (white foam at front)
                Vector3f bowPos = boat.position + dirNorm * 12.0f;
                glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
                glBegin(GL_TRIANGLE_FAN);
                glVertex3f(bowPos.x, waterLevel + 0.6f, bowPos.z);
                for (int i = 0; i <= 12; i++) {
                    float angle = (float)i / 12.0f * 3.14159f - 1.5708f;
                    float r = 6.0f + sin(ringTimer * 4.0f + (float)i) * 1.0f;
                    glColor4f(1.0f, 1.0f, 1.0f, 0.5f - (float)i * 0.03f);
                    glVertex3f(
                        bowPos.x + cos(angle) * r * dirNorm.x + sin(angle) * r * right.x,
                        waterLevel + 0.55f,
                        bowPos.z + cos(angle) * r * dirNorm.z + sin(angle) * r * right.z
                    );
                }
                glEnd();

                // V-shaped wake trail (left arm)
                float wakeLen = 60.0f + boat.moveSpeed * 1.5f;
                float spreadAngle = 0.35f;
                Vector3f sternPos = boat.position - dirNorm * 10.0f;

                glBegin(GL_QUAD_STRIP);
                for (int s = 0; s <= 10; s++) {
                    float t = (float)s / 10.0f;
                    float dist = t * wakeLen;
                    float width = 2.0f + t * 8.0f;
                    float alpha = 0.5f * (1.0f - t);
                    float waveOff = sin(ringTimer * 3.0f + t * 8.0f) * 0.8f;

                    Vector3f leftDir = dirNorm * (-1.0f) + right * spreadAngle;
                    float leftLen = sqrt(leftDir.x * leftDir.x + leftDir.z * leftDir.z);
                    leftDir = leftDir * (1.0f / leftLen);

                    Vector3f pos = sternPos + leftDir * dist;
                    glColor4f(1.0f, 1.0f, 1.0f, alpha);
                    glVertex3f(pos.x - right.x * width + waveOff, waterLevel + 0.4f, pos.z - right.z * width);
                    glVertex3f(pos.x + right.x * width + waveOff, waterLevel + 0.4f, pos.z + right.z * width);
                }
                glEnd();

                // V-shaped wake trail (right arm)
                glBegin(GL_QUAD_STRIP);
                for (int s = 0; s <= 10; s++) {
                    float t = (float)s / 10.0f;
                    float dist = t * wakeLen;
                    float width = 2.0f + t * 8.0f;
                    float alpha = 0.5f * (1.0f - t);
                    float waveOff = sin(ringTimer * 3.0f + t * 8.0f + 1.5f) * 0.8f;

                    Vector3f rightDir = dirNorm * (-1.0f) - right * spreadAngle;
                    float rightDirLen = sqrt(rightDir.x * rightDir.x + rightDir.z * rightDir.z);
                    rightDir = rightDir * (1.0f / rightDirLen);

                    Vector3f pos = sternPos + rightDir * dist;
                    glColor4f(1.0f, 1.0f, 1.0f, alpha);
                    glVertex3f(pos.x - right.x * width + waveOff, waterLevel + 0.4f, pos.z - right.z * width);
                    glVertex3f(pos.x + right.x * width + waveOff, waterLevel + 0.4f, pos.z + right.z * width);
                }
                glEnd();

                // Center turbulence strip
                glBegin(GL_QUAD_STRIP);
                for (int s = 0; s <= 12; s++) {
                    float t = (float)s / 12.0f;
                    float dist = t * wakeLen * 0.7f;
                    float width = 3.0f + t * 4.0f;
                    float alpha = 0.4f * (1.0f - t * 0.8f);
                    float waveOff = sin(ringTimer * 5.0f + t * 10.0f) * 1.2f;

                    Vector3f pos = sternPos - dirNorm * dist;
                    glColor4f(0.9f, 0.95f, 1.0f, alpha);
                    glVertex3f(pos.x - right.x * width, waterLevel + 0.45f + waveOff * 0.1f, pos.z - right.z * width);
                    glVertex3f(pos.x + right.x * width, waterLevel + 0.45f + waveOff * 0.1f, pos.z + right.z * width);
                }
                glEnd();
            }
        }

        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
    }
}

void Level1::renderPort() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_concrete);
    glDisable(GL_LIGHTING);
    
    // Make port much larger and more extensive
    float size = 1500.0f;  // Doubled from 800 to 1500
    float portX = 450.0f;  // Moved slightly further right
    float portDepth = 600.0f;  // How far the port extends
    
    // Main port concrete area - larger perimeter
    glColor3f(0.6f, 0.6f, 0.65f);
    glBegin(GL_QUADS);
    float texScale = 0.01f;
    
    glTexCoord2f(0, 0);
    glVertex3f(portX, portHeight, -size);
    glTexCoord2f(portDepth * texScale, 0);
    glVertex3f(portX + portDepth, portHeight, -size);
    glTexCoord2f(portDepth * texScale, size * texScale * 2);
    glVertex3f(portX + portDepth, portHeight, size);
    glTexCoord2f(0, size * texScale * 2);
    glVertex3f(portX, portHeight, size);
    glEnd();
    
    // Port edge wall (between water and port) - taller and more defined
    glColor3f(0.35f, 0.35f, 0.4f);
    glBegin(GL_QUADS);
    glVertex3f(portX, waterLevel, -size);
    glVertex3f(portX, portHeight + 2.0f, -size);
    glVertex3f(portX, portHeight + 2.0f, size);
    glVertex3f(portX, waterLevel, size);
    glEnd();
    
    // Add concrete pylons/pillars along the edge
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.4f, 0.4f, 0.45f);
    for (float z = -size + 50.0f; z < size; z += 100.0f) {
        glPushMatrix();
        glTranslatef(portX, waterLevel + (portHeight - waterLevel) / 2, z);
        glScalef(8.0f, portHeight - waterLevel + 2.0f, 8.0f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
    
    glEnable(GL_LIGHTING);
    
    // Render port cranes at strategic positions
    glPushMatrix();
    glColor3f(0.8f, 0.7f, 0.1f);  // Yellow crane color
    
    // Crane 1 - Near front of port
    glPushMatrix();
    glTranslatef(portX + 19.0f, portHeight, -400.0f);
    //glRotatef(45.0f, 0, 1, 0);
    glScalef(0.0015f, 0.0015f, 0.0015f);
    model_crane.Draw();
    glPopMatrix();
    
    // Crane 2 - Middle section
    glPushMatrix();
    glTranslatef(portX + 19.0f, portHeight, 0.0f);
    //glRotatef(-30.0f, 0, 1, 0);
    glScalef(0.0015f, 0.0015f, 0.0015f);
    model_crane.Draw();
    glPopMatrix();
    
    // Crane 3 - Back section
    glPushMatrix();
    glTranslatef(portX + 19.0f, portHeight, 450.0f);
    glRotatef(90.0f, 0, 1, 0);
    glScalef(0.0015f, 0.0015f, 0.0015f);
    model_crane.Draw();
    glPopMatrix();
    
    glPopMatrix();
    
    // Render shipping containers in organized stacks
    glPushMatrix();
    
    // Container yard 1 - organized rows
    float containerBaseX = portX + 300.0f;
    float containerBaseZ = -600.0f;
    
    // Define fixed stack heights per position (no randomness to avoid floating)
    int stackHeights1[4][6] = {
        {2, 3, 2, 1, 2, 3},
        {1, 2, 3, 2, 1, 2},
        {3, 1, 2, 3, 2, 1},
        {2, 2, 1, 2, 3, 2}
    };
    
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 6; col++) {
            int stackHeight = stackHeights1[row][col];
            
            for (int height = 0; height < stackHeight; height++) {
                glPushMatrix();
                glTranslatef(
                    containerBaseX + row * 35.0f,
                    portHeight + 0.5f + height * 7.5f,  // Lower to ground level
                    containerBaseZ + col * 40.0f
                );
                glRotatef(90.0f * (row % 2), 0, 1, 0);  // Alternate rotation
                
                // Draw textured box instead of 3DS model to ensure textures work
                glEnable(GL_TEXTURE_2D);
                
                // Select texture based on color variation
                if ((row + col + height) % 3 == 0)
                    glBindTexture(GL_TEXTURE_2D, tex_container_red);
                else if ((row + col + height) % 3 == 1)
                    glBindTexture(GL_TEXTURE_2D, tex_container_blue);
                else
                    glBindTexture(GL_TEXTURE_2D, tex_container_yellow);
                
                // Draw shipping container as textured box (20ft container proportions)
                float containerLength = 2*9.0f;
                float containerWidth = 2*3.6f;
                float containerHeight = 2*3.9f;
                
                glBegin(GL_QUADS);
                glColor3f(1.0f, 1.0f, 1.0f);
                
                // Front face
                glNormal3f(0.0f, 0.0f, 1.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, containerWidth/2);
                
                // Back face
                glNormal3f(0.0f, 0.0f, -1.0f);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, -containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, -containerWidth/2);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, -containerWidth/2);
                
                // Left face
                glNormal3f(-1.0f, 0.0f, 0.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, -containerWidth/2);
                
                // Right face
                glNormal3f(1.0f, 0.0f, 0.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, -containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, containerWidth/2);
                
                // Top face
                glNormal3f(0.0f, 1.0f, 0.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(-containerLength/2, containerHeight, containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(containerLength/2, containerHeight, containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, -containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, -containerWidth/2);
                
                // Bottom face
                glNormal3f(0.0f, -1.0f, 0.0f);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(-containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(containerLength/2, 0.0f, containerWidth/2);
                
                glEnd();
                glDisable(GL_TEXTURE_2D);
                glPopMatrix();
            }
        }
    }
    
    // Container yard 2 - second area
    containerBaseZ = 200.0f;
    int stackHeights2[3][5] = {
        {2, 1, 2, 1, 2},
        {1, 2, 1, 2, 1},
        {2, 1, 2, 2, 1}
    };
    
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 5; col++) {
            int stackHeight = stackHeights2[row][col];
            
            for (int height = 0; height < stackHeight; height++) {
                glPushMatrix();
                glTranslatef(
                    containerBaseX + row * 35.0f,
                    portHeight + 0.5f + height * 7.5f,  // Lower to ground level
                    containerBaseZ + col * 40.0f
                );
                glRotatef(90.0f * ((row + 1) % 2), 0, 1, 0);
                
                // Draw textured box instead of 3DS model to ensure textures work
                glEnable(GL_TEXTURE_2D);
                
                // Select texture based on color variation
                if ((row + col + height) % 3 == 0)
                    glBindTexture(GL_TEXTURE_2D, tex_container_yellow);
                else if ((row + col + height) % 3 == 1)
                    glBindTexture(GL_TEXTURE_2D, tex_container_red);
                else
                    glBindTexture(GL_TEXTURE_2D, tex_container_blue);
                
                // Draw shipping container as textured box (20ft container proportions)
                float containerLength = 2*9.0f;
                float containerWidth = 2*3.6f;
                float containerHeight = 2*3.9f;
                
                glBegin(GL_QUADS);
                glColor3f(1.0f, 1.0f, 1.0f);
                
                // Front face
                glNormal3f(0.0f, 0.0f, 1.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, containerWidth/2);
                
                // Back face
                glNormal3f(0.0f, 0.0f, -1.0f);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, -containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, -containerWidth/2);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, -containerWidth/2);
                
                // Left face
                glNormal3f(-1.0f, 0.0f, 0.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, -containerWidth/2);
                
                // Right face
                glNormal3f(1.0f, 0.0f, 0.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, -containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, containerWidth/2);
                
                // Top face
                glNormal3f(0.0f, 1.0f, 0.0f);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(-containerLength/2, containerHeight, containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(containerLength/2, containerHeight, containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(containerLength/2, containerHeight, -containerWidth/2);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(-containerLength/2, containerHeight, -containerWidth/2);
                
                // Bottom face
                glNormal3f(0.0f, -1.0f, 0.0f);
                glTexCoord2f(0.0f, 1.0f); glVertex3f(-containerLength/2, 0.0f, containerWidth/2);
                glTexCoord2f(0.0f, 0.0f); glVertex3f(-containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 0.0f); glVertex3f(containerLength/2, 0.0f, -containerWidth/2);
                glTexCoord2f(1.0f, 1.0f); glVertex3f(containerLength/2, 0.0f, containerWidth/2);
                
                glEnd();
                glDisable(GL_TEXTURE_2D);
                glPopMatrix();
            }
        }
    }
    
    glPopMatrix();
    
    // Render helipad on the port
    glPushMatrix();
    glColor3f(1.0f, 1.0f, 1.0f);
    glTranslatef(portX + 150.0f, portHeight + 0.1f, -700.0f);
    glRotatef(-90.0f, 0, 1, 0);
    glScalef(0.8f, 0.8f, 0.8f);  // Increased from 0.5f
    model_helipad.Draw();
    glPopMatrix();
    
    // Render tents near the port edge
    // Tent 1
    glPushMatrix();
    glColor3f(1.0f, 1.0f, 1.0f);
    glTranslatef(portX + 100.0f, portHeight + 0.1f, -200.0f);
    glRotatef(180.0f, 0, 1, 0);
    glScalef(3.0f, 3.0f, 3.0f);  // Increased from 2.0f
    model_tents.Draw();
    glPopMatrix();
    
    // Tent 2
    glPushMatrix();
    glColor3f(1.0f, 1.0f, 1.0f);
    glTranslatef(portX + 100.0f, portHeight + 0.1f, 100.0f);
    glRotatef(180.0f, 0, 1, 0);
    glScalef(3.0f, 3.0f, 3.0f);  // Increased from 2.0f
    model_tents.Draw();
    glPopMatrix();
    
    // Tent 3
    glPushMatrix();
    glColor3f(1.0f, 1.0f, 1.0f);
    glTranslatef(portX + 100.0f, portHeight + 0.1f, 400.0f);
    glRotatef(180.0f, 0, 1, 0);
    glScalef(3.0f, 3.0f, 3.0f);  // Increased from 2.0f
    model_tents.Draw();
    glPopMatrix();
    
    // Render trucks on the port
    // Truck 1
    glPushMatrix();
    glColor3f(1.0f, 1.0f, 1.0f);
    glTranslatef(portX + 200.0f, portHeight + 0.1f, -300.0f);
    glRotatef(0.0f, 0, 1, 0);
    glScalef(0.1f, 0.1f, 0.1f);
    model_truck.Draw();
    glPopMatrix();
    
    // Truck 2
    glPushMatrix();
    glColor3f(1.0f, 1.0f, 1.0f);
    glTranslatef(portX + 200.0f, portHeight + 0.1f, 200.0f);
    glRotatef(180.0f, 0, 1, 0);
    glScalef(0.1f, 0.1f, 0.1f);
    model_truck.Draw();
    glPopMatrix();

    // Render humvees on the port
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_humvee);
    glColor3f(1.0f, 1.0f, 1.0f);

    // Humvee 1 - near helipad
    glPushMatrix();
    glTranslatef(portX + 120.0f, portHeight + 0.1f, -650.0f);
    glRotatef(45.0f, 0, 1, 0);
    glScalef(0.08f, 0.08f, 0.08f);
    model_humvee.Draw();
    glPopMatrix();

    // Humvee 2 - near helipad
    glPushMatrix();
    glTranslatef(portX + 140.0f, portHeight + 0.1f, -620.0f);
    glRotatef(30.0f, 0, 1, 0);
    glScalef(0.08f, 0.08f, 0.08f);
    model_humvee.Draw();
    glPopMatrix();

    // Humvee 3 - near tents
    glPushMatrix();
    glTranslatef(portX + 80.0f, portHeight + 0.1f, -150.0f);
    glRotatef(-60.0f, 0, 1, 0);
    glScalef(0.08f, 0.08f, 0.08f);
    model_humvee.Draw();
    glPopMatrix();

    // Humvee 4 - near container yard
    glPushMatrix();
    glTranslatef(portX + 250.0f, portHeight + 0.1f, -500.0f);
    glRotatef(90.0f, 0, 1, 0);
    glScalef(0.08f, 0.08f, 0.08f);
    model_humvee.Draw();
    glPopMatrix();

    // Humvee 5 - patrol near edge
    glPushMatrix();
    glTranslatef(portX + 50.0f, portHeight + 0.1f, 50.0f);
    glRotatef(0.0f, 0, 1, 0);
    glScalef(0.08f, 0.08f, 0.08f);
    model_humvee.Draw();
    glPopMatrix();

    // Humvee 6 - near second tent area
    glPushMatrix();
    glTranslatef(portX + 85.0f, portHeight + 0.1f, 350.0f);
    glRotatef(120.0f, 0, 1, 0);
    glScalef(0.08f, 0.08f, 0.08f);
    model_humvee.Draw();
    glPopMatrix();

    // Leave texture/lighting state enabled for subsequent textured objects
    
    // Draw Lighthouse (Light Animation Source)
    drawLighthouse();
}

void Level1::renderCarrier() {
    // 1. Draw the Model (Scaled) with material properties
    glPushMatrix();
    glTranslatef(carrierPosition.x, carrierPosition.y, carrierPosition.z);
    glRotatef(carrierRotation, 0, 1, 0);
    glScalef(carrierScale, carrierScale, carrierScale);
    
    // Set material properties for metal carrier
    GLfloat matAmbient[] = { 0.3f, 0.3f, 0.35f, 1.0f };
    GLfloat matDiffuse[] = { 0.5f, 0.5f, 0.55f, 1.0f };
    GLfloat matSpecular[] = { 0.6f, 0.6f, 0.65f, 1.0f };
    GLfloat matShininess = 25.0f;  // Moderate shine for painted metal
    glMaterialfv(GL_FRONT, GL_AMBIENT, matAmbient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, matDiffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpecular);
    glMaterialf(GL_FRONT, GL_SHININESS, matShininess);
    
    glColor3f(0.5f, 0.5f, 0.55f);
    model_carrier.Draw();
    glPopMatrix();
    
    // 2. Draw the Deck Primitive (Unscaled, World Units)
    glPushMatrix();
    glTranslatef(carrierPosition.x, carrierPosition.y + 3.0f, carrierPosition.z); // +3.0f height
    glRotatef(carrierRotation, 0, 1, 0);
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_carrier);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    
    // Draw textured deck surface (Runway style)
    float deckWidth = 25.0f;
    float deckLength = 120.0f;
    
    // Main deck surface
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-deckWidth, 0.0f, -deckLength);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(deckWidth, 0.0f, -deckLength);
    glTexCoord2f(1.0f, 5.0f); glVertex3f(deckWidth, 0.0f, deckLength);
    glTexCoord2f(0.0f, 5.0f); glVertex3f(-deckWidth, 0.0f, deckLength);
    glEnd();
    
    // Markings (White lines)
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Centerline dashes
    float dashLength = 10.0f;
    float dashGap = 10.0f;
    float dashWidth = 1.0f;
    
    for (float z = -deckLength + 10.0f; z < deckLength - 10.0f; z += (dashLength + dashGap)) {
        glBegin(GL_QUADS);
        glVertex3f(-dashWidth/2, 0.1f, z);
        glVertex3f(dashWidth/2, 0.1f, z);
        glVertex3f(dashWidth/2, 0.1f, z + dashLength);
        glVertex3f(-dashWidth/2, 0.1f, z + dashLength);
        glEnd();
    }
    
    // Edge lines
    float edgeWidth = 1.0f;
    float edgeOffset = deckWidth - 2.0f;
    
    // Left edge
    glBegin(GL_QUADS);
    glVertex3f(-edgeOffset - edgeWidth, 0.1f, -deckLength);
    glVertex3f(-edgeOffset, 0.1f, -deckLength);
    glVertex3f(-edgeOffset, 0.1f, deckLength);
    glVertex3f(-edgeOffset - edgeWidth, 0.1f, deckLength);
    glEnd();
    
    // Right edge
    glBegin(GL_QUADS);
    glVertex3f(edgeOffset, 0.1f, -deckLength);
    glVertex3f(edgeOffset + edgeWidth, 0.1f, -deckLength);
    glVertex3f(edgeOffset + edgeWidth, 0.1f, deckLength);
    glVertex3f(edgeOffset, 0.1f, deckLength);
    glEnd();
    
    glEnable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glPopMatrix();
}
    // glTranslatef... glRotatef...
    // glPushMatrix(); // Save unscaled
    // glScalef...
    // model.Draw();
    // glPopMatrix(); // Restore unscaled
    // ... primitive ...
    // glPopMatrix(); // Restore original
    
    // This is cleaner.
    
    // Let's find the start of renderCarrier.


void Level1::renderRings() {
    for (const auto& ring : rings) {
        renderRing(ring);
    }
}

void Level1::renderRing(const Ring& ring) {
    glPushMatrix();
    glTranslatef(ring.position.x, ring.position.y, ring.position.z);
    glRotatef(ring.rotationAngle, 0, 0, 1);  // Rotate around forward axis
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_rings);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Find the next ring that needs to be passed (first unpassed ring in order)
    int nextRingIndex = -1;
    for (int i = 0; i < (int)rings.size(); i++) {
        if (!rings[i].passed) {
            nextRingIndex = i;
            break;
        }
    }
    
    // Determine if this is the next ring to pass through
    bool isNextRing = false;
    if (nextRingIndex != -1) {
        // Check if this ring matches the next ring position (by comparing position)
        const Ring& nextRing = rings[nextRingIndex];
        if (ring.position.x == nextRing.position.x && 
            ring.position.y == nextRing.position.y && 
            ring.position.z == nextRing.position.z) {
            isNextRing = true;
        }
    }
    
    // Ring color
    if (ring.passed) {
        glColor4f(0.3f, 0.3f, 0.3f, 0.5f);  // Dim gray for passed rings
    } else if (ring.isGolden && isNextRing) {
        // Golden ring when it's the next one to pass - pulsing glow
        float pulse = 0.7f + 0.3f * sin(ringTimer * 3.0f);
        glColor4f(1.0f * pulse, 0.84f * pulse, 0.0f, 0.9f);
    } else if (isNextRing) {
        // Next ring to pass - bright blue
        float pulse = 0.8f + 0.2f * sin(ringTimer * 4.0f);
        glColor4f(0.0f, 0.6f * pulse, 1.0f * pulse, 0.9f);
    } else {
        // Other unpassed rings - grey
        glColor4f(0.5f, 0.5f, 0.5f, 0.6f);
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
    
    // Inner glow for next ring to pass
    if (!ring.passed && isNextRing) {
        if (ring.isGolden) {
            glColor4f(1.0f, 0.9f, 0.3f, 0.3f);
            glutSolidSphere(ring.radius * 0.8f, 16, 16);
        } 
        
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
    glEnable(GL_LIGHTING);
    
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

        // Align model forward (3ds likely +X) to world +Z (direction of travel)
        glRotatef(-90.0f, 0, 1, 0);
        
        // Render 3D rocket model
        glColor3f(1.0f, 1.0f, 1.0f);
        glScalef(0.15f, 0.15f, 0.15f); // Reduced from 0.25f
        if (tex_rocket != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tex_rocket);
        }
        model_rocket.Draw();
        
        glPopMatrix();
        
        // Render smoke trail behind rocket
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_TEXTURE_2D);
        
        // Create smoke particles trailing behind the rocket
        Vector3f smokeOffset = dir * -4.5f;  // Offset behind rocket (further back)
        for (int i = 0; i < 5; i++) {
            float offset = (float)i * 0.8f;
            Vector3f smokePos = rocket.position + smokeOffset * offset;
            
            glPushMatrix();
            glTranslatef(smokePos.x, smokePos.y, smokePos.z);
            
            // Face the camera
            glRotatef(-yaw, 0, 1, 0);
            glRotatef(-pitch, 1, 0, 0);
            
            // Smoke color - gets lighter and more transparent with distance
            float alpha = 0.5f - (float)i * 0.08f;
            float size = 1.0f + (float)i * 0.3f;
            glColor4f(0.6f, 0.6f, 0.6f, alpha);
            
            // Draw smoke particle as a quad
            glBegin(GL_QUADS);
            glVertex3f(-size, -size, 0);
            glVertex3f(size, -size, 0);
            glVertex3f(size, size, 0);
            glVertex3f(-size, size, 0);
            glEnd();
            
            glPopMatrix();
        }
        
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }
}

void Level1::renderShadows() {
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
    
    // Render shadows for rings
    for (size_t i = 0; i < rings.size(); i++) {
        if (rings[i].passed) continue;
        
        Vector3f ringPos = rings[i].position;
        float height = ringPos.y;
        
        shadowSystem.renderBlobShadow(ringPos, rings[i].radius * 0.5f, height, 120.0f);
    }
    
    // Render shadows for toolkits
    for (size_t i = 0; i < toolkits.size(); i++) {
        if (toolkits[i].collected) continue;
        
        float bobHeight = sin(toolkits[i].bobOffset) * 3.0f;
        Vector3f toolkitPos = toolkits[i].position;
        float height = toolkitPos.y + bobHeight;
        
        shadowSystem.renderBlobShadow(toolkitPos, 2.5f, height, 120.0f);
    }
    
    // Render shadows for rockets
    for (size_t i = 0; i < rockets.size(); i++) {
        if (!rockets[i].active) continue;
        
        Vector3f rocketPos = rockets[i].position;
        float height = rocketPos.y;
        
        shadowSystem.renderBlobShadow(rocketPos, 2.0f, height, 100.0f);
    }
    
    // Render ambient occlusion at carrier base
    shadowSystem.renderBaseAO(carrierPosition, 50.0f, 0.4f);
    
    // Render ambient occlusion at crane bases
    float portX = 450.0f;
    shadowSystem.renderBaseAO(Vector3f(portX + 19.0f, portHeight, -400.0f), 15.0f, 0.35f);
    shadowSystem.renderBaseAO(Vector3f(portX + 19.0f, portHeight, 0.0f), 15.0f, 0.35f);
    shadowSystem.renderBaseAO(Vector3f(portX + 19.0f, portHeight, 450.0f), 15.0f, 0.35f);
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
    if (key == 27) {  // ESC - open options menu
        GameManager::getInstance().switchToLevel("options");
        return;
    }
    
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
        initBoats();
        shootingSystem.reset();  // Reset shooting system
        
        // Reset plane - start on carrier deck, stationary
        if (flightSim) {
            flightSim->player.position = Vector3f(carrierPosition.x, carrierPosition.y + 3.0f, carrierPosition.z - 25.0f);
            flightSim->player.velocity = Vector3f(0, 0, 0);  // Stationary
            flightSim->player.forward = Vector3f(0, 0, 1);  // Facing forward along carrier
            flightSim->player.up = Vector3f(0, 1, 0);
            flightSim->player.right = Vector3f(1, 0, 0);
            flightSim->player.throttle = 0.0f;  // Engine off
            flightSim->isGrounded = true;  // Start on deck
            flightSim->isCrashed = false;
        }
        
        // Reset spawn protection
        spawnProtectionTimer = 3.0f;
        hasSpawnProtection = true;
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
    // IMPORTANT: Ensure depth test is enabled when entering level
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    // Reload the selected plane model and texture to support "Change Plane" from menus
    if (flightSim) {
        flightSim->modelLoaded = false; // Force reload even if a model is already loaded
        int selectedPlane = PlaneSelectionLevel::getSelectedPlane();
        if (selectedPlane == 1) {
             flightSim->loadModelWithTexture("Models/plane 2/plane2.3ds", "Models/plane 2/Textures/Color.bmp");
        } else if (selectedPlane == 2) {
             flightSim->loadModelWithTexture("Models/plane 3/plane 3.3ds", "");
        } else {
             flightSim->loadModelWithTexture("models/plane/mitsubishi_a6m2_zero_model_11.3ds", "models/plane/mitsubishi_a6m2_zero_texture.bmp");
        }
    }

    screenWidth = glutGet(GLUT_WINDOW_WIDTH);
    screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    glutSetCursor(GLUT_CURSOR_NONE);
    // Reset spawn protection when entering/re-entering level
    spawnProtectionTimer = 3.0f;
    hasSpawnProtection = true;
    
    // Reload plane model based on current selection
    if (flightSim) {
        int selectedPlane = PlaneSelectionLevel::getSelectedPlane();
        printf("Level1 loading plane %d on enter\n", selectedPlane);
        flightSim->modelLoaded = false;  // allow reload
        if (selectedPlane == 1) {
            flightSim->loadModelWithTexture("Models/plane 2/plane2.3ds", "Models/plane 2/Textures/Color.bmp");
        } else if (selectedPlane == 2) {
            flightSim->loadModelWithTexture("Models/plane 3/plane 3.3ds", "");
        } else {
            flightSim->loadModelWithTexture("models/plane/mitsubishi_a6m2_zero_model_11.3ds", "models/plane/mitsubishi_a6m2_zero_texture.bmp");
        }
        flightSim->isCrashed = false;
    }
}

void Level1::onExit() {
    active = false;
    glutSetCursor(GLUT_CURSOR_INHERIT);
    soundSystem.stopEngineSound();
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

void Level1::drawLighthouse() {
    float x = 500.0f;
    float z = 0.0f;
    float baseY = portHeight;
    
    glPushMatrix();
    glTranslatef(x, baseY, z);
    glRotatef(-90.0f, 1, 0, 0); // Upright cylinder
    
    glEnable(GL_TEXTURE_2D);
    
    // Base/Wall
    glBindTexture(GL_TEXTURE_2D, tex_lighthouse_wall);
    glColor3f(1.0f, 1.0f, 1.0f);
    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluCylinder(quad, 6.0f, 4.0f, 55.0f, 32, 1); // Tapered cylinder
    
    // Top Platform (Red)
    glTranslatef(0.0f, 0.0f, 55.0f);
    glBindTexture(GL_TEXTURE_2D, tex_lighthouse_top);
    // Platform base
    gluDisk(quad, 0.0f, 7.0f, 32, 1);
    
    // Lantern room
    gluCylinder(quad, 4.0f, 4.0f, 8.0f, 32, 1);
    
    // === LIGHT BULB (Emissive Sphere) ===
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 4.0f); // Center of lantern room
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING); // Make it self-illuminated
    glColor3f(1.0f, 1.0f, 0.8f); // Bright yellow-white
    glutSolidSphere(2.0f, 16, 16);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
    
    // === LIGHT BEAM (Visual cone) ===
    // Only draw at night
    if (skySystem.isNightTime()) {
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 4.0f); // Center of lantern room
        
        // Rotate beam to match GL_LIGHT1 rotation
        float time = ringTimer * 2.0f;
        float angle = -time * 180.0f / 3.14159f; // Convert rad to deg (negative for direction match)
        glRotatef(angle, 0, 0, 1); // Rotate around local Z (which is Up for the cylinder)
        
        // Draw beam
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending
        
        glColor4f(1.0f, 0.9f, 0.7f, 0.3f); // Semi-transparent yellow beam
        
        // Beam cone
        glPushMatrix();
        glRotatef(90.0f, 0, 1, 0); // Point outwards
        gluCylinder(quad, 0.5f, 15.0f, 100.0f, 16, 1); // Expand from 0.5 to 15 width, length 100
        glPopMatrix();
        
        // Restore state
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glPopMatrix();
    }
    
    // Roof
    glTranslatef(0.0f, 0.0f, 8.0f);
    glBindTexture(GL_TEXTURE_2D, tex_lighthouse_top); // Reuse red texture
    gluCylinder(quad, 5.0f, 0.0f, 5.0f, 32, 1); // Cone
    
    gluDeleteQuadric(quad);
    glPopMatrix();
}


