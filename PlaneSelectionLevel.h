#pragma once
#include "glew.h"
#include "Level.h"
#include "Model_3DS.h"

class PlaneSelectionLevel : public Level {
public:
    PlaneSelectionLevel();
    ~PlaneSelectionLevel() override;
    
    void init() override;
    void update(float deltaTime) override;
    void render() override;
    void cleanup() override;
    
    void handleKeyboard(unsigned char key, bool pressed) override;
    void handleKeyboardUp(unsigned char key) override;
    void handleMouse(int x, int y) override;
    void handleSpecialKeys(int key, bool pressed);
    
    void onEnter() override;
    void onExit() override;
    
    static int getSelectedPlane() { return selectedPlane; }
    static void setSelectedPlane(int plane) { if (plane >= 0 && plane < 3) selectedPlane = plane; }
    
private:
    void renderPlanePreview(int planeIndex, float xPos, float yPos, float zPos);
    void renderUI();
    
    Model_3DS model_plane1;
    Model_3DS model_plane2;
    Model_3DS model_plane3;

    GLuint tex_plane1;
    GLuint tex_plane2;
    GLuint tex_plane3;
    
    static int selectedPlane;  // 0 = plane1, 1 = plane2, 2 = plane3
    int highlightedPlane;      // Currently highlighted plane
    
    float rotationAngle;
    float pulseTimer;
    
    int screenWidth;
    int screenHeight;
};
