#include "SkySystem.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>

SkySystem::SkySystem() : tex_sky(0), sunIntensity(1.0f) {
    // Default sun direction (high and slightly to the right)
    sunDirection = Vector3f(0.4f, 0.85f, -0.35f);
    float len = sqrt(sunDirection.x*sunDirection.x + sunDirection.y*sunDirection.y + sunDirection.z*sunDirection.z);
    sunDirection.x /= len;
    sunDirection.y /= len;
    sunDirection.z /= len;
    
    for (int i = 0; i < 6; i++) {
        tex_flare[i] = 0;
    }
}

SkySystem::~SkySystem() {
    if (tex_sky != 0) {
        glDeleteTextures(1, &tex_sky);
    }
    for (int i = 0; i < 6; i++) {
        if (tex_flare[i] != 0) {
            glDeleteTextures(1, &tex_flare[i]);
        }
    }
}

void SkySystem::init() {
    loadSkyTexture("textures/sky.bmp");
    generateFlareTextures();
}

void SkySystem::setSunDirection(const Vector3f& dir) {
    sunDirection = dir;
    float len = sqrt(sunDirection.x*sunDirection.x + sunDirection.y*sunDirection.y + sunDirection.z*sunDirection.z);
    if (len > 0.001f) {
        sunDirection.x /= len;
        sunDirection.y /= len;
        sunDirection.z /= len;
    }
}

bool SkySystem::loadSkyTexture(const char* filename) {
    FILE* file = NULL;
    fopen_s(&file, filename, "rb");
    if (!file) {
        // Fallback: generate procedural sky if file not found
        const int width = 512;
        const int height = 256;
        unsigned char* data = new unsigned char[width * height * 3];
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float v = (float)y / (float)height;
                float horizonR = 0.75f, horizonG = 0.85f, horizonB = 0.95f;
                float zenithR = 0.2f, zenithG = 0.4f, zenithB = 0.8f;
                float t = v * v;
                float r = horizonR + (zenithR - horizonR) * t;
                float g = horizonG + (zenithG - horizonG) * t;
                float b = horizonB + (zenithB - horizonB) * t;
                int idx = (y * width + x) * 3;
                data[idx + 0] = (unsigned char)(r * 255);
                data[idx + 1] = (unsigned char)(g * 255);
                data[idx + 2] = (unsigned char)(b * 255);
            }
        }
        glGenTextures(1, &tex_sky);
        glBindTexture(GL_TEXTURE_2D, tex_sky);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        delete[] data;
        return false;
    }
    
    // Read BMP header
    unsigned char header[54];
    size_t headerRead = fread(header, 1, 54, file);
    if (headerRead < 54 || header[0] != 'B' || header[1] != 'M') {
        fclose(file);
        return false;
    }
    
    // Extract dimensions from header
    int width = *(int*)&header[18];
    int height = *(int*)&header[22];
    int bitsPerPixel = *(short*)&header[28];
    int dataOffset = *(int*)&header[10];
    
    if (width <= 0 || height <= 0) {
        fclose(file);
        return false;
    }
    
    // Seek to pixel data
    fseek(file, dataOffset, SEEK_SET);
    
    // Calculate row size with padding (BMP rows are 4-byte aligned)
    int bytesPerPixel = bitsPerPixel / 8;
    int rowSize = ((width * bytesPerPixel + 3) / 4) * 4;
    int imageSize = rowSize * abs(height);
    
    unsigned char* bmpData = new unsigned char[imageSize];
    size_t bytesRead = fread(bmpData, 1, imageSize, file);
    fclose(file);
    
    if (bytesRead == 0) {
        delete[] bmpData;
        return false;
    }
    
    // Convert BGR to RGB and flip vertically
    unsigned char* rgbData = new unsigned char[width * abs(height) * 3];
    int absHeight = abs(height);
    
    for (int y = 0; y < absHeight; y++) {
        for (int x = 0; x < width; x++) {
            int srcY = (height > 0) ? (absHeight - 1 - y) : y;  // BMP is bottom-up if height > 0
            int srcIdx = srcY * rowSize + x * bytesPerPixel;
            int dstIdx = (y * width + x) * 3;
            
            if (bytesPerPixel >= 3) {
                rgbData[dstIdx + 0] = bmpData[srcIdx + 2];  // R
                rgbData[dstIdx + 1] = bmpData[srcIdx + 1];  // G
                rgbData[dstIdx + 2] = bmpData[srcIdx + 0];  // B
            }
        }
    }
    
    delete[] bmpData;
    
    glGenTextures(1, &tex_sky);
    glBindTexture(GL_TEXTURE_2D, tex_sky);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, absHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, absHeight, GL_RGB, GL_UNSIGNED_BYTE, rgbData);
    
    delete[] rgbData;
    return true;
}

