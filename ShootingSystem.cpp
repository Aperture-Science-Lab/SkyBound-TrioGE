#include "ShootingSystem.h"
#include "glew.h"
#include <glut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <direct.h>

ShootingSystem::ShootingSystem() 
    : explosionTexture(0), fireCooldown(0.0f), fireRate(8.0f) {
    bullets.reserve(50);
    explosions.reserve(20);
}

ShootingSystem::~ShootingSystem() {
    if (explosionTexture != 0) {
        glDeleteTextures(1, &explosionTexture);
    }
}

std::string ShootingSystem::getFullPath(const char* filename) {
    // Try to find the file in multiple locations
    FILE* test = NULL;
    
    // Try relative path first (for running from VS)
    std::string path1 = std::string("../") + filename;
    fopen_s(&test, path1.c_str(), "rb");
    if (test) {
        fclose(test);
        return path1;
    }
    
    // Try direct path (for running from project folder)
    fopen_s(&test, filename, "rb");
    if (test) {
        fclose(test);
        return std::string(filename);
    }
    
    // Try with absolute path using current directory
    char cwd[512];
    if (_getcwd(cwd, sizeof(cwd))) {
        std::string absPath = std::string(cwd) + "\\" + filename;
        fopen_s(&test, absPath.c_str(), "rb");
        if (test) {
            fclose(test);
            return absPath;
        }
        
        // Try parent directory
        std::string parentPath = std::string(cwd) + "\\..\\" + filename;
        fopen_s(&test, parentPath.c_str(), "rb");
        if (test) {
            fclose(test);
            return parentPath;
        }
    }
    
    return std::string(filename);
}

void ShootingSystem::init() {
    bullets.clear();
    explosions.clear();
    fireCooldown = 0.0f;
    
    // Determine base path for sounds
    char cwd[512];
    if (_getcwd(cwd, sizeof(cwd))) {
        std::string cwdStr(cwd);
        // Check if we're in Debug folder
        if (cwdStr.find("Debug") != std::string::npos) {
            basePath = "../sound/";
        } else {
            basePath = "sound/";
        }
    } else {
        basePath = "sound/";
    }
    
    // Load explosion texture
    loadExplosionTexture("textures/explosion.bmp");
}

bool ShootingSystem::loadExplosionTexture(const char* filename) {
    std::string fullPath = getFullPath(filename);
    
    FILE* file = NULL;
    fopen_s(&file, fullPath.c_str(), "rb");
    if (!file) {
        // Try alternative paths
        fopen_s(&file, "textures/explosion.bmp", "rb");
        if (!file) {
            fopen_s(&file, "../textures/explosion.bmp", "rb");
            if (!file) {
                return false;
            }
        }
    }
    
    // Read BMP header
    unsigned char header[54];
    if (fread(header, 1, 54, file) != 54 || header[0] != 'B' || header[1] != 'M') {
        fclose(file);
        return false;
    }
    
    int dataOffset = *(int*)&header[10];
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];
    int bitsPerPixel = *(short*)&header[28];
    
    if (width <= 0 || height <= 0) {
        fclose(file);
        return false;
    }
    
    bool topDown = height < 0;
    if (topDown) height = -height;
    
    fseek(file, dataOffset, SEEK_SET);
    
    int rowSize = ((width * 3 + 3) / 4) * 4;
    unsigned char* bmpData = new unsigned char[rowSize * height];
    
    if (fread(bmpData, 1, rowSize * height, file) != (size_t)(rowSize * height)) {
        delete[] bmpData;
        fclose(file);
        return false;
    }
    fclose(file);
    
    // Convert BGR to RGBA with alpha based on brightness (for transparency)
    unsigned char* rgbaData = new unsigned char[width * height * 4];
    
    for (int y = 0; y < height; y++) {
        int srcY = topDown ? y : (height - 1 - y);
        for (int x = 0; x < width; x++) {
            int srcIdx = srcY * rowSize + x * 3;
            int destIdx = (y * width + x) * 4;
            
            unsigned char b = bmpData[srcIdx + 0];
            unsigned char g = bmpData[srcIdx + 1];
            unsigned char r = bmpData[srcIdx + 2];
            
            rgbaData[destIdx + 0] = r;
            rgbaData[destIdx + 1] = g;
            rgbaData[destIdx + 2] = b;
            
            // Use brightness as alpha - black = transparent
            float brightness = (r + g + b) / 3.0f;
            rgbaData[destIdx + 3] = (unsigned char)(brightness);
        }
    }
    
    delete[] bmpData;
    
    glGenTextures(1, &explosionTexture);
    glBindTexture(GL_TEXTURE_2D, explosionTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    
    delete[] rgbaData;
    return true;
}

