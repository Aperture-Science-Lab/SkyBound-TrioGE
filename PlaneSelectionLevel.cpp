#include "PlaneSelectionLevel.h"
#include "GameManager.h"
#include <glut.h>
#include <stdio.h>
#include <cmath>

// External texture loading function
extern void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

// Static member initialization
int PlaneSelectionLevel::selectedPlane = 0;

PlaneSelectionLevel::PlaneSelectionLevel() 
        : Level(), highlightedPlane(0), rotationAngle(0.0f), pulseTimer(0.0f),
            screenWidth(1280), screenHeight(720), tex_plane1(0), tex_plane2(0), tex_plane3(0) {
}

PlaneSelectionLevel::~PlaneSelectionLevel() {
    cleanup();
}

void PlaneSelectionLevel::init() {
    printf("Initializing Plane Selection Level...\n");
    glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    
    // Load plane 1 model (new model with geometry)
    model_plane1.Load("models/plane/mitsubishi_a6m2_zero_model_11.3ds");
    printf("Loaded plane 1 model (objects: %d, materials: %d)\n", model_plane1.numObjects, model_plane1.numMaterials);

    // Fallback to alternate model if primary has no geometry
    if (model_plane1.numObjects == 0) {
        printf("Plane 1 primary model empty, loading fallback a6m2_geo.3ds...\n");
        model_plane1.Load("models/plane/a6m2_geo.3ds");
        printf("Fallback plane 1 model (objects: %d, materials: %d)\n", model_plane1.numObjects, model_plane1.numMaterials);
    }

    // Skip external BMP load to avoid DIB incompatibility; use model-embedded materials/textures if present
    
    // Load plane 2 model (will use embedded materials)
    model_plane2.Load("Models/plane 2/plane2.3ds");
    printf("Loaded plane 2 model (objects: %d, materials: %d)\n", model_plane2.numObjects, model_plane2.numMaterials);

    // Load plane 3 model
    model_plane3.Load("Models/plane 3/plane 3.3ds");
    printf("Loaded plane 3 model (objects: %d, materials: %d)\n", model_plane3.numObjects, model_plane3.numMaterials);

    // Skip external BMP load to avoid DIB incompatibility; rely on embedded materials
    
    // Let models use their own textures
    // No need to manually load textures here since the 3DS loader handles it
    
    highlightedPlane = selectedPlane;
}

void PlaneSelectionLevel::update(float deltaTime) {
    rotationAngle += deltaTime * 30.0f;  // Slow rotation
    if (rotationAngle > 360.0f) rotationAngle -= 360.0f;
    
    pulseTimer += deltaTime * 2.0f;
}

