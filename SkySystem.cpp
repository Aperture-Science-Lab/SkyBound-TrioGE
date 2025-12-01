#include "SkySystem.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>

SkySystem::SkySystem() 
    : tex_sky_morning(0), tex_sky_noon(0), tex_sky_sunset(0), tex_sky_night(0),
      sunIntensity(1.0f), currentTime(TimeOfDay::MORNING), previousTime(TimeOfDay::MORNING),
      cycleTimer(0.0f), transitionProgress(0.0f), inTransition(false) {
    
    // Initial sun direction for morning
    sunDirection = Vector3f(0.5f, 0.3f, 0.8f);
    float len = sqrt(sunDirection.x*sunDirection.x + sunDirection.y*sunDirection.y + sunDirection.z*sunDirection.z);
    sunDirection.x /= len;
    sunDirection.y /= len;
    sunDirection.z /= len;
    
    for (int i = 0; i < 10; i++) {
        tex_flare[i] = 0;
    }
}

SkySystem::~SkySystem() {
    if (tex_sky_morning != 0) glDeleteTextures(1, &tex_sky_morning);
    if (tex_sky_noon != 0) glDeleteTextures(1, &tex_sky_noon);
    if (tex_sky_sunset != 0) glDeleteTextures(1, &tex_sky_sunset);
    if (tex_sky_night != 0) glDeleteTextures(1, &tex_sky_night);
    
    for (int i = 0; i < 10; i++) {
        if (tex_flare[i] != 0) {
            glDeleteTextures(1, &tex_flare[i]);
        }
    }
}

void SkySystem::init() {
    // Load all sky textures - try multiple paths
    if (!loadSkyTexture("textures/sky.bmp", tex_sky_morning)) {
        loadSkyTexture("../textures/sky.bmp", tex_sky_morning);
    }
    if (!loadSkyTexture("textures/noonsky.bmp", tex_sky_noon, true)) {
        loadSkyTexture("../textures/noonsky.bmp", tex_sky_noon, true);
    }
    if (!loadSkyTexture("textures/sunsetsky.bmp", tex_sky_sunset, true)) {
        loadSkyTexture("../textures/sunsetsky.bmp", tex_sky_sunset, true);
    }
    if (!loadSkyTexture("textures/nightsky.bmp", tex_sky_night, true)) {
        loadSkyTexture("../textures/nightsky.bmp", tex_sky_night, true);
    }
    
    generateFlareTextures();
    
    // Start at morning
    currentTime = TimeOfDay::MORNING;
    cycleTimer = 0.0f;
}

void SkySystem::update(float deltaTime) {
    cycleTimer += deltaTime;
    
    // Wrap around for continuous cycle
    if (cycleTimer >= fullCycleDuration) {
        cycleTimer -= fullCycleDuration;
    }
    
    // Determine current phase based on timer
    TimeOfDay newTime;
    float phaseTime = fmod(cycleTimer, phaseDuration);
    
    if (cycleTimer < phaseDuration) {
        newTime = TimeOfDay::MORNING;
    } else if (cycleTimer < phaseDuration * 2) {
        newTime = TimeOfDay::NOON;
    } else if (cycleTimer < phaseDuration * 3) {
        newTime = TimeOfDay::SUNSET;
    } else {
        newTime = TimeOfDay::NIGHT;
    }
    
    // Check for transition
    if (newTime != currentTime) {
        previousTime = currentTime;
        currentTime = newTime;
        inTransition = true;
        transitionProgress = 0.0f;
    }
    
    // Update transition progress
    if (inTransition) {
        transitionProgress += deltaTime / transitionDuration;
        if (transitionProgress >= 1.0f) {
            transitionProgress = 1.0f;
            inTransition = false;
        }
    }
    
    // Update sun position based on time
    updateSunPosition();
    
    // Apply lighting
    applyLighting();
}

void SkySystem::updateSunPosition() {
    float sunY, sunZ;
    
    switch (currentTime) {
        case TimeOfDay::MORNING:
            sunY = 0.3f;   // Low sun
            sunZ = 0.9f;   // Behind/side
            sunIntensity = 0.8f;
            break;
        case TimeOfDay::NOON:
            sunY = 0.8f;   // High sun
            sunZ = 0.5f;   // More overhead
            sunIntensity = 1.0f;
            break;
        case TimeOfDay::SUNSET:
            sunY = 0.15f;  // Very low
            sunZ = 0.95f;  // On horizon
            sunIntensity = 0.6f;
            break;
        case TimeOfDay::NIGHT:
            sunY = -0.3f;  // Below horizon (moon position)
            sunZ = 0.9f;
            sunIntensity = 0.0f;  // No sun
            break;
    }
    
    sunDirection = Vector3f(0.0f, sunY, sunZ);
    float len = sqrt(sunDirection.x*sunDirection.x + sunDirection.y*sunDirection.y + sunDirection.z*sunDirection.z);
    if (len > 0.001f) {
        sunDirection.x /= len;
        sunDirection.y /= len;
        sunDirection.z /= len;
    }
}