void ShootingSystem::fire(const Vector3f& position, const Vector3f& forward) {
    if (fireCooldown > 0.0f) return;
    
    // Spawn bullet slightly in front of plane
    Vector3f bulletPos = position + forward * 5.0f;
    bulletPos.y -= 1.0f;  // Slightly below plane center
    
    spawnBullet(bulletPos, forward);
    
    // Play sound
    playShootSound();
    
    // Reset cooldown
    fireCooldown = 1.0f / fireRate;
}

void ShootingSystem::spawnBullet(const Vector3f& position, const Vector3f& direction) {
    // Find inactive bullet or add new one
    Bullet* bullet = nullptr;
    
    for (size_t i = 0; i < bullets.size(); i++) {
        if (!bullets[i].active) {
            bullet = &bullets[i];
            break;
        }
    }
    
    if (!bullet) {
        bullets.push_back(Bullet());
        bullet = &bullets.back();
    }
    
    bullet->position = position;
    bullet->direction = direction;
    bullet->speed = 500.0f;  // Fast bullets
    bullet->velocity = direction * bullet->speed;
    bullet->life = 0.0f;
    bullet->maxLife = 3.0f;  // 3 seconds max flight time
    bullet->active = true;
}

void ShootingSystem::spawnExplosion(const Vector3f& position) {
    // Find inactive explosion or add new one
    GroundExplosion* exp = nullptr;
    
    for (size_t i = 0; i < explosions.size(); i++) {
        if (!explosions[i].active) {
            exp = &explosions[i];
            break;
        }
    }
    
    if (!exp) {
        explosions.push_back(GroundExplosion());
        exp = &explosions.back();
    }
    
    exp->position = position;
    exp->position.y = 0.5f;  // Slightly above ground
    exp->timer = 0.0f;
    exp->maxTime = 0.8f;  // 0.8 second explosion
    exp->size = 5.0f + (rand() % 100) / 50.0f;  // 5-7 size variation
    exp->alpha = 1.0f;
    exp->frame = 0;
    exp->active = true;
    
    // Play explosion sound
    playExplosionSound();
}

void ShootingSystem::playShootSound() {
    std::string soundPath = basePath + "shooting.wav";
    PlaySoundA(soundPath.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NOSTOP);
}

void ShootingSystem::playExplosionSound() {
    // Use crash sound for explosion or create a separate one
    std::string soundPath = basePath + "plane-crash.wav";
    PlaySoundA(soundPath.c_str(), NULL, SND_FILENAME | SND_ASYNC);
}

bool ShootingSystem::checkGroundCollision(Bullet& bullet) {
    // Simple ground collision at Y = 0
    if (bullet.position.y <= 0.0f) {
        return true;
    }
    return false;
}

void ShootingSystem::update(float deltaTime) {
    // Update cooldown
    if (fireCooldown > 0.0f) {
        fireCooldown -= deltaTime;
    }
    
    // Update bullets
    for (size_t i = 0; i < bullets.size(); i++) {
        Bullet& bullet = bullets[i];
        if (!bullet.active) continue;
        
        // Move bullet
        bullet.position = bullet.position + bullet.velocity * deltaTime;
        
        // Apply gravity (slight arc)
        bullet.velocity.y -= 50.0f * deltaTime;
        
        // Update life
        bullet.life += deltaTime;
        
        // Check ground collision
        if (checkGroundCollision(bullet)) {
            spawnExplosion(bullet.position);
            bullet.active = false;
            continue;
        }
        
        // Check lifetime
        if (bullet.life >= bullet.maxLife) {
            bullet.active = false;
        }
    }
    
    // Update explosions
    for (size_t i = 0; i < explosions.size(); i++) {
        GroundExplosion& exp = explosions[i];
        if (!exp.active) continue;
        
        exp.timer += deltaTime;
        
        // Grow size over time
        float t = exp.timer / exp.maxTime;
        exp.size = (5.0f + t * 10.0f);  // Grow from 5 to 15
        
        // Fade out
        exp.alpha = 1.0f - t;
        
        // Update animation frame (8 frames assumed)
        exp.frame = (int)(t * 8) % 8;
        
        // Deactivate when done
        if (exp.timer >= exp.maxTime) {
            exp.active = false;
        }
    }
}

