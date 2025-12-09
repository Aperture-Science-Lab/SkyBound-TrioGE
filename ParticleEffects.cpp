#include "ParticleEffects.h"
#include "glew.h"
#include <glut.h>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <stdio.h>

// ============ WIND SYSTEM IMPLEMENTATION ============

WindSystem::WindSystem() : minSpeedThreshold(30.0f) {
}

WindSystem::~WindSystem() {
}

void WindSystem::init(int maxParticles) {
    particles.resize(maxParticles);
    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].active = false;
    }
}

void WindSystem::reset() {
    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].active = false;
    }
}

void WindSystem::update(float deltaTime, const Vector3f& cameraPos, const Vector3f& forward, float speed) {
    // Only show wind if moving fast enough
    if (speed < minSpeedThreshold) return;
    
    for (size_t i = 0; i < particles.size(); i++) {
        WindParticle& p = particles[i];
        
        if (!p.active) {
            // Respawn chance based on speed
            if (rand() % 100 < (int)(speed / 5.0f)) {
                p.active = true;
                // Spawn ahead of player in a random radius
                float range = 15.0f;
                float rX = ((rand() % 100) / 50.0f - 1.0f) * range;
                float rY = ((rand() % 100) / 50.0f - 1.0f) * range;
                float rZ = ((rand() % 100) / 50.0f - 1.0f) * range;
                
                p.position = cameraPos + (forward * 40.0f) + Vector3f(rX, rY, rZ);
                p.length = 2.0f + (speed / 20.0f);
                p.life = 1.0f;
            }
        } else {
            // Move particle opposite to flight direction
            p.position = p.position - (forward * (speed * 1.5f * deltaTime));
            p.life -= deltaTime;
            
            // Deactivate if behind camera or dead
            Vector3f toParticle = p.position - cameraPos;
            float dot = toParticle.x * forward.x + toParticle.y * forward.y + toParticle.z * forward.z;
            
            if (p.life <= 0 || dot < -5.0f) {
                p.active = false;
            }
        }
    }
}

void WindSystem::render(const Vector3f& forward, float speed) {
    if (speed < minSpeedThreshold) return;
    
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    
    for (size_t i = 0; i < particles.size(); i++) {
        const WindParticle& p = particles[i];
        if (p.active) {
            // Fade based on speed
            float alpha = (speed / 100.0f) * 0.5f;
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

// ============ EXPLOSION SYSTEM IMPLEMENTATION ============

ExplosionSystem::ExplosionSystem()
    : textureID(0), explosionActive(false), explosionTimer(0.0f),
      spawnTimer(0.0f), spawnDuration(0.3f) {
    particles.reserve(100);
}

ExplosionSystem::~ExplosionSystem() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

float ExplosionSystem::randomFloat(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

bool ExplosionSystem::loadExplosionTexture(const char* filename) {
    FILE* file = NULL;
    fopen_s(&file, filename, "rb");
    if (!file) {
        return false;
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
    
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        fclose(file);
        return false;
    }
    
    bool topDown = height < 0;
    if (topDown) height = -height;
    
    // We'll convert to RGBA for transparency support
    unsigned char* rgbaData = new unsigned char[width * height * 4];
    
    if (bitsPerPixel == 24) {
        fseek(file, dataOffset, SEEK_SET);
        int rowSize = ((width * 3 + 3) / 4) * 4;
        unsigned char* bmpData = new unsigned char[rowSize * height];
        
        if (fread(bmpData, 1, rowSize * height, file) != (size_t)(rowSize * height)) {
            delete[] bmpData;
            delete[] rgbaData;
            fclose(file);
            return false;
        }
        
        // Convert BGR to RGBA, using brightness as alpha
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
                
                // Use brightness as alpha (for explosion effect)
                float brightness = (r + g + b) / 3.0f;
                rgbaData[destIdx + 3] = (unsigned char)brightness;
            }
        }
        delete[] bmpData;
    } else {
        delete[] rgbaData;
        fclose(file);
        return false;
    }
    
    fclose(file);
    
    // Create OpenGL texture
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaData);
    
    delete[] rgbaData;
    return true;
}

void ExplosionSystem::init() {
    // Try to load explosion texture
    if (!loadExplosionTexture("../textures/explosion.bmp")) {
        if (!loadExplosionTexture("textures/explosion.bmp")) {
            // Create procedural explosion texture
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            
            const int texSize = 64;
            unsigned char* texData = new unsigned char[texSize * texSize * 4];
            
            float centerX = texSize / 2.0f;
            float centerY = texSize / 2.0f;
            float maxDist = texSize / 2.0f;
            
            for (int y = 0; y < texSize; y++) {
                for (int x = 0; x < texSize; x++) {
                    float dx = x - centerX;
                    float dy = y - centerY;
                    float dist = sqrt(dx * dx + dy * dy);
                    
                    // Soft falloff from center
                    float alpha = 1.0f - (dist / maxDist);
                    if (alpha < 0.0f) alpha = 0.0f;
                    alpha = alpha * alpha;  // Quadratic falloff
                    
                    int idx = (y * texSize + x) * 4;
                    
                    // Orange/yellow gradient for explosion
                    float t = dist / maxDist;
                    texData[idx + 0] = (unsigned char)(255);                    // R
                    texData[idx + 1] = (unsigned char)(200 * (1.0f - t * 0.5f)); // G (fades to orange)
                    texData[idx + 2] = (unsigned char)(50 * (1.0f - t));         // B (very little)
                    texData[idx + 3] = (unsigned char)(alpha * 255);             // A
                }
            }
            
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            
            delete[] texData;
        }
    }
}

void ExplosionSystem::startExplosion(const Vector3f& position) {
    explosionCenter = position;
    explosionActive = true;
    explosionTimer = 0.0f;
    spawnTimer = 0.0f;
    
    // Spawn initial burst of particles
    for (int i = 0; i < 30; i++) {
        spawnParticle();
    }
}

void ExplosionSystem::reset() {
    explosionActive = false;
    explosionTimer = 0.0f;
    spawnTimer = 0.0f;
    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].active = false;
    }
}

