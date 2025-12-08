#include "GameManager.h"
#include <iostream>
#include <glut.h>

GameManager::GameManager() : currentLevel(nullptr), currentLevelName("") {
}

GameManager::~GameManager() {
    cleanup();
}

GameManager& GameManager::getInstance() {
    static GameManager instance;
    return instance;
}

void GameManager::registerLevel(const std::string& name, Level* level) {
    if (level == nullptr) {
        std::cerr << "Error: Attempting to register null level with name: " << name << std::endl;
        return;
    }
    
    // Check if level already exists
    if (levels.find(name) != levels.end()) {
        std::cerr << "Warning: Level '" << name << "' already registered. Replacing." << std::endl;
        delete levels[name];
    }
    
    levels[name] = level;
    std::cout << "Level '" << name << "' registered successfully." << std::endl;
}

void GameManager::switchToLevel(const std::string& name) {
    // Check if level exists
    auto it = levels.find(name);
    if (it == levels.end()) {
        std::cerr << "Error: Level '" << name << "' not found!" << std::endl;
        return;
    }
    
    // Exit current level and ENSURE it's deactivated
    if (currentLevel != nullptr) {
        std::cout << "Exiting level: " << currentLevelName << std::endl;
        currentLevel->onExit();
        printf("Old level deactivated: %d\n", currentLevel->isActive());
    }
    
    // Switch to new level
    currentLevel = it->second;
    currentLevelName = name;
    
    std::cout << "Switching to level: " << name << std::endl;
    
    // Initialize if not already initialized
    currentLevel->onEnter();
    printf("New level activated: %d\n", currentLevel->isActive());
    
    std::cout << "Level '" << name << "' loaded successfully." << std::endl;
}

void GameManager::unloadLevel(const std::string& name) {
    auto it = levels.find(name);
    if (it == levels.end()) {
        std::cerr << "Warning: Attempting to unload non-existent level: " << name << std::endl;
        return;
    }
    
    // If this is the current level, clear it
    if (currentLevelName == name) {
        if (currentLevel) {
            currentLevel->onExit();
        }
        currentLevel = nullptr;
        currentLevelName = "";
    }
    
    // Clean up and remove
    it->second->cleanup();
    delete it->second;
    levels.erase(it);
    
    std::cout << "Level '" << name << "' unloaded." << std::endl;
}

void GameManager::unloadAllLevels() {
    std::cout << "Unloading all levels..." << std::endl;
    
    // Exit current level first
    if (currentLevel) {
        currentLevel->onExit();
        currentLevel = nullptr;
        currentLevelName = "";
    }
    
    // Clean up all levels
    for (auto& pair : levels) {
        pair.second->cleanup();
        delete pair.second;
    }
    
    levels.clear();
    std::cout << "All levels unloaded." << std::endl;
}

void GameManager::update(float deltaTime) {
    if (currentLevel && currentLevel->isActive()) {
        currentLevel->update(deltaTime);
    }
}

void GameManager::render() {
    static int frameCount = 0;
    // Reset counter on level switch for continuous debugging
    static std::string lastLevelName = "";
    if (currentLevelName != lastLevelName) {
        frameCount = 0;
        lastLevelName = currentLevelName;
    }
    
    if (frameCount < 100) {  // Extended debug
        printf("GM::render() frame %d, currLevel=%s, ptr=%p, active=%d\n", 
               frameCount++, currentLevelName.c_str(), currentLevel, 
               currentLevel ? currentLevel->isActive() : -1);
    }
    if (currentLevel && currentLevel->isActive()) {
        currentLevel->render();
    } else {
        if (frameCount < 100) {
            printf("  -> SKIP render (no active level)\n");
        }
    }
}

void GameManager::handleKeyboard(unsigned char key, bool pressed) {
    if (currentLevel && currentLevel->isActive()) {
        currentLevel->handleKeyboard(key, pressed);
    }
}

void GameManager::handleKeyboardUp(unsigned char key) {
    if (currentLevel && currentLevel->isActive()) {
        currentLevel->handleKeyboardUp(key);
    }
}

void GameManager::handleMouse(int x, int y) {
    if (currentLevel && currentLevel->isActive()) {
        currentLevel->handleMouse(x, y);
    }
}

void GameManager::cleanup() {
    unloadAllLevels();
}