void ShootingSystem::renderBullets() {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    
    for (size_t i = 0; i < bullets.size(); i++) {
        Bullet& bullet = bullets[i];
        if (!bullet.active) continue;
        
        // Calculate tracer end point
        Vector3f tracerEnd = bullet.position - bullet.direction * 3.0f;  // 3 unit tracer
        
        // Draw tracer line
        glLineWidth(3.0f);
        glBegin(GL_LINES);
        
        // Bright yellow/orange at front
        glColor4f(1.0f, 0.9f, 0.3f, 1.0f);
        glVertex3f(bullet.position.x, bullet.position.y, bullet.position.z);
        
        // Fade to red at back
        glColor4f(1.0f, 0.3f, 0.1f, 0.3f);
        glVertex3f(tracerEnd.x, tracerEnd.y, tracerEnd.z);
        
        glEnd();
        
        // Draw bullet glow
        glPointSize(8.0f);
        glBegin(GL_POINTS);
        glColor4f(1.0f, 1.0f, 0.5f, 0.8f);
        glVertex3f(bullet.position.x, bullet.position.y, bullet.position.z);
        glEnd();
    }
    
    glPopAttrib();
}

void ShootingSystem::renderExplosions(const Vector3f& cameraPosition) {
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for fire
    glDepthMask(GL_FALSE);
    
    if (explosionTexture != 0) {
        glBindTexture(GL_TEXTURE_2D, explosionTexture);
    }
    
    for (size_t i = 0; i < explosions.size(); i++) {
        GroundExplosion& exp = explosions[i];
        if (!exp.active) continue;
        
        glPushMatrix();
        glTranslatef(exp.position.x, exp.position.y + exp.size * 0.5f, exp.position.z);
        
        // Billboard - face camera
        float dx = cameraPosition.x - exp.position.x;
        float dz = cameraPosition.z - exp.position.z;
        float angle = atan2(dx, dz) * 180.0f / 3.14159f;
        glRotatef(angle, 0, 1, 0);
        
        float size = exp.size;
        float alpha = exp.alpha;
        
        // Orange/yellow explosion color
        float t = exp.timer / exp.maxTime;
        float r = 1.0f;
        float g = 0.6f - t * 0.4f;
        float b = 0.1f;
        
        glColor4f(r, g, b, alpha);
        
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3f(-size, -size * 0.5f, 0);
        glTexCoord2f(1, 0); glVertex3f(size, -size * 0.5f, 0);
        glTexCoord2f(1, 1); glVertex3f(size, size * 1.5f, 0);
        glTexCoord2f(0, 1); glVertex3f(-size, size * 1.5f, 0);
        glEnd();
        
        // Second layer for more volume
        glColor4f(1.0f, 0.9f, 0.3f, alpha * 0.5f);
        float size2 = size * 0.7f;
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3f(-size2, -size2 * 0.3f, 0.1f);
        glTexCoord2f(1, 0); glVertex3f(size2, -size2 * 0.3f, 0.1f);
        glTexCoord2f(1, 1); glVertex3f(size2, size2 * 1.7f, 0.1f);
        glTexCoord2f(0, 1); glVertex3f(-size2, size2 * 1.7f, 0.1f);
        glEnd();
        
        glPopMatrix();
    }
    
    glDepthMask(GL_TRUE);
    glPopAttrib();
}

void ShootingSystem::reset() {
    for (size_t i = 0; i < bullets.size(); i++) {
        bullets[i].active = false;
    }
    for (size_t i = 0; i < explosions.size(); i++) {
        explosions[i].active = false;
    }
    fireCooldown = 0.0f;
}

int ShootingSystem::getActiveBulletCount() const {
    int count = 0;
    for (size_t i = 0; i < bullets.size(); i++) {
        if (bullets[i].active) count++;
    }
    return count;
}

int ShootingSystem::getActiveExplosionCount() const {
    int count = 0;
    for (size_t i = 0; i < explosions.size(); i++) {
        if (explosions[i].active) count++;
    }
    return count;
}