void SkySystem::generateFlareTextures() {
    for (int i = 0; i < 6; i++) {
        glGenTextures(1, &tex_flare[i]);
        glBindTexture(GL_TEXTURE_2D, tex_flare[i]);
        
        const int size = 128;
        const float halfSize = size / 2.0f;
        unsigned char* data = new unsigned char[size * size * 4];
        
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                float dx = (x - halfSize) / halfSize;
                float dy = (y - halfSize) / halfSize;
                float dist = sqrt(dx*dx + dy*dy);
                
                float alpha = 0.0f;
                float r = 1.0f, g = 1.0f, b = 1.0f;
                
                if (i == 0) {
                    // Main sun glow - large soft circle
                    alpha = (1.0f - dist) * 0.8f;
                    if (alpha < 0) alpha = 0;
                    alpha = alpha * alpha;
                    r = 1.0f; g = 0.95f; b = 0.8f;
                } else if (i == 1) {
                    // Bright core
                    alpha = pow(fmax(0.0f, 1.0f - dist), 4.0f);
                    if (dist > 1.0f) alpha = 0;
                    r = 1.0f; g = 1.0f; b = 1.0f;
                } else if (i == 2) {
                    // Hexagonal flare (ring)
                    float ring = fabs(dist - 0.6f);
                    alpha = (0.15f - ring) * 3.0f;
                    if (alpha < 0) alpha = 0;
                    r = 0.8f; g = 0.9f; b = 1.0f;
                } else if (i == 3) {
                    // Green/yellow circle
                    alpha = fmax(0.0f, (1.0f - dist * 1.5f) * 0.5f);
                    r = 0.7f; g = 1.0f; b = 0.5f;
                } else if (i == 4) {
                    // Small bright spot
                    alpha = pow(fmax(0.0f, 1.0f - dist * 2.0f), 3.0f);
                    if (dist > 0.5f) alpha = 0;
                    r = 1.0f; g = 0.8f; b = 0.6f;
                } else {
                    // Outer ring flare
                    float ring = fabs(dist - 0.8f);
                    alpha = (0.1f - ring) * 5.0f;
                    if (alpha < 0) alpha = 0;
                    r = 0.9f; g = 0.7f; b = 1.0f;
                }
                
                int idx = (y * size + x) * 4;
                data[idx + 0] = (unsigned char)(r * 255);
                data[idx + 1] = (unsigned char)(g * 255);
                data[idx + 2] = (unsigned char)(b * 255);
                data[idx + 3] = (unsigned char)(alpha * 255);
            }
        }
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        
        delete[] data;
    }
}

void SkySystem::renderSky(const Vector3f& playerPosition) {
    glPushMatrix();
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // Center on player
    glTranslatef(playerPosition.x, playerPosition.y, playerPosition.z);
    glRotated(90, 1, 0, 1);
    
    // Setup for sky rendering
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex_sky);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Render sky sphere
    GLUquadricObj* qobj = gluNewQuadric();
    gluQuadricTexture(qobj, true);
    gluQuadricNormals(qobj, GL_SMOOTH);
    gluSphere(qobj, 800, 100, 100);
    gluDeleteQuadric(qobj);
    
    glPopAttrib();
    glPopMatrix();
}