bool ExplosionSystem::hasActiveParticles() const {
    for (size_t i = 0; i < particles.size(); i++) {
        if (particles[i].active) return true;
    }
    return false;
}

void ExplosionSystem::spawnParticle() {
    ExplosionParticle p;
    
    // Start at explosion center with slight random offset
    p.position.x = explosionCenter.x + randomFloat(-3.0f, 3.0f);
    p.position.y = explosionCenter.y + randomFloat(-1.0f, 3.0f);
    p.position.z = explosionCenter.z + randomFloat(-3.0f, 3.0f);
    
    // Velocity - expand outward in all directions
    float speed = randomFloat(10.0f, 30.0f);
    float angleH = randomFloat(0.0f, 6.28318f);
    float angleV = randomFloat(-0.5f, 1.0f);
    
    p.velocity.x = cos(angleH) * speed * (1.0f - fabs(angleV));
    p.velocity.y = angleV * speed + randomFloat(5.0f, 15.0f);  // Bias upward
    p.velocity.z = sin(angleH) * speed * (1.0f - fabs(angleV));
    
    // Size properties - starts medium, grows large
    p.startSize = randomFloat(3.0f, 8.0f);
    p.size = p.startSize;
    p.maxSize = randomFloat(20.0f, 40.0f);
    
    // Short lifetime for quick explosion
    p.maxLife = randomFloat(0.8f, 1.5f);
    p.life = p.maxLife;
    
    // Start opaque
    p.alpha = randomFloat(0.8f, 1.0f);
    
    // Random rotation
    p.rotation = randomFloat(0.0f, 360.0f);
    p.rotationSpeed = randomFloat(-90.0f, 90.0f);
    
    p.active = true;
    
    // Find inactive slot or add new
    bool found = false;
    for (size_t i = 0; i < particles.size(); i++) {
        if (!particles[i].active) {
            particles[i] = p;
            found = true;
            break;
        }
    }
    if (!found && particles.size() < 100) {
        particles.push_back(p);
    }
}

void ExplosionSystem::update(float deltaTime) {
    if (explosionActive) {
        explosionTimer += deltaTime;
        spawnTimer += deltaTime;
        
        // Spawn additional particles during spawn duration
        if (explosionTimer < spawnDuration) {
            while (spawnTimer >= 0.02f) {
                spawnParticle();
                spawnTimer -= 0.02f;
            }
        } else {
            explosionActive = false;
        }
    }
    
    // Update all particles
    for (size_t i = 0; i < particles.size(); i++) {
        if (!particles[i].active) continue;
        
        ExplosionParticle& p = particles[i];
        
        p.life -= deltaTime;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }
        
        float lifeRatio = p.life / p.maxLife;
        float ageRatio = 1.0f - lifeRatio;
        
        // Update position
        p.position.x += p.velocity.x * deltaTime;
        p.position.y += p.velocity.y * deltaTime;
        p.position.z += p.velocity.z * deltaTime;
        
        // Slow down due to air resistance
        p.velocity.x *= (1.0f - 2.0f * deltaTime);
        p.velocity.y *= (1.0f - 1.5f * deltaTime);
        p.velocity.z *= (1.0f - 2.0f * deltaTime);
        
        // Apply gravity slightly
        p.velocity.y -= 5.0f * deltaTime;
        
        // Size grows quickly then stabilizes
        float growthCurve = 1.0f - pow(1.0f - ageRatio, 3.0f);  // Fast ease-out
        p.size = p.startSize + (p.maxSize - p.startSize) * growthCurve;
        
        // Alpha fades out throughout lifetime
        p.alpha = lifeRatio * 0.9f;
        
        // Rotate
        p.rotation += p.rotationSpeed * deltaTime;
    }
}

