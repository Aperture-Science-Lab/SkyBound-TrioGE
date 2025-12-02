#include "ShadowSystem.h"
#include <cstdlib>

ShadowSystem::ShadowSystem() 
    : shadowDarkness(0.4f), shadowTexture(0) {
    // Default light direction (from above and slightly angled)
    lightDirection = Vector3f(0.3f, -0.9f, 0.2f);
    float len = sqrt(lightDirection.x*lightDirection.x + 
                     lightDirection.y*lightDirection.y + 
                     lightDirection.z*lightDirection.z);
    lightDirection.x /= len;
    lightDirection.y /= len;
    lightDirection.z /= len;
}

ShadowSystem::~ShadowSystem() {
    if (shadowTexture != 0) {
        glDeleteTextures(1, &shadowTexture);
    }
}

void ShadowSystem::init() {
    generateShadowTexture();
}

void ShadowSystem::generateShadowTexture() {
    // Generate a soft circular gradient texture for blob shadows
    const int size = 64;
    unsigned char* data = new unsigned char[size * size * 4];
    
    float center = size / 2.0f;
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (x - center) / center;
            float dy = (y - center) / center;
            float dist = sqrt(dx*dx + dy*dy);
            
            // Soft falloff using smoothstep-like curve
            float alpha = 0.0f;
            if (dist < 1.0f) {
                // Quadratic falloff for soft edges
                float t = 1.0f - dist;
                alpha = t * t;  // Soft circular gradient
            }
            
            int idx = (y * size + x) * 4;
            data[idx + 0] = 0;    // R
            data[idx + 1] = 0;    // G
            data[idx + 2] = 0;    // B
            data[idx + 3] = (unsigned char)(alpha * 255);  // A
        }
    }
    
    glGenTextures(1, &shadowTexture);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    delete[] data;
}

void ShadowSystem::setLightDirection(const Vector3f& dir) {
    lightDirection = dir;
    float len = sqrt(lightDirection.x*lightDirection.x + 
                     lightDirection.y*lightDirection.y + 
                     lightDirection.z*lightDirection.z);
    if (len > 0.001f) {
        lightDirection.x /= len;
        lightDirection.y /= len;
        lightDirection.z /= len;
    }
}

void ShadowSystem::setShadowDarkness(float darkness) {
    shadowDarkness = darkness;
    if (shadowDarkness < 0.0f) shadowDarkness = 0.0f;
    if (shadowDarkness > 1.0f) shadowDarkness = 1.0f;
}

void ShadowSystem::renderBlobShadow(const Vector3f& position, float radius, 
                                     float height, float maxHeight) {
    // Don't render shadows for objects too high
    if (height > maxHeight) return;
    
    // Calculate shadow intensity based on height
    // Higher objects have fainter, larger shadows
    float heightFactor = 1.0f - (height / maxHeight);
    float intensity = shadowDarkness * heightFactor * heightFactor;
    
    // Shadow gets slightly larger as object is higher
    float shadowRadius = radius * (1.0f + (height / maxHeight) * 0.5f);
    
    // Calculate shadow offset based on light direction
    // Project light onto ground plane
    float offsetX = 0.0f;
    float offsetZ = 0.0f;
    if (lightDirection.y < -0.1f) {
        float projectionFactor = height / (-lightDirection.y);
        offsetX = lightDirection.x * projectionFactor * 0.3f;  // Subtle offset
        offsetZ = lightDirection.z * projectionFactor * 0.3f;
    }
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    
    glDepthMask(GL_FALSE);  // Don't write to depth buffer
    
    // Render shadow quad on ground (slightly above to prevent z-fighting)
    float groundY = 0.02f;
    float sx = position.x + offsetX;
    float sz = position.z + offsetZ;
    
    glColor4f(0.0f, 0.0f, 0.0f, intensity);
    
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(sx - shadowRadius, groundY, sz - shadowRadius);
    glTexCoord2f(1, 0); glVertex3f(sx + shadowRadius, groundY, sz - shadowRadius);
    glTexCoord2f(1, 1); glVertex3f(sx + shadowRadius, groundY, sz + shadowRadius);
    glTexCoord2f(0, 1); glVertex3f(sx - shadowRadius, groundY, sz + shadowRadius);
    glEnd();
    
    glDepthMask(GL_TRUE);
    glPopAttrib();
}