bool SkySystem::isSunVisible(const Vector3f& playerPosition, const Vector3f& playerForward,
                              int screenWidth, int screenHeight, float& screenX, float& screenY) {
    // Get sun position in world space
    Vector3f sunPos = playerPosition + sunDirection * 500.0f;
    
    // Get current matrices
    GLdouble modelview[16], projection[16];
    GLint viewport[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);
    
    // Project sun to screen space
    GLdouble winX, winY, winZ;
    gluProject(sunPos.x, sunPos.y, sunPos.z, modelview, projection, viewport, &winX, &winY, &winZ);
    
    screenX = (float)winX;
    screenY = (float)winY;
    
    // Check if sun is in front of camera
    if (winZ < 0 || winZ > 1) return false;
    if (screenX < -100 || screenX > screenWidth + 100) return false;
    if (screenY < -100 || screenY > screenHeight + 100) return false;
    
    // Check dot product with forward vector
    float dot = sunDirection.x * playerForward.x +
                sunDirection.y * playerForward.y +
                sunDirection.z * playerForward.z;
    
    if (dot > 0.3f) return false;  // Sun behind us
    
    return true;
}

void SkySystem::renderLensFlare(const Vector3f& playerPosition, const Vector3f& playerForward,
                                 int screenWidth, int screenHeight) {
    float sunX, sunY;
    if (!isSunVisible(playerPosition, playerForward, screenWidth, screenHeight, sunX, sunY)) {
        return;
    }
    
    // Calculate center of screen (use floats to avoid conversion warnings)
    float fScreenW = (float)screenWidth;
    float fScreenH = (float)screenHeight;
    float centerX = fScreenW / 2.0f;
    float centerY = fScreenH / 2.0f;
    
    // Vector from sun to center
    float dx = centerX - sunX;
    float dy = centerY - sunY;
    
    // Calculate intensity based on sun position
    float distFromCenter = sqrt(dx*dx + dy*dy);
    float maxDist = sqrt(centerX*centerX + centerY*centerY);
    sunIntensity = 1.0f - (distFromCenter / maxDist) * 0.5f;
    
    // Save state
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // Setup 2D rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, (GLdouble)screenWidth, 0, (GLdouble)screenHeight);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending
    glEnable(GL_TEXTURE_2D);
    
    // Flare element configuration
    float flarePositions[] = { 0.0f, 0.1f, 0.4f, 0.6f, 1.0f, 1.5f };
    float flareSizes[] = { 150.0f, 80.0f, 40.0f, 60.0f, 30.0f, 50.0f };
    float flareAlphas[] = { 0.4f, 0.8f, 0.3f, 0.25f, 0.5f, 0.2f };
    
    for (int i = 0; i < 6; i++) {
        float t = flarePositions[i];
        float fx = sunX + dx * t;
        float fy = sunY + dy * t;
        float size = flareSizes[i] * sunIntensity;
        float alpha = flareAlphas[i] * sunIntensity;
        
        glBindTexture(GL_TEXTURE_2D, tex_flare[i]);
        glColor4f(1.0f, 1.0f, 1.0f, alpha);
        
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(fx - size, fy - size);
        glTexCoord2f(1, 0); glVertex2f(fx + size, fy - size);
        glTexCoord2f(1, 1); glVertex2f(fx + size, fy + size);
        glTexCoord2f(0, 1); glVertex2f(fx - size, fy + size);
        glEnd();
    }
    
    // Anamorphic streak
    glBindTexture(GL_TEXTURE_2D, tex_flare[1]);
    glColor4f(1.0f, 0.95f, 0.8f, 0.15f * sunIntensity);
    float streakWidth = 400.0f * sunIntensity;
    float streakHeight = 8.0f;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0.4f); glVertex2f(sunX - streakWidth, sunY - streakHeight);
    glTexCoord2f(1, 0.4f); glVertex2f(sunX + streakWidth, sunY - streakHeight);
    glTexCoord2f(1, 0.6f); glVertex2f(sunX + streakWidth, sunY + streakHeight);
    glTexCoord2f(0, 0.6f); glVertex2f(sunX - streakWidth, sunY + streakHeight);
    glEnd();
    
    // Restore state
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glPopAttrib();
}
