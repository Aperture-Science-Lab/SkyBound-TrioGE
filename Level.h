#pragma once

class Level {
public:
    Level();
    virtual ~Level();
    
    // Core level lifecycle methods
    virtual void init() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void render() = 0;
    virtual void cleanup() = 0;
    
    // Input handling
    virtual void handleKeyboard(unsigned char key, bool pressed) = 0;
    virtual void handleKeyboardUp(unsigned char key) = 0;
    virtual void handleSpecialKeys(int key, int x, int y) {}
    virtual void handleMouse(int x, int y) = 0;
    virtual void handleMouseClick(int button, int state, int x, int y) {}
    
    // Utility methods
    virtual void onEnter();
    virtual void onExit();
    
    bool isActive() const { return active; }
    void setActive(bool value) { active = value; }
    
protected:
    bool active;
};