void ShadowSystem::renderOvalShadow(const Vector3f& position, const Vector3f& forward,
                                     float length, float width, float height, float maxHeight) {
    // Don't render shadows for objects too high
    if (height > maxHeight) return;
    
    // Calculate shadow intensity based on height
    float heightFactor = 1.0f - (height / maxHeight);
    float intensity = shadowDarkness * heightFactor * heightFactor;
    
    // Shadow gets slightly larger as object is higher
    float shadowScale = 1.0f + (height / maxHeight) * 0.5f;
    float shadowLength = length * shadowScale;
    float shadowWidth = width * shadowScale;
    
    // Calculate shadow offset based on light direction
    float offsetX = 0.0f;
    float offsetZ = 0.0f;
    if (lightDirection.y < -0.1f) {
        float projectionFactor = height / (-lightDirection.y);
        offsetX = lightDirection.x * projectionFactor * 0.3f;
        offsetZ = lightDirection.z * projectionFactor * 0.3f;
    }
    
    // Get forward vector projected onto ground (XZ plane)
    Vector3f fwdXZ = Vector3f(forward.x, 0, forward.z);
    float fwdLen = sqrt(fwdXZ.x*fwdXZ.x + fwdXZ.z*fwdXZ.z);
    if (fwdLen < 0.001f) {
        fwdXZ = Vector3f(0, 0, -1);
    } else {
        fwdXZ.x /= fwdLen;
        fwdXZ.z /= fwdLen;
    }
    
    // Calculate right vector
    Vector3f rightXZ = Vector3f(-fwdXZ.z, 0, fwdXZ.x);
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    
    glDepthMask(GL_FALSE);
    
    float groundY = 0.02f;
    float sx = position.x + offsetX;
    float sz = position.z + offsetZ;
    
    glColor4f(0.0f, 0.0f, 0.0f, intensity);
    
    // Calculate oval corners
    Vector3f corners[4];
    corners[0] = Vector3f(sx - fwdXZ.x * shadowLength - rightXZ.x * shadowWidth, groundY,
                          sz - fwdXZ.z * shadowLength - rightXZ.z * shadowWidth);
    corners[1] = Vector3f(sx - fwdXZ.x * shadowLength + rightXZ.x * shadowWidth, groundY,
                          sz - fwdXZ.z * shadowLength + rightXZ.z * shadowWidth);
    corners[2] = Vector3f(sx + fwdXZ.x * shadowLength + rightXZ.x * shadowWidth, groundY,
                          sz + fwdXZ.z * shadowLength + rightXZ.z * shadowWidth);
    corners[3] = Vector3f(sx + fwdXZ.x * shadowLength - rightXZ.x * shadowWidth, groundY,
                          sz + fwdXZ.z * shadowLength - rightXZ.z * shadowWidth);
    
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(corners[0].x, corners[0].y, corners[0].z);
    glTexCoord2f(1, 0); glVertex3f(corners[1].x, corners[1].y, corners[1].z);
    glTexCoord2f(1, 1); glVertex3f(corners[2].x, corners[2].y, corners[2].z);
    glTexCoord2f(0, 1); glVertex3f(corners[3].x, corners[3].y, corners[3].z);
    glEnd();
    
    glDepthMask(GL_TRUE);
    glPopAttrib();
}

void ShadowSystem::renderBaseAO(const Vector3f& position, float radius, float intensity) {
    // Render ambient occlusion as a darker ring at the base of objects
    // This simulates the darkening that occurs where objects meet the ground
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glDepthMask(GL_FALSE);
    
    float groundY = 0.01f;
    int segments = 16;
    
    // Draw gradient ring
    glBegin(GL_TRIANGLE_FAN);
    
    // Center is darker
    glColor4f(0.0f, 0.0f, 0.0f, intensity);
    glVertex3f(position.x, groundY, position.z);
    
    // Outer edge is transparent
    glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 2.0f * 3.14159f;
        float x = position.x + cos(angle) * radius;
        float z = position.z + sin(angle) * radius;
        glVertex3f(x, groundY, z);
    }
    
    glEnd();
    
    glDepthMask(GL_TRUE);
    glPopAttrib();
}

void ShadowSystem::beginPlanarShadow(const Vector3f& lightDir, float groundY) {
    // Create a projection matrix that flattens geometry onto the ground plane
    // This is useful for rendering shadow volumes of 3D models
    
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPushMatrix();
    
    // Disable lighting and textures for shadow pass
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Enable stencil to prevent double-drawing shadows
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
    
    glDepthMask(GL_FALSE);
    
    // Build shadow projection matrix
    // Projects onto Y = groundY plane
    float d = -lightDir.y;
    if (fabs(d) < 0.001f) d = -0.001f;
    
    float shadowMat[16] = {
        d,              0,              0,              0,
        -lightDir.x,    0,              -lightDir.z,    -1,
        0,              0,              d,              0,
        0,              groundY * d,    0,              d
    };
    
    // This simplified version just translates down and scales Y
    glTranslatef(0, -groundY + 0.03f, 0);  // Move to ground level
    
    // Set shadow color
    glColor4f(0.0f, 0.0f, 0.0f, shadowDarkness * 0.3f);
}

void ShadowSystem::endPlanarShadow() {
    glDepthMask(GL_TRUE);
    glPopMatrix();
    glPopAttrib();
}
