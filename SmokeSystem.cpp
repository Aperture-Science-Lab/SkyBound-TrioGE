#include "SmokeSystem.h"
#include "glew.h"
#include <glut.h>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <stdio.h>

SmokeSystem::SmokeSystem() 
    : textureID(0), smokeActive(false), spawnTimer(0.0f), 
      spawnRate(0.03f), emitDuration(5.0f), emitTimer(0.0f) {
    particles.reserve(200);  // Pre-allocate for performance
}

SmokeSystem::~SmokeSystem() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

void SmokeSystem::init() {
    // Create a simple procedural smoke texture if file loading fails
    // This ensures smoke always works even without texture file
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Create a 64x64 soft circle texture procedurally
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
            alpha = alpha * alpha;  // Quadratic falloff for softer edges
            
            int idx = (y * texSize + x) * 4;
            texData[idx + 0] = 200;  // R - light gray
            texData[idx + 1] = 200;  // G
            texData[idx + 2] = 200;  // B
            texData[idx + 3] = (unsigned char)(alpha * 255);  // A
        }
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    
    delete[] texData;
}

float SmokeSystem::randomFloat(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

void SmokeSystem::startSmoke(const Vector3f& position) {
    emitPosition = position;
    smokeActive = true;
    emitTimer = 0.0f;
    spawnTimer = 0.0f;
}

void SmokeSystem::stopSmoke() {
    smokeActive = false;
}

void SmokeSystem::reset() {
    smokeActive = false;
    emitTimer = 0.0f;
    spawnTimer = 0.0f;
    for (size_t i = 0; i < particles.size(); i++) {
        particles[i].active = false;
    }
}

bool SmokeSystem::hasActiveParticles() const {
    for (size_t i = 0; i < particles.size(); i++) {
        if (particles[i].active) return true;
    }
    return false;
}

void SmokeSystem::spawnParticle() {
    SmokeParticle p;
    
    // Start position with slight random offset around crash point
    p.position.x = emitPosition.x + randomFloat(-2.0f, 2.0f);
    p.position.y = emitPosition.y + randomFloat(0.0f, 1.0f);
    p.position.z = emitPosition.z + randomFloat(-2.0f, 2.0f);
    
    // Velocity - mostly upward with slight horizontal drift
    p.velocity.x = randomFloat(-3.0f, 3.0f);
    p.velocity.y = randomFloat(8.0f, 15.0f);  // Rise upward
    p.velocity.z = randomFloat(-3.0f, 3.0f);
    
    // Size properties - starts small, grows large
    p.startSize = randomFloat(1.0f, 2.0f);
    p.size = p.startSize;
    p.maxSize = randomFloat(12.0f, 20.0f);
    
    // Lifetime
    p.maxLife = randomFloat(2.5f, 4.5f);
    p.life = p.maxLife;
    
    // Start mostly opaque
    p.alpha = randomFloat(0.6f, 0.9f);
    
    // Random rotation for variety
    p.rotation = randomFloat(0.0f, 360.0f);
    p.rotationSpeed = randomFloat(-45.0f, 45.0f);
    
    p.active = true;
    
    // Find inactive particle slot or add new one
    bool found = false;
    for (size_t i = 0; i < particles.size(); i++) {
        if (!particles[i].active) {
            particles[i] = p;
            found = true;
            break;
        }
    }
    if (!found && particles.size() < 200) {
        particles.push_back(p);
    }
}

void SmokeSystem::update(float deltaTime) {
    // Spawn new particles if smoke is active
    if (smokeActive) {
        emitTimer += deltaTime;
        spawnTimer += deltaTime;
        
        // Spawn multiple particles per frame for thick smoke
        while (spawnTimer >= spawnRate) {
            spawnParticle();
            spawnTimer -= spawnRate;
        }
        
        // Stop emitting after duration (but particles continue to exist)
        if (emitTimer >= emitDuration) {
            smokeActive = false;
        }
    }
    
    // Update all active particles
    for (size_t i = 0; i < particles.size(); i++) {
        if (!particles[i].active) continue;
        
        SmokeParticle& p = particles[i];
        
        // Decrease lifetime
        p.life -= deltaTime;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }
        
        // Calculate life ratio (1.0 = just born, 0.0 = about to die)
        float lifeRatio = p.life / p.maxLife;
        float ageRatio = 1.0f - lifeRatio;  // 0.0 = just born, 1.0 = old
        
        // Update position
        p.position.x += p.velocity.x * deltaTime;
        p.position.y += p.velocity.y * deltaTime;
        p.position.z += p.velocity.z * deltaTime;
        
        // Slow down horizontal drift over time (air resistance)
        p.velocity.x *= (1.0f - 0.5f * deltaTime);
        p.velocity.z *= (1.0f - 0.5f * deltaTime);
        
        // Slow down vertical rise as it ages
        p.velocity.y *= (1.0f - 0.3f * deltaTime);
        
        // Size grows from startSize to maxSize over lifetime
        // Quick initial growth, then slower
        float growthCurve = 1.0f - pow(1.0f - ageRatio, 2.0f);  // Ease-out
        p.size = p.startSize + (p.maxSize - p.startSize) * growthCurve;
        
        // Alpha fades out in the last 40% of lifetime
        if (lifeRatio < 0.4f) {
            p.alpha = (lifeRatio / 0.4f) * 0.7f;  // Fade from 0.7 to 0
        } else if (ageRatio < 0.2f) {
            // Fade in slightly at the start
            p.alpha = (ageRatio / 0.2f) * 0.7f + 0.1f;
        }
        
        // Rotate
        p.rotation += p.rotationSpeed * deltaTime;
    }
}

