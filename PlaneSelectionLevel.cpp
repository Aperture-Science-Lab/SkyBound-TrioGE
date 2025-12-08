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
            screenWidth(1280), screenHeight(720), tex_plane1(0), tex_plane2(0) {
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
    
    static int renderCount = 0;
    if (renderCount < 100) {
        printf("  PlaneSelect::render() #%d, active=%d\n", renderCount++, active);
    }
    
    // Sync viewport with current window size
    int winW = glutGet(GLUT_WINDOW_WIDTH);
    int winH = glutGet(GLUT_WINDOW_HEIGHT);
    if (winW > 0 && winH > 0) {
        screenWidth = winW;
        screenHeight = winH;
    }
    glViewport(0, 0, screenWidth, screenHeight);

    // Clear to flat mid-gray background for visibility
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);  // Brighter gray
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

    // Card positions
    float cardWidth = 280.0f;
    float cardHeight = 180.0f;
    float gap = 80.0f;
    float centerX = screenWidth * 0.5f;
    float centerY = screenHeight * 0.55f;
    float leftX = centerX - cardWidth - gap * 0.5f;
    float rightX = centerX + gap * 0.5f;
    float cardY = centerY - cardHeight * 0.5f;

    // Force white color for testing
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
        glVertex2f(100, 100);
        glVertex2f(300, 100);
        glVertex2f(300, 300);
        glVertex2f(100, 300);
    glEnd();
    
    auto drawCard = [](float x, float y, float w, float h, bool selected, const float color[3]) {
        float border = 6.0f;
        glColor3f(color[0] * 0.6f, color[1] * 0.6f, color[2] * 0.6f);
        glBegin(GL_QUADS);
            glVertex2f(x - border, y - border);
            glVertex2f(x + w + border, y - border);
            glVertex2f(x + w + border, y + h + border);
            glVertex2f(x - border, y + h + border);
        glEnd();
        glColor3f(color[0], color[1], color[2]);
        glBegin(GL_QUADS);
            glVertex2f(x, y);
            glVertex2f(x + w, y);
            glVertex2f(x + w, y + h);
            glVertex2f(x, y + h);
        glEnd();
        if (selected) {
            glColor3f(1.0f, 1.0f, 1.0f);
            glLineWidth(3.0f);
            glBegin(GL_LINE_LOOP);
                glVertex2f(x - border, y - border);
                glVertex2f(x + w + border, y - border);
                glVertex2f(x + w + border, y + h + border);
                glVertex2f(x - border, y + h + border);
            glEnd();
            glLineWidth(1.0f);
        }
    };

    // Draw cards
    const float blue[3] = {0.20f, 0.36f, 0.65f};
    const float green[3] = {0.19f, 0.55f, 0.38f};
    drawCard(leftX, cardY, cardWidth, cardHeight, highlightedPlane == 0, blue);
    drawCard(rightX, cardY, cardWidth, cardHeight, highlightedPlane == 1, green);

    // Text labels
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(centerX - 90, screenHeight - 60);
    const char* title = "Select Your Aircraft";
    for (const char* c = title; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);

    glRasterPos2f(leftX + 40, cardY + cardHeight - 30);
    const char* p1 = "Plane 1";
    for (const char* c = p1; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    glRasterPos2f(rightX + 40, cardY + cardHeight - 30);
    const char* p2 = "Plane 2";
    for (const char* c = p2; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);

    glColor3f(0.8f, 0.8f, 0.8f);
    glRasterPos2f(centerX - 150, screenHeight * 0.15f);
    const char* hint = "LEFT/RIGHT to choose, ENTER to confirm";
    for (const char* c = hint; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
}

void PlaneSelectionLevel::renderPlanePreview(int planeIndex, float xPos, float yPos, float zPos) {
    glPushMatrix();
    
    glTranslatef(xPos, yPos, zPos);
    glTranslatef(0.0f, 0.0f, -20.0f); // push models away from camera
    glRotatef(rotationAngle, 0, 1, 0);
    glScalef(0.01f, 0.01f, 0.01f); // downscale large models
    
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
    } else {
        if (model_plane2.numObjects > 0) {
            model_plane2.Draw();
        } else {
            printf("WARNING: Plane 2 has no objects to draw! Drawing placeholder cube.\n");
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
    glRasterPos2f(screenWidth / 4 - 40, 100);
    const char* plane1Label = "FIGHTER JET";
    for (const char* c = plane1Label; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
    
    // Plane 2 label
    glRasterPos2f(3 * screenWidth / 4 - 40, 100);
    const char* plane2Label = "INTERCEPTOR";
    for (const char* c = plane2Label; *c != '\0'; c++) {
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
            highlightedPlane = 0;
            break;
        case 'd':
        case 'D':
            highlightedPlane = 1;
            break;
    }
}

void PlaneSelectionLevel::handleSpecialKeys(int key, bool pressed) {
    if (!pressed) return;
    
    if (key == GLUT_KEY_LEFT) {
        highlightedPlane = 0;
    } else if (key == GLUT_KEY_RIGHT) {
        highlightedPlane = 1;
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