void ExplosionSystem::render(const Vector3f& cameraPosition) {
    if (particles.empty()) return;
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for fire effect
    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);
    
    // Sort particles back to front
    std::vector<std::pair<float, size_t>> sortedIndices;
    for (size_t i = 0; i < particles.size(); i++) {
        if (particles[i].active) {
            float dx = particles[i].position.x - cameraPosition.x;
            float dy = particles[i].position.y - cameraPosition.y;
            float dz = particles[i].position.z - cameraPosition.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            sortedIndices.push_back(std::make_pair(distSq, i));
        }
    }
    
    std::sort(sortedIndices.begin(), sortedIndices.end(),
              [](const std::pair<float, size_t>& a, const std::pair<float, size_t>& b) {
                  return a.first > b.first;
              });
    
    // Get modelview matrix for billboarding
    float modelview[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    
    Vector3f camRight(modelview[0], modelview[4], modelview[8]);
    Vector3f camUp(modelview[1], modelview[5], modelview[9]);
    
    glBegin(GL_QUADS);
    
    for (size_t idx = 0; idx < sortedIndices.size(); idx++) {
        const ExplosionParticle& p = particles[sortedIndices[idx].second];
        
        float halfSize = p.size * 0.5f;
        
        // Apply rotation
        float rad = p.rotation * 3.14159f / 180.0f;
        float cosR = cos(rad);
        float sinR = sin(rad);
        
        Vector3f right = camRight * cosR + camUp * sinR;
        Vector3f up = camUp * cosR - camRight * sinR;
        
        // Color based on life - starts bright yellow, fades to dark orange/red
        float lifeRatio = p.life / p.maxLife;
        float r = 1.0f;
        float g = 0.5f + lifeRatio * 0.5f;  // Yellow to orange
        float b = lifeRatio * 0.3f;          // Fades to dark
        
        glColor4f(r, g, b, p.alpha);
        
        Vector3f bottomLeft = p.position - right * halfSize - up * halfSize;
        Vector3f bottomRight = p.position + right * halfSize - up * halfSize;
        Vector3f topRight = p.position + right * halfSize + up * halfSize;
        Vector3f topLeft = p.position - right * halfSize + up * halfSize;
        
        glTexCoord2f(0.0f, 0.0f);
        glVertex3f(bottomLeft.x, bottomLeft.y, bottomLeft.z);
        
        glTexCoord2f(1.0f, 0.0f);
        glVertex3f(bottomRight.x, bottomRight.y, bottomRight.z);
        
        glTexCoord2f(1.0f, 1.0f);
        glVertex3f(topRight.x, topRight.y, topRight.z);
        
        glTexCoord2f(0.0f, 1.0f);
        glVertex3f(topLeft.x, topLeft.y, topLeft.z);
    }
    
    glEnd();
    
    glDepthMask(GL_TRUE);
    glPopAttrib();
}

// ============ JET TRAIL SYSTEM IMPLEMENTATION ============

JetTrailSystem::JetTrailSystem() 
    : textureID(0), spawnTimer(0.0f), spawnInterval(0.05f) {
    particles.reserve(200);
}

JetTrailSystem::~JetTrailSystem() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

bool JetTrailSystem::loadTrailTexture(const char* filename) {
    FILE* file = NULL;
    fopen_s(&file, filename, "rb");
    if (!file) return false;
    
    unsigned char header[54];
    if (fread(header, 1, 54, file) != 54 || header[0] != 'B' || header[1] != 'M') {
        fclose(file);
        return false;
    }
    
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];
    int dataOffset = *(int*)&header[10];
    
    if (width <= 0 || height <= 0) {
        fclose(file);
        return false;
    }
    
    int size = width * height * 4;
    unsigned char* data = new unsigned char[size];
    
    fseek(file, dataOffset, SEEK_SET);
    // Simple read assuming 24/32bpp, similar logic to explosion loader needed for robustness
    // For simplicity here, we'll use procedural generation if this fails or just proceed
    // But since we want "AAA" look, let's generate a high quality procedural texture instead of relying on file
    
    fclose(file);
    delete[] data;
    return false; // Fallback to procedural
}

