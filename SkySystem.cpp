#include "SkySystem.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>

SkySystem::SkySystem() : tex_sky(0), sunIntensity(1.0f) {
    // Sun direction matching the sky.bmp texture - the orange sunset area
    // The sunset appears on the opposite side of the sky sphere from current view
    // Pointing towards positive Z (behind initial camera) and low on horizon
    sunDirection = Vector3f(0.0f, 0.25f, 0.97f);  // Low sun, behind player (towards sunset)
    float len = sqrt(sunDirection.x*sunDirection.x + sunDirection.y*sunDirection.y + sunDirection.z*sunDirection.z);
    sunDirection.x /= len;
    sunDirection.y /= len;
    sunDirection.z /= len;
    
    for (int i = 0; i < 10; i++) {
        tex_flare[i] = 0;
    }
}

SkySystem::~SkySystem() {
    if (tex_sky != 0) {
        glDeleteTextures(1, &tex_sky);
    }
    for (int i = 0; i < 10; i++) {
        if (tex_flare[i] != 0) {
            glDeleteTextures(1, &tex_flare[i]);
        }
    }
}

void SkySystem::init() {
    // Try multiple paths for sky texture
    if (!loadSkyTexture("../textures/sky.bmp")) {
        loadSkyTexture("textures/sky.bmp");
    }
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
    // Generate 10 high-quality AAA lens flare textures
    for (int i = 0; i < 10; i++) {
        glGenTextures(1, &tex_flare[i]);
        glBindTexture(GL_TEXTURE_2D, tex_flare[i]);
        
        const int size = 256;  // Higher resolution for quality
        const float halfSize = size / 2.0f;
        unsigned char* data = new unsigned char[size * size * 4];
        
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                float dx = (x - halfSize) / halfSize;
                float dy = (y - halfSize) / halfSize;
                float dist = sqrt(dx*dx + dy*dy);
                float angle = atan2(dy, dx);
                
                float alpha = 0.0f;
                float r = 1.0f, g = 1.0f, b = 1.0f;
                
                switch(i) {
                    case 0: // Sun corona - large soft warm glow
                        alpha = pow(fmax(0.0f, 1.0f - dist), 2.5f);
                        r = 1.0f; g = 0.9f; b = 0.7f;
                        break;
                        
                    case 1: // Bright white core with rays
                        {
                            float core = pow(fmax(0.0f, 1.0f - dist * 2.0f), 6.0f);
                            float rays = pow(fmax(0.0f, 1.0f - dist), 2.0f) * 
                                         (0.5f + 0.5f * pow(fabs(sin(angle * 8.0f)), 8.0f));
                            alpha = fmin(1.0f, core + rays * 0.3f);
                            r = 1.0f; g = 1.0f; b = 1.0f;
                        }
                        break;
                        
                    case 2: // Blue/cyan ring flare
                        {
                            float ring = 1.0f - fabs(dist - 0.5f) * 5.0f;
                            alpha = fmax(0.0f, ring) * 0.6f;
                            r = 0.6f; g = 0.85f; b = 1.0f;
                        }
                        break;
                        
                    case 3: // Orange/gold circle
                        alpha = pow(fmax(0.0f, 1.0f - dist * 1.2f), 3.0f) * 0.7f;
                        r = 1.0f; g = 0.7f; b = 0.3f;
                        break;
                        
                    case 4: // Green ghost circle
                        alpha = pow(fmax(0.0f, 1.0f - dist * 1.5f), 2.0f) * 0.5f;
                        r = 0.5f; g = 1.0f; b = 0.6f;
                        break;
                        
                    case 5: // Purple/magenta spot
                        alpha = pow(fmax(0.0f, 1.0f - dist * 2.0f), 4.0f) * 0.8f;
                        r = 1.0f; g = 0.5f; b = 0.9f;
                        break;
                        
                    case 6: // Rainbow ring (chromatic aberration)
                        {
                            float ring = 1.0f - fabs(dist - 0.7f) * 8.0f;
                            alpha = fmax(0.0f, ring) * 0.4f;
                            // Chromatic colors based on angle
                            r = 0.8f + 0.2f * sin(angle * 2.0f);
                            g = 0.8f + 0.2f * sin(angle * 2.0f + 2.1f);
                            b = 0.8f + 0.2f * sin(angle * 2.0f + 4.2f);
                        }
                        break;
                        
                    case 7: // Hexagonal bokeh
                        {
                            // 6-sided shape
                            float hex = 0.0f;
                            for (int k = 0; k < 6; k++) {
                                float a = k * 3.14159f / 3.0f;
                                hex = fmax(hex, fabs(dx * cos(a) + dy * sin(a)));
                            }
                            alpha = pow(fmax(0.0f, 1.0f - hex * 1.5f), 2.0f) * 0.5f;
                            r = 1.0f; g = 0.95f; b = 0.85f;
                        }
                        break;
                        
                    case 8: // Anamorphic streak texture
                        {
                            float streak = exp(-dy * dy * 50.0f) * exp(-dx * dx * 0.5f);
                            alpha = streak * 0.8f;
                            r = 1.0f; g = 0.95f; b = 0.9f;
                        }
                        break;
                        
                    case 9: // Starburst
                        {
                            float star = 0.0f;
                            for (int k = 0; k < 6; k++) {
                                float a = k * 3.14159f / 3.0f;
                                float rayDist = fabs(sin(angle - a));
                                star += exp(-rayDist * 20.0f) * exp(-dist * 3.0f);
                            }
                            alpha = fmin(1.0f, star) * 0.6f;
                            r = 1.0f; g = 0.98f; b = 0.9f;
                        }
                        break;
                }
                
                // Clamp values
                if (alpha < 0) alpha = 0;
                if (alpha > 1) alpha = 1;
                if (r > 1) r = 1; if (g > 1) g = 1; if (b > 1) b = 1;
                
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
    
    // Rotate sky sphere so texture horizon aligns with world horizon
    // Rotate 90 degrees around X axis so texture wraps correctly
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    
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
    
    // More lenient visibility check - allow flare even when sun is partially off-screen
    if (winZ < 0 || winZ > 1) return false;
    
    // Allow sun to be off-screen for flare effect
    float margin = 300.0f;
    if (screenX < -margin || screenX > screenWidth + margin) return false;
    if (screenY < -margin || screenY > screenHeight + margin) return false;
    
    // Always show lens flare when sun is visible in the hemisphere
    // No dot product check - let the projection handle visibility
    
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
    
    // Calculate intensity - balanced for natural look
    float distFromCenter = sqrt(dx*dx + dy*dy);
    float maxDist = sqrt(centerX*centerX + centerY*centerY);
    sunIntensity = 0.8f - (distFromCenter / maxDist) * 0.2f;  // Reduced intensity for natural look
    if (sunIntensity > 0.8f) sunIntensity = 0.8f;
    
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
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for realistic light
    glEnable(GL_TEXTURE_2D);
    
    // AAA-quality flare configuration: position along sun-center line, size, alpha, texture index
    // Positions: 0.0 = at sun, 1.0 = at center, >1.0 = past center
    struct FlareElement {
        float position;
        float size;
        float alpha;
        int textureIndex;
        float r, g, b;
    };
    
    // Reduced alpha values for more subtle, natural look
    FlareElement flares[] = {
        // Main sun glow and corona - visible but not overwhelming
        { 0.0f,  280.0f, 0.35f, 0, 1.0f, 0.95f, 0.85f },  // Warm corona
        { 0.0f,  140.0f, 0.5f,  1, 1.0f, 1.0f, 1.0f },    // Core with rays
        { 0.0f,  200.0f, 0.25f, 9, 1.0f, 0.98f, 0.9f },   // Starburst
        
        // Ghost flares along the line - subtle
        { 0.15f, 60.0f,  0.2f,  2, 0.7f, 0.85f, 1.0f },   // Blue ring near sun
        { 0.3f,  45.0f,  0.25f, 3, 1.0f, 0.8f, 0.4f },    // Orange spot
        { 0.45f, 80.0f,  0.18f, 7, 1.0f, 0.95f, 0.85f },  // Hexagonal bokeh
        { 0.6f,  40.0f,  0.22f, 4, 0.6f, 1.0f, 0.7f },    // Green ghost
        { 0.75f, 55.0f,  0.18f, 6, 1.0f, 1.0f, 1.0f },    // Rainbow ring
        { 0.9f,  45.0f,  0.2f,  5, 1.0f, 0.6f, 0.9f },    // Purple spot near center
        { 1.1f,  35.0f,  0.18f, 3, 1.0f, 0.7f, 0.3f },    // Orange past center
        { 1.3f,  60.0f,  0.15f, 2, 0.5f, 0.8f, 1.0f },    // Blue ring far
        { 1.5f,  30.0f,  0.2f,  4, 0.7f, 1.0f, 0.8f },    // Green far
        { 1.7f,  70.0f,  0.12f, 6, 1.0f, 1.0f, 1.0f },    // Rainbow far
    };
    
    int numFlares = sizeof(flares) / sizeof(flares[0]);
    
    for (int i = 0; i < numFlares; i++) {
        float t = flares[i].position;
        float fx = sunX + dx * t;
        float fy = sunY + dy * t;
        float size = flares[i].size * sunIntensity;
        float alpha = flares[i].alpha * sunIntensity;
        
        // Fade flares that are off-screen
        float screenMargin = 50.0f;
        if (fx < -screenMargin || fx > fScreenW + screenMargin ||
            fy < -screenMargin || fy > fScreenH + screenMargin) {
            continue;
        }
        
        glBindTexture(GL_TEXTURE_2D, tex_flare[flares[i].textureIndex]);
        glColor4f(flares[i].r, flares[i].g, flares[i].b, alpha);
        
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(fx - size, fy - size);
        glTexCoord2f(1, 0); glVertex2f(fx + size, fy - size);
        glTexCoord2f(1, 1); glVertex2f(fx + size, fy + size);
        glTexCoord2f(0, 1); glVertex2f(fx - size, fy + size);
        glEnd();
    }
    
    // AAA Anamorphic streak (horizontal light streak through sun - cinematic effect)
    glBindTexture(GL_TEXTURE_2D, tex_flare[8]);
    float streakIntensity = sunIntensity * 0.2f;  // More subtle
    float streakWidth = fScreenW * 0.3f * sunIntensity;  // Much shorter
    float streakHeight = 8.0f * sunIntensity;  // Thinner
    
    // Main streak - subtle
    glColor4f(1.0f, 0.95f, 0.85f, streakIntensity);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(sunX - streakWidth, sunY - streakHeight);
    glTexCoord2f(1, 0); glVertex2f(sunX + streakWidth, sunY - streakHeight);
    glTexCoord2f(1, 1); glVertex2f(sunX + streakWidth, sunY + streakHeight);
    glTexCoord2f(0, 1); glVertex2f(sunX - streakWidth, sunY + streakHeight);
    glEnd();
    
    // Secondary thinner streak - blue tint
    glColor4f(0.85f, 0.9f, 1.0f, streakIntensity * 0.4f);
    float streak2Height = 4.0f * sunIntensity;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(sunX - streakWidth * 1.1f, sunY - streak2Height);
    glTexCoord2f(1, 0); glVertex2f(sunX + streakWidth * 1.1f, sunY - streak2Height);
    glTexCoord2f(1, 1); glVertex2f(sunX + streakWidth * 1.1f, sunY + streak2Height);
    glTexCoord2f(0, 1); glVertex2f(sunX - streakWidth * 1.1f, sunY + streak2Height);
    glEnd();
    
    // Screen-wide bloom - subtle glow
    glBindTexture(GL_TEXTURE_2D, tex_flare[0]);
    glColor4f(1.0f, 0.97f, 0.9f, 0.08f * sunIntensity);
    float bloomSize = 450.0f * sunIntensity;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(sunX - bloomSize, sunY - bloomSize);
    glTexCoord2f(1, 0); glVertex2f(sunX + bloomSize, sunY - bloomSize);
    glTexCoord2f(1, 1); glVertex2f(sunX + bloomSize, sunY + bloomSize);
    glTexCoord2f(0, 1); glVertex2f(sunX - bloomSize, sunY + bloomSize);
    glEnd();
    
    // Restore state
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glPopAttrib();
}