void PlaneSelectionLevel::render() {
    // CRITICAL: Don't render if this level is not active
    if (!active) {
        return;
    }
    
    // Sync viewport with current window size
    int winW = glutGet(GLUT_WINDOW_WIDTH);
    int winH = glutGet(GLUT_WINDOW_HEIGHT);
    if (winW > 0 && winH > 0) {
        screenWidth = winW;
        screenHeight = winH;
    }
    glViewport(0, 0, screenWidth, screenHeight);

    // Clear with gradient-like dark blue background
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(0.08f, 0.12f, 0.22f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 2D UI pass
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, screenWidth, 0, screenHeight);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Draw background gradient panels
    glBegin(GL_QUADS);
    glColor4f(0.1f, 0.15f, 0.25f, 1.0f);
    glVertex2f(0, 0);
    glVertex2f(screenWidth, 0);
    glColor4f(0.05f, 0.08f, 0.15f, 1.0f);
    glVertex2f(screenWidth, screenHeight);
    glVertex2f(0, screenHeight);
    glEnd();

    // Card layout for 3 planes
    float cardWidth = 220.0f;
    float cardHeight = 280.0f;
    float gap = 40.0f;
    float totalWidth = cardWidth * 3 + gap * 2;
    float startX = (screenWidth - totalWidth) * 0.5f;
    float cardY = screenHeight * 0.35f;

    // Plane names and colors
    const char* planeNames[3] = {"ZERO FIGHTER", "INTERCEPTOR", "STEALTH JET"};
    const char* planeDesc[3] = {"Classic WWII fighter", "Modern combat aircraft", "Advanced stealth plane"};
    const float cardColors[3][3] = {
        {0.15f, 0.25f, 0.45f},  // Blue
        {0.15f, 0.40f, 0.30f},  // Green
        {0.35f, 0.20f, 0.40f}   // Purple
    };

    // Draw cards
    for (int i = 0; i < 3; i++) {
        float cardX = startX + i * (cardWidth + gap);
        bool selected = (highlightedPlane == i);
        float scale = selected ? 1.08f : 1.0f;
        float pulse = selected ? (0.15f * sin(pulseTimer * 3.0f)) : 0.0f;
        
        float actualWidth = cardWidth * scale;
        float actualHeight = cardHeight * scale;
        float offsetX = (actualWidth - cardWidth) * 0.5f;
        float offsetY = (actualHeight - cardHeight) * 0.5f;
        float x = cardX - offsetX;
        float y = cardY - offsetY;

        // Card shadow
        glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
        glBegin(GL_QUADS);
        glVertex2f(x + 8, y - 8);
        glVertex2f(x + actualWidth + 8, y - 8);
        glVertex2f(x + actualWidth + 8, y + actualHeight - 8);
        glVertex2f(x + 8, y + actualHeight - 8);
        glEnd();

        // Card background with glow if selected
        if (selected) {
            glColor4f(cardColors[i][0] + pulse + 0.3f, cardColors[i][1] + pulse + 0.3f, cardColors[i][2] + pulse + 0.3f, 0.3f);
            glBegin(GL_QUADS);
            glVertex2f(x - 10, y - 10);
            glVertex2f(x + actualWidth + 10, y - 10);
            glVertex2f(x + actualWidth + 10, y + actualHeight + 10);
            glVertex2f(x - 10, y + actualHeight + 10);
            glEnd();
        }

        // Card body
        glColor4f(cardColors[i][0] + pulse, cardColors[i][1] + pulse, cardColors[i][2] + pulse, 0.95f);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + actualWidth, y);
        glVertex2f(x + actualWidth, y + actualHeight);
        glVertex2f(x, y + actualHeight);
        glEnd();

        // Card border
        if (selected) {
            glColor4f(1.0f, 0.9f, 0.4f, 1.0f);
            glLineWidth(4.0f);
        } else {
            glColor4f(0.5f, 0.5f, 0.6f, 0.8f);
            glLineWidth(2.0f);
        }
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + actualWidth, y);
        glVertex2f(x + actualWidth, y + actualHeight);
        glVertex2f(x, y + actualHeight);
        glEnd();
        glLineWidth(1.0f);

        // Plane number indicator
        glColor4f(1.0f, 1.0f, 1.0f, 0.9f);
        char numStr[16];
        sprintf_s(numStr, sizeof(numStr), "%d", i + 1);
        glRasterPos2f(x + 15, y + actualHeight - 30);
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, numStr[0]);

        // Plane name
        glColor3f(1.0f, 1.0f, 1.0f);
        float nameX = x + actualWidth * 0.5f - strlen(planeNames[i]) * 4.5f;
        glRasterPos2f(nameX, y + 55);
        for (const char* c = planeNames[i]; *c; ++c) 
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

        // Plane description
        glColor4f(0.8f, 0.8f, 0.9f, 0.9f);
        float descX = x + actualWidth * 0.5f - strlen(planeDesc[i]) * 3.0f;
        glRasterPos2f(descX, y + 30);
        for (const char* c = planeDesc[i]; *c; ++c) 
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

        // Selection indicator arrow
        if (selected) {
            float arrowX = x + actualWidth * 0.5f;
            float arrowY = y + actualHeight + 25 + sin(pulseTimer * 4.0f) * 5.0f;
            glColor4f(1.0f, 0.9f, 0.3f, 1.0f);
            glBegin(GL_TRIANGLES);
            glVertex2f(arrowX, arrowY - 15);
            glVertex2f(arrowX - 12, arrowY);
            glVertex2f(arrowX + 12, arrowY);
            glEnd();
        }
    }

    // Title with shadow
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glRasterPos2f(screenWidth * 0.5f - 118, screenHeight - 68);
    const char* title = "SELECT YOUR AIRCRAFT";
    for (const char* c = title; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(screenWidth * 0.5f - 120, screenHeight - 70);
    for (const char* c = title; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);

    // Subtitle
    glColor4f(0.7f, 0.8f, 0.9f, 0.9f);
    glRasterPos2f(screenWidth * 0.5f - 100, screenHeight - 100);
    const char* subtitle = "Choose your fighter wisely";
    for (const char* c = subtitle; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    // Instructions bar at bottom
    glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(screenWidth, 0);
    glVertex2f(screenWidth, 60);
    glVertex2f(0, 60);
    glEnd();

    glColor4f(0.9f, 0.9f, 0.9f, 1.0f);
    glRasterPos2f(screenWidth * 0.5f - 180, 25);
    const char* hint = "[LEFT/RIGHT or A/D] Navigate   |   [ENTER] Confirm Selection";
    for (const char* c = hint; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    // Key indicators
    float keyY = 22;
    float keySize = 20;
    
    // Left arrow key
    glColor4f(0.3f, 0.3f, 0.4f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(50, keyY);
    glVertex2f(50 + keySize, keyY);
    glVertex2f(50 + keySize, keyY + keySize);
    glVertex2f(50, keyY + keySize);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(55, keyY + 5);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, '<');

    // Right arrow key
    glColor4f(0.3f, 0.3f, 0.4f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(80, keyY);
    glVertex2f(80 + keySize, keyY);
    glVertex2f(80 + keySize, keyY + keySize);
    glVertex2f(80, keyY + keySize);
    glEnd();
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(85, keyY + 5);
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, '>');

    glDisable(GL_BLEND);
}

void PlaneSelectionLevel::renderPlanePreview(int planeIndex, float xPos, float yPos, float zPos) {
    glPushMatrix();
    
    glTranslatef(xPos, yPos, zPos);
    glTranslatef(0.0f, 0.0f, -20.0f); // push models away from camera
    glRotatef(rotationAngle, 0, 1, 0);
    
    // Apply different scales for different planes
    if (planeIndex == 2) {
        glScalef(0.00001f, 0.00001f, 0.00001f); // stealth jet is much larger, scale it down more
    } else {
        glScalef(0.01f, 0.01f, 0.01f); // downscale large models
    }
    
    // Highlight selected plane with glow effect
    if (planeIndex == highlightedPlane) {
        float pulse = 0.7f + 0.3f * sin(pulseTimer);
        glColor3f(pulse, pulse, 1.0f);
        
        // Draw selection ring
        glDisable(GL_LIGHTING);
        glPushMatrix();
        glRotatef(-90, 1, 0, 0);
        glTranslatef(0, 0, -50.0f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 64; i++) {
            float angle = (float)i / 64.0f * 6.28318f;
            glVertex3f(cos(angle) * 80.0f, sin(angle) * 80.0f, 0);
        }
        glEnd();
        glPopMatrix();
        glEnable(GL_LIGHTING);
    }
    
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);  // Unlit preview
    glColor3f(1.0f, 1.0f, 1.0f);
    
    if (planeIndex == 0) {
        if (model_plane1.numObjects > 0) {
            model_plane1.Draw();
        } else {
            printf("WARNING: Plane 1 has no objects to draw! Drawing placeholder cube.\n");
            glutWireCube(2.0);
        }
    } else if (planeIndex == 1) {
        if (model_plane2.numObjects > 0) {
            model_plane2.Draw();
        } else {
            printf("WARNING: Plane 2 has no objects to draw! Drawing placeholder cube.\n");
            glutWireCube(2.0);
        }
    } else {
        if (model_plane3.numObjects > 0) {
            model_plane3.Draw();
        } else {
            printf("WARNING: Plane 3 has no objects to draw! Drawing placeholder cube.\n");
            glutWireCube(2.0);
        }
    }
    glEnable(GL_LIGHTING);
    
    glPopMatrix();
}

void PlaneSelectionLevel::renderUI() {
    // Switch to 2D rendering for text
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, screenWidth, 0, screenHeight);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Title
    glRasterPos2f(screenWidth / 2 - 80, screenHeight - 50);
    const char* title = "SELECT YOUR AIRCRAFT";
    for (const char* c = title; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
    
    // Plane 1 label
    glRasterPos2f(screenWidth / 5 - 40, 100);
    const char* plane1Label = "ZERO FIGHTER";
    for (const char* c = plane1Label; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    // Plane 2 label
    glRasterPos2f(screenWidth / 2 - 40, 100);
    const char* plane2Label = "INTERCEPTOR";
    for (const char* c = plane2Label; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    // Plane 3 label
    glRasterPos2f(4 * screenWidth / 5 - 40, 100);
    const char* plane3Label = "STEALTH JET";
    for (const char* c = plane3Label; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    // Instructions
    glColor3f(0.8f, 0.8f, 0.8f);
    glRasterPos2f(screenWidth / 2 - 120, 50);
    const char* instructions = "LEFT/RIGHT: Select  |  ENTER: Confirm";
    for (const char* c = instructions; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    glEnable(GL_DEPTH_TEST);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void PlaneSelectionLevel::cleanup() {
    if (tex_plane1 != 0) {
        glDeleteTextures(1, &tex_plane1);
        tex_plane1 = 0;
    }
    if (tex_plane2 != 0) {
        glDeleteTextures(1, &tex_plane2);
        tex_plane2 = 0;
    }
}

void PlaneSelectionLevel::handleKeyboard(unsigned char key, bool pressed) {
    if (!pressed) return;
    
    switch (key) {
        case 13:  // Enter key
            selectedPlane = highlightedPlane;
            printf("Selected plane: %d\n", selectedPlane);
            GameManager::getInstance().switchToLevel("level1");
            break;
        case 27:  // Escape key
            exit(0);
            break;
        case 'a':
        case 'A':
            highlightedPlane--;
            if (highlightedPlane < 0) highlightedPlane = 2;
            break;
        case 'd':
        case 'D':
            highlightedPlane++;
            if (highlightedPlane > 2) highlightedPlane = 0;
            break;
    }
}

void PlaneSelectionLevel::handleSpecialKeys(int key, bool pressed) {
    if (!pressed) return;
    
    if (key == GLUT_KEY_LEFT) {
        highlightedPlane--;
        if (highlightedPlane < 0) highlightedPlane = 2;
    } else if (key == GLUT_KEY_RIGHT) {
        highlightedPlane++;
        if (highlightedPlane > 2) highlightedPlane = 0;
    }
}

void PlaneSelectionLevel::handleKeyboardUp(unsigned char key) {
}

void PlaneSelectionLevel::handleMouse(int x, int y) {
}

void PlaneSelectionLevel::onEnter() {
    printf("Entered Plane Selection Level\n");
    active = true;
    printf("PlaneSelectionLevel active state: %d\n", active);
}

void PlaneSelectionLevel::onExit() {
    printf("Exited Plane Selection Level\n");
    active = false;
    printf("PlaneSelectionLevel deactivated: %d\n", active);
}
