#pragma once
#include "Level.h"
#include <map>
#include <string>

class GameManager {
public:
    // Singleton pattern
    static GameManager& getInstance();
    
    // Prevent copying
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;
    
    // Level management
    void registerLevel(const std::string& name, Level* level);
    void switchToLevel(const std::string& name);
    void unloadLevel(const std::string& name);
    void unloadAllLevels();
    
    // Core game loop methods
    void update(float deltaTime);
    void render();
    
    // Input forwarding
    void handleKeyboard(unsigned char key, bool pressed);
    void handleKeyboardUp(unsigned char key);
    void handleSpecialKeys(int key, int x, int y);
    void handleMouse(int x, int y);
    void handleMouseClick(int button, int state, int x, int y);
    
    // Getters
    Level* getCurrentLevel() const { return currentLevel; }
    const std::string& getCurrentLevelName() const { return currentLevelName; }
    
    // Cleanup
    void cleanup();
    
private:
    GameManager();
    ~GameManager();
    
    std::map<std::string, Level*> levels;
    Level* currentLevel;
    std::string currentLevelName;
};