void JetTrailSystem::init(int maxParticles) {
    particles.resize(maxParticles);
    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].active = false;
    }
    
    // Generate procedural smoke/trail texture
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    const int size = 64;
    unsigned char* texData = new unsigned char[size * size * 4];
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (x - size/2) / (float)(size/2);
            float dy = (y - size/2) / (float)(size/2);
            float d = sqrt(dx*dx + dy*dy);
            
            float alpha = 1.0f - d;
            if (alpha < 0) alpha = 0;
            alpha = pow(alpha, 2.0f); // Soft falloff
            
            int idx = (y * size + x) * 4;
            // White smoke
            texData[idx+0] = 200;
            texData[idx+1] = 200;
            texData[idx+2] = 220; // Slight blue tint
            texData[idx+3] = (unsigned char)(alpha * 255);
        }
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    
    delete[] texData;
}

void JetTrailSystem::diffEmit(const Vector3f& position, const Vector3f& velocity) {
    // Find free particle
    for (size_t i = 0; i < particles.size(); i++) {
        if (!particles[i].active) {
            particles[i].active = true;
            particles[i].position = position;
            particles[i].life = 1.5f; // Short life
            particles[i].maxLife = 1.5f;
            particles[i].size = 2.0f; // Start small
            particles[i].alpha = 0.5f;
            break;
        }
    }
}

void JetTrailSystem::update(float deltaTime) {
    for (size_t i = 0; i < particles.size(); i++) {
        if (particles[i].active) {
            JetTrailParticle& p = particles[i];
            
            p.life -= deltaTime;
            if (p.life <= 0) {
                p.active = false;
                continue;
            }
            
            // Expand
            p.size += 15.0f * deltaTime;
            
            // Fade out
            p.alpha = (p.life / p.maxLife) * 0.4f;
        }
    }
}

void JetTrailSystem::render() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    glDepthMask(GL_FALSE);
    
    // Billboard setup
    float modelview[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    Vector3f right(modelview[0], modelview[4], modelview[8]);
    Vector3f up(modelview[1], modelview[5], modelview[9]);
    
    glBegin(GL_QUADS);
    for (size_t i = 0; i < particles.size(); i++) {
        if (particles[i].active) {
            const JetTrailParticle& p = particles[i];
            float hs = p.size * 0.5f;
            
            Vector3f v1 = p.position - right * hs - up * hs;
            Vector3f v2 = p.position + right * hs - up * hs;
            Vector3f v3 = p.position + right * hs + up * hs;
            Vector3f v4 = p.position - right * hs + up * hs;
            
            glColor4f(1.0f, 1.0f, 1.0f, p.alpha);
            
            glTexCoord2f(0, 0); glVertex3f(v1.x, v1.y, v1.z);
            glTexCoord2f(1, 0); glVertex3f(v2.x, v2.y, v2.z);
            glTexCoord2f(1, 1); glVertex3f(v3.x, v3.y, v3.z);
            glTexCoord2f(0, 1); glVertex3f(v4.x, v4.y, v4.z);
        }
    }
    glEnd();
    
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
}

void JetTrailSystem::reset() {
    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].active = false;
    }
}

// ============ PARTICLE EFFECTS MANAGER IMPLEMENTATION ============

ParticleEffects::ParticleEffects() {
}

ParticleEffects::~ParticleEffects() {
}

void ParticleEffects::init() {
    wind.init(50);
    explosion.init();
    jetTrail.init();
}

void ParticleEffects::update(float deltaTime, const Vector3f& cameraPos, const Vector3f& forward, float speed) {
    wind.update(deltaTime, cameraPos, forward, speed);
    explosion.update(deltaTime);
    jetTrail.update(deltaTime);
}

void ParticleEffects::renderWind(const Vector3f& forward, float speed) {
    wind.render(forward, speed);
}

void ParticleEffects::renderExplosion(const Vector3f& cameraPosition) {
    explosion.render(cameraPosition);
}

void ParticleEffects::triggerExplosion(const Vector3f& position) {
    explosion.startExplosion(position);
}

bool ParticleEffects::isExplosionActive() const {
    return explosion.isActive();
}

void ParticleEffects::reset() {
    wind.reset();
    explosion.reset();
    jetTrail.reset();
}

void ParticleEffects::renderJetTrail() {
    jetTrail.render();
}

void ParticleEffects::emitJetTrail(const Vector3f& position, const Vector3f& velocity) {
    jetTrail.diffEmit(position, velocity);
}
