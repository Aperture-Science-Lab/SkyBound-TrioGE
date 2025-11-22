#include "Level2.h"
#include <glut.h>

// External function from TextureBuilder.h (defined in OpenGLMeshLoader.cpp's translation unit)
extern void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

Level2::Level2() : Level(), flightSim(nullptr), tex_sky(0), screenWidth(1280), screenHeight(720) {
}

Level2::~Level2() {
    cleanup();
}

void Level2::init() {
    // Initialize flight controller
    flightSim = new FlightController();
    
    // Load all assets
    loadAssets();
}

void Level2::loadAssets() {
    model_house.Load("Models/house/house.3DS");
    model_tree.Load("Models/tree/Tree1.3ds");
    tex_ground.Load("Textures/ground.bmp");
    loadBMP(&tex_sky, "Textures/blu-sky-3.bmp", true);
    
    if (flightSim) {
        flightSim->loadModel("models/plane/mitsubishi_a6m2_zero_model_11.3ds");
    }
}

void Level2::update(float deltaTime) {
    if (!active || !flightSim) return;
    
    // Clamp delta time to prevent large jumps
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    
    flightSim->update(deltaTime);
}

void Level2::render() {
    if (!active) return;
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Setup camera from flight controller
    if (flightSim) {
        flightSim->setupCamera();
    }
    
    // Render sky
    renderSky();
    
    // Set up lighting
    GLfloat lightIntensity[] = { 0.7f, 0.7f, 0.7f, 1.0f };
    GLfloat lightPosition[] = { 0.0f, 100.0f, 0.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightIntensity);
    
    // Render ground
    renderGround();
    
    // Draw plane
    if (flightSim) {
        flightSim->drawPlane();
    }
    
    // Draw tree model
    glPushMatrix();
    glTranslatef(10, 0, 0);
    glScalef(0.7f, 0.7f, 0.7f);
    model_tree.Draw();
    glPopMatrix();
    
    // Draw house model
    glPushMatrix();
    glRotatef(90.f, 1, 0, 0);
    model_house.Draw();
    glPopMatrix();
    
    glutSwapBuffers();
}

void Level2::renderSky() {
    glPushMatrix();
    GLUquadricObj* qobj;
    qobj = gluNewQuadric();
    
    // Move skybox with camera to make it appear infinite
    if (flightSim) {
        glTranslatef(flightSim->player.position.x, flightSim->player.position.y, flightSim->player.position.z);
    }
    
    glRotated(90, 1, 0, 1);
    glBindTexture(GL_TEXTURE_2D, tex_sky);
    gluQuadricTexture(qobj, true);
    gluQuadricNormals(qobj, GL_SMOOTH);
    
    // Disable depth write so it's always background
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    gluSphere(qobj, 800, 100, 100);
    glEnable(GL_LIGHTING);
    glDepthMask(GL_TRUE);
    
    gluDeleteQuadric(qobj);
    glPopMatrix();
}

void Level2::renderGround() {
    glDisable(GL_LIGHTING);
    glColor3f(0.6f, 0.6f, 0.6f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_ground.texture[0]);
    
    // Ensure texture repeats
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glPushMatrix();
    
    // Infinite ground logic
    float px = 0, pz = 0;
    if (flightSim) {
        px = flightSim->player.position.x;
        pz = flightSim->player.position.z;
    }
    
    // Move ground with player
    glTranslatef(px, 0, pz);
    
    float size = 2000.0f;
    float texScale = 0.05f;
    
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    
    // Calculate texture coordinates based on world position to simulate movement
    glTexCoord2f((px - size) * texScale, (pz - size) * texScale);
    glVertex3f(-size, 0, -size);
    
    glTexCoord2f((px + size) * texScale, (pz - size) * texScale);
    glVertex3f(size, 0, -size);
    
    glTexCoord2f((px + size) * texScale, (pz + size) * texScale);
    glVertex3f(size, 0, size);
    
    glTexCoord2f((px - size) * texScale, (pz + size) * texScale);
    glVertex3f(-size, 0, size);
    
    glEnd();
    glPopMatrix();
    
    glEnable(GL_LIGHTING);
    glColor3f(1, 1, 1);
}

void Level2::handleKeyboard(unsigned char key, bool pressed) {
    if (!active) return;
    
    // Exit on ESC
    if (key == 27) {
        exit(0);
    }
    
    if (flightSim) {
        flightSim->handleInput(key, pressed);
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
    
    // Center mouse
    if (screenWidth > 0 && screenHeight > 0) {
        glutWarpPointer(centerX, centerY);
    }
}

void Level2::onEnter() {
    Level::onEnter();
    
    // Get current window size
    screenWidth = glutGet(GLUT_WINDOW_WIDTH);
    screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    
    // Hide cursor
    glutSetCursor(GLUT_CURSOR_NONE);
}

void Level2::onExit() {
    Level::onExit();
    
    // Show cursor
    glutSetCursor(GLUT_CURSOR_INHERIT);
}

void Level2::cleanup() {
    if (flightSim) {
        delete flightSim;
        flightSim = nullptr;
    }
    
    // OpenGL will clean up textures when context is destroyed
}