void SmokeSystem::render(const Vector3f& cameraPosition) {
    if (particles.empty()) return;
    
    // Save OpenGL state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Enable texture
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Disable lighting for particles (they should be self-illuminated)
    glDisable(GL_LIGHTING);
    
    // Disable depth writing (but keep depth testing for proper ordering with scene)
    glDepthMask(GL_FALSE);
    
    // Sort particles by distance to camera (back to front) for proper blending
    // Create index array for sorting
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
    
    // Sort back to front (farthest first)
    std::sort(sortedIndices.begin(), sortedIndices.end(), 
              [](const std::pair<float, size_t>& a, const std::pair<float, size_t>& b) {
                  return a.first > b.first;
              });
    
    // Get the current modelview matrix for billboarding
    float modelview[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    
    // Extract camera right and up vectors from modelview matrix
    Vector3f camRight(modelview[0], modelview[4], modelview[8]);
    Vector3f camUp(modelview[1], modelview[5], modelview[9]);
    
    // Render each particle as a billboard quad
    glBegin(GL_QUADS);
    
    for (size_t idx = 0; idx < sortedIndices.size(); idx++) {
        const SmokeParticle& p = particles[sortedIndices[idx].second];
        
        float halfSize = p.size * 0.5f;
        
        // Apply rotation to the billboard
        float rad = p.rotation * 3.14159f / 180.0f;
        float cosR = cos(rad);
        float sinR = sin(rad);
        
        // Rotated right and up vectors
        Vector3f right = camRight * cosR + camUp * sinR;
        Vector3f up = camUp * cosR - camRight * sinR;
        
        // Smoke color (dark gray to light gray)
        float gray = 0.3f + (1.0f - p.life / p.maxLife) * 0.4f;  // Gets lighter as it ages
        glColor4f(gray, gray, gray, p.alpha);
        
        // Calculate quad corners
        Vector3f bottomLeft = p.position - right * halfSize - up * halfSize;
        Vector3f bottomRight = p.position + right * halfSize - up * halfSize;
        Vector3f topRight = p.position + right * halfSize + up * halfSize;
        Vector3f topLeft = p.position - right * halfSize + up * halfSize;
        
        // Draw quad with texture coordinates
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
    
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glPopAttrib();
}