DayLighting SkySystem::getCurrentLighting() const {
    DayLighting lighting;
    
    switch (currentTime) {
        case TimeOfDay::MORNING:
            lighting.ambientR = 0.4f; lighting.ambientG = 0.4f; lighting.ambientB = 0.45f;
            lighting.diffuseR = 1.0f; lighting.diffuseG = 0.9f; lighting.diffuseB = 0.8f;
            lighting.sunHeight = 0.3f;
            lighting.showLensFlare = true;
            break;
        case TimeOfDay::NOON:
            lighting.ambientR = 0.5f; lighting.ambientG = 0.5f; lighting.ambientB = 0.5f;
            lighting.diffuseR = 1.0f; lighting.diffuseG = 1.0f; lighting.diffuseB = 0.95f;
            lighting.sunHeight = 0.8f;
            lighting.showLensFlare = true;
            break;
        case TimeOfDay::SUNSET:
            lighting.ambientR = 0.4f; lighting.ambientG = 0.3f; lighting.ambientB = 0.35f;
            lighting.diffuseR = 1.0f; lighting.diffuseG = 0.6f; lighting.diffuseB = 0.4f;
            lighting.sunHeight = 0.15f;
            lighting.showLensFlare = true;
            break;
        case TimeOfDay::NIGHT:
            lighting.ambientR = 0.1f; lighting.ambientG = 0.1f; lighting.ambientB = 0.15f;
            lighting.diffuseR = 0.2f; lighting.diffuseG = 0.2f; lighting.diffuseB = 0.3f;
            lighting.sunHeight = -0.3f;
            lighting.showLensFlare = false;  // No lens flare at night
            break;
    }
    
    return lighting;
}

void SkySystem::applyLighting() {
    DayLighting lighting = getCurrentLighting();
    
    GLfloat ambientLight[] = { lighting.ambientR, lighting.ambientG, lighting.ambientB, 1.0f };
    GLfloat diffuseLight[] = { lighting.diffuseR, lighting.diffuseG, lighting.diffuseB, 1.0f };
    GLfloat lightPosition[] = { sunDirection.x * 100.0f, sunDirection.y * 100.0f, sunDirection.z * 100.0f, 0.0f };
    
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
}

