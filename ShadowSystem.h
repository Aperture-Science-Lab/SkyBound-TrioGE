#pragma once
#ifndef SHADOW_SYSTEM_H
#define SHADOW_SYSTEM_H

#include "Vector3f.h"
#include "glew.h"
#include <glut.h>
#include <cmath>

// Simple shadow system for fixed-function OpenGL pipeline
// Uses projected blob shadows and fake ambient occlusion

class ShadowSystem {
public:
    ShadowSystem();
    ~ShadowSystem();
    
    void init();
    
    // Render a circular blob shadow under an object
    // position: world position of the object
    // radius: shadow radius
    // height: height of the object above ground (affects shadow intensity/size)
    // maxHeight: maximum height for shadow visibility
    void renderBlobShadow(const Vector3f& position, float radius, float height, float maxHeight = 100.0f);
    
    // Render an oval shadow for elongated objects (like the plane)
    void renderOvalShadow(const Vector3f& position, const Vector3f& forward, 
                          float length, float width, float height, float maxHeight = 100.0f);
    
    // Render ambient occlusion darkening at object base
    // Creates a gradient that darkens the ground near object bases
    void renderBaseAO(const Vector3f& position, float radius, float intensity = 0.5f);
    
    // Render a projected planar shadow from a model silhouette
    // Uses shadow matrix projection onto ground plane
    void beginPlanarShadow(const Vector3f& lightDir, float groundY = 0.0f);
    void endPlanarShadow();
    
    // Set light direction for shadows (normalized)
    void setLightDirection(const Vector3f& dir);
    
    // Adjust shadow darkness (0.0 = invisible, 1.0 = solid black)
    void setShadowDarkness(float darkness);
    
private:
    Vector3f lightDirection;
    float shadowDarkness;
    GLuint shadowTexture;
    
    void generateShadowTexture();
};

#endif
