#pragma once
#include "Level.h"

class OptionsMenu : public Level {
public:
    OptionsMenu();
    ~OptionsMenu();
    
    void init() override;
    void update(float deltaTime) override;
    void render() override;
    void cleanup() override;
    
    void handleKeyboard(unsigned char key, bool pressed) override;
    void handleKeyboardUp(unsigned char key) override;
    void handleMouse(int x, int y) override;
    void handleMouseClick(int button, int state, int x, int y) override;
    
private:
    enum MenuSection {
        LEVEL_SELECT,
        PLANE_SELECT,
        BACK
    };
    
    struct MenuItem {
        const char* label;
        bool selected;
    };
    
    MenuSection currentSection;
    int selectedLevelIndex;
    int selectedPlaneIndex;
    bool changePlaneHovered;
    float pulseTimer;
    int screenWidth;
    int screenHeight;
    
    // Button hover detection
    bool isMouseOverButton(int mx, int my, float x, float y, float w, float h);
    void drawButton(const char* text, float x, float y, float w, float h, bool highlighted, bool pressed);
    void drawSection(const char* title, float startY);
    void drawLevelSelect();
    void drawPlaneSelect();
    void drawTitle();
};