void SkySystem::setTimeOfDay(TimeOfDay time) {
    previousTime = currentTime;
    currentTime = time;
    
    // Set cycle timer to match
    switch (time) {
        case TimeOfDay::MORNING: cycleTimer = 0.0f; break;
        case TimeOfDay::NOON: cycleTimer = phaseDuration; break;
        case TimeOfDay::SUNSET: cycleTimer = phaseDuration * 2; break;
        case TimeOfDay::NIGHT: cycleTimer = phaseDuration * 3; break;
    }
    
    updateSunPosition();
    applyLighting();
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

unsigned int SkySystem::getTextureForTime(TimeOfDay time) {
    switch (time) {
        case TimeOfDay::MORNING: return tex_sky_morning;
        case TimeOfDay::NOON: return tex_sky_noon;
        case TimeOfDay::SUNSET: return tex_sky_sunset;
        case TimeOfDay::NIGHT: return tex_sky_night;
        default: return tex_sky_morning;
    }
}

bool SkySystem::loadSkyTexture(const char* filename, unsigned int& texId, bool flipVertical) {
    FILE* file = NULL;
    fopen_s(&file, filename, "rb");
    if (!file) {
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
            // Standard BMP flip, with optional extra flip for upside-down textures
            int srcY;
            if (flipVertical) {
                srcY = (height > 0) ? y : (absHeight - 1 - y);  // Opposite of normal
            } else {
                srcY = (height > 0) ? (absHeight - 1 - y) : y;  // Normal BMP flip
            }
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
    
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
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
        
        const int size = 256;
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
                    case 0: // Sun corona
                        alpha = pow(fmax(0.0f, 1.0f - dist), 2.5f);
                        r = 1.0f; g = 0.9f; b = 0.7f;
                        break;
                    case 1: // Bright white core with rays
                        {
                            float core = pow(fmax(0.0f, 1.0f - dist * 2.0f), 6.0f);
                            float rays = pow(fmax(0.0f, 1.0f - dist), 2.0f) * 
                                         (0.5f + 0.5f * pow(fabs(sin(angle * 8.0f)), 8.0f));
                            alpha = fmin(1.0f, core + rays * 0.3f);
                        }
                        break;
                    case 2: // Blue/cyan ring
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
                    case 4: // Green ghost
                        alpha = pow(fmax(0.0f, 1.0f - dist * 1.5f), 2.0f) * 0.5f;
                        r = 0.5f; g = 1.0f; b = 0.6f;
                        break;
                    case 5: // Purple/magenta
                        alpha = pow(fmax(0.0f, 1.0f - dist * 2.0f), 4.0f) * 0.8f;
                        r = 1.0f; g = 0.5f; b = 0.9f;
                        break;
                    case 6: // Rainbow ring
                        {
                            float ring = 1.0f - fabs(dist - 0.7f) * 8.0f;
                            alpha = fmax(0.0f, ring) * 0.4f;
                            r = 0.8f + 0.2f * sin(angle * 2.0f);
                            g = 0.8f + 0.2f * sin(angle * 2.0f + 2.1f);
                            b = 0.8f + 0.2f * sin(angle * 2.0f + 4.2f);
                        }
                        break;
                    case 7: // Hexagonal bokeh
                        {
                            float hex = 0.0f;
                            for (int k = 0; k < 6; k++) {
                                float a = k * 3.14159f / 3.0f;
                                hex = fmax(hex, fabs(dx * cos(a) + dy * sin(a)));
                            }
                            alpha = pow(fmax(0.0f, 1.0f - hex * 1.5f), 2.0f) * 0.5f;
                            r = 1.0f; g = 0.95f; b = 0.85f;
                        }
                        break;
                    case 8: // Anamorphic streak
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
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    
    // Setup for sky rendering
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    glDisable(GL_LIGHTING);
    
    // If in transition, blend between two skies
    if (inTransition && transitionProgress < 1.0f) {
        // First pass: previous sky (fading out)
        glBindTexture(GL_TEXTURE_2D, getTextureForTime(previousTime));
        float fadeOut = 1.0f - transitionProgress;
        glColor4f(fadeOut, fadeOut, fadeOut, 1.0f);
        
        GLUquadricObj* qobj = gluNewQuadric();
        gluQuadricTexture(qobj, true);
        gluQuadricNormals(qobj, GL_SMOOTH);
        gluSphere(qobj, 800, 100, 100);
        gluDeleteQuadric(qobj);
        
        // Second pass: current sky (fading in) with blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glBindTexture(GL_TEXTURE_2D, getTextureForTime(currentTime));
        glColor4f(1.0f, 1.0f, 1.0f, transitionProgress);
        
        qobj = gluNewQuadric();
        gluQuadricTexture(qobj, true);
        gluQuadricNormals(qobj, GL_SMOOTH);
        gluSphere(qobj, 799, 100, 100);  // Slightly smaller to avoid z-fighting
        gluDeleteQuadric(qobj);
    } else {
        // Normal rendering - single sky
        glBindTexture(GL_TEXTURE_2D, getTextureForTime(currentTime));
        glColor3f(1.0f, 1.0f, 1.0f);
        
        GLUquadricObj* qobj = gluNewQuadric();
        gluQuadricTexture(qobj, true);
        gluQuadricNormals(qobj, GL_SMOOTH);
        gluSphere(qobj, 800, 100, 100);
        gluDeleteQuadric(qobj);
    }
    
    glPopAttrib();
    glPopMatrix();
}

bool SkySystem::isSunVisible(const Vector3f& playerPosition, const Vector3f& playerForward,
                              int screenWidth, int screenHeight, float& screenX, float& screenY) {
    // No sun visibility at night
    if (currentTime == TimeOfDay::NIGHT) {
        return false;
    }
    
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
    
    if (winZ < 0 || winZ > 1) return false;
    
    float margin = 300.0f;
    if (screenX < -margin || screenX > screenWidth + margin) return false;
    if (screenY < -margin || screenY > screenHeight + margin) return false;
    
    return true;
}

void SkySystem::renderLensFlare(const Vector3f& playerPosition, const Vector3f& playerForward,
                                 int screenWidth, int screenHeight) {
    // No lens flare at night
    if (currentTime == TimeOfDay::NIGHT) {
        return;
    }
    
    float sunX, sunY;
    if (!isSunVisible(playerPosition, playerForward, screenWidth, screenHeight, sunX, sunY)) {
        return;
    }
    
    float fScreenW = (float)screenWidth;
    float fScreenH = (float)screenHeight;
    float centerX = fScreenW / 2.0f;
    float centerY = fScreenH / 2.0f;
    
    float dx = centerX - sunX;
    float dy = centerY - sunY;
    
    float distFromCenter = sqrt(dx*dx + dy*dy);
    float maxDist = sqrt(centerX*centerX + centerY*centerY);
    sunIntensity = 0.8f - (distFromCenter / maxDist) * 0.2f;
    if (sunIntensity > 0.8f) sunIntensity = 0.8f;
    
    // Reduce flare intensity during sunset
    if (currentTime == TimeOfDay::SUNSET) {
        sunIntensity *= 0.7f;
    }
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
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
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_TEXTURE_2D);
    
    struct FlareElement {
        float position;
        float size;
        float alpha;
        int textureIndex;
        float r, g, b;
    };
    
    FlareElement flares[] = {
        { 0.0f,  280.0f, 0.35f, 0, 1.0f, 0.95f, 0.85f },
        { 0.0f,  140.0f, 0.5f,  1, 1.0f, 1.0f, 1.0f },
        { 0.0f,  200.0f, 0.25f, 9, 1.0f, 0.98f, 0.9f },
        { 0.15f, 60.0f,  0.2f,  2, 0.7f, 0.85f, 1.0f },
        { 0.3f,  45.0f,  0.25f, 3, 1.0f, 0.8f, 0.4f },
        { 0.45f, 80.0f,  0.18f, 7, 1.0f, 0.95f, 0.85f },
        { 0.6f,  40.0f,  0.22f, 4, 0.6f, 1.0f, 0.7f },
        { 0.75f, 55.0f,  0.18f, 6, 1.0f, 1.0f, 1.0f },
        { 0.9f,  45.0f,  0.2f,  5, 1.0f, 0.6f, 0.9f },
        { 1.1f,  35.0f,  0.18f, 3, 1.0f, 0.7f, 0.3f },
        { 1.3f,  60.0f,  0.15f, 2, 0.5f, 0.8f, 1.0f },
        { 1.5f,  30.0f,  0.2f,  4, 0.7f, 1.0f, 0.8f },
        { 1.7f,  70.0f,  0.12f, 6, 1.0f, 1.0f, 1.0f },
    };
    
    int numFlares = sizeof(flares) / sizeof(flares[0]);
    
    for (int i = 0; i < numFlares; i++) {
        float t = flares[i].position;
        float fx = sunX + dx * t;
        float fy = sunY + dy * t;
        float size = flares[i].size * sunIntensity;
        float alpha = flares[i].alpha * sunIntensity;
        
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
    
    // Anamorphic streak
    glBindTexture(GL_TEXTURE_2D, tex_flare[8]);
    float streakIntensity = sunIntensity * 0.2f;
    float streakWidth = fScreenW * 0.3f * sunIntensity;
    float streakHeight = 8.0f * sunIntensity;
    
    glColor4f(1.0f, 0.95f, 0.85f, streakIntensity);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(sunX - streakWidth, sunY - streakHeight);
    glTexCoord2f(1, 0); glVertex2f(sunX + streakWidth, sunY - streakHeight);
    glTexCoord2f(1, 1); glVertex2f(sunX + streakWidth, sunY + streakHeight);
    glTexCoord2f(0, 1); glVertex2f(sunX - streakWidth, sunY + streakHeight);
    glEnd();
    
    // Secondary streak
    glColor4f(0.85f, 0.9f, 1.0f, streakIntensity * 0.4f);
    float streak2Height = 4.0f * sunIntensity;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(sunX - streakWidth * 1.1f, sunY - streak2Height);
    glTexCoord2f(1, 0); glVertex2f(sunX + streakWidth * 1.1f, sunY - streak2Height);
    glTexCoord2f(1, 1); glVertex2f(sunX + streakWidth * 1.1f, sunY + streak2Height);
    glTexCoord2f(0, 1); glVertex2f(sunX - streakWidth * 1.1f, sunY + streak2Height);
    glEnd();
    
    // Screen bloom
    glBindTexture(GL_TEXTURE_2D, tex_flare[0]);
    glColor4f(1.0f, 0.97f, 0.9f, 0.08f * sunIntensity);
    float bloomSize = 450.0f * sunIntensity;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(sunX - bloomSize, sunY - bloomSize);
    glTexCoord2f(1, 0); glVertex2f(sunX + bloomSize, sunY - bloomSize);
    glTexCoord2f(1, 1); glVertex2f(sunX + bloomSize, sunY + bloomSize);
    glTexCoord2f(0, 1); glVertex2f(sunX - bloomSize, sunY + bloomSize);
    glEnd();
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glPopAttrib();
}
