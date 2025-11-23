#include "Level2.h"
#include <glut.h>

extern void loadBMP(unsigned int* textureID, char* strFileName, int wrap);

Level2::Level2() : Level(), flightSim(nullptr), tex_sky(0), screenWidth(1280), screenHeight(720) {
}

Level2::~Level2() {
    cleanup();
}

void Level2::init() {
    flightSim = new FlightController();
    loadAssets();
    initWindParticles(); // Initialize wind
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
    tex_ground.Load("Textures/ground.bmp");
    loadBMP(&tex_sky, "Textures/blu-sky-3.bmp", true);
}

void Level2::update(float deltaTime) {
    if (!active || !flightSim) return;
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    
    flightSim->update(deltaTime);
    updateWindParticles(deltaTime); // Update wind
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
    
    renderSky();
    
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
    
    glPushMatrix();
    glTranslatef(10, 0, 0);
    glScalef(0.7f, 0.7f, 0.7f);
    model_tree.Draw();
    glPopMatrix();
    
    glPushMatrix();
    glRotatef(90.f, 1, 0, 0);
    model_house.Draw();
    glPopMatrix();
    
    glutSwapBuffers();
}

// ... [Existing RenderSky, RenderGround, Handlers stay the same] ...

void Level2::renderSky() {
    glPushMatrix();
    GLUquadricObj* qobj;
    qobj = gluNewQuadric();
    if (flightSim) {
        glTranslatef(flightSim->player.position.x, flightSim->player.position.y, flightSim->player.position.z);
    }
    glRotated(90, 1, 0, 1);
    glBindTexture(GL_TEXTURE_2D, tex_sky);
    gluQuadricTexture(qobj, true);
    gluQuadricNormals(qobj, GL_SMOOTH);
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