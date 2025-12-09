#include "SoundSystem.h"
#include <stdio.h>
#include <direct.h>
#include <iostream>

// Helper to log MCI errors
void logMCIError(MCIERROR err) {
    if (err != 0) {
        char buffer[256];
        mciGetErrorStringA(err, buffer, 256);
        printf("MCI Error: %s\n", buffer);
    }
}

SoundSystem::SoundSystem() 
    : currentSound(PlaneSound::NONE), previousSound(PlaneSound::NONE), 
      initialized(false), crashed(false), landed(false), basePath("") {
}

SoundSystem::~SoundSystem() {
    stopAll();
}

std::string SoundSystem::getFullPath(const char* filename) {
    if (!basePath.empty()) {
        std::string longPath = basePath + "\\" + filename;
        // Convert to short path to avoid space issues with MCI
        char shortPath[MAX_PATH];
        DWORD result = GetShortPathNameA(longPath.c_str(), shortPath, MAX_PATH);
        if (result == 0) {
            // Failed (file might not exist yet?), return quoted long path
            return std::string("\"") + longPath + "\"";
        }
        return std::string(shortPath);
    }
    return std::string(filename);
}

void SoundSystem::playLoopingSound(const char* filename) {
    mciSendString("close engine", NULL, 0, NULL);

    std::string fullPath = getFullPath(filename);
    // Use mpegvideo for better compatibility, and ensure surrounding quotes (getFullPath handles internal if needed, but safe to double quote in MCI? No, be careful)
    // Actually getFullPath returns either short path (no spaces) OR quoted long path.
    // So we just use it.
    std::string command = "open " + fullPath + " type mpegvideo alias engine";
    
    // Explicit error checking
    MCIERROR err = mciSendString(command.c_str(), NULL, 0, NULL);
    if (err) {
        printf("Failed to open engine sound: %s\n", command.c_str());
        logMCIError(err);
    } else {
        mciSendString("play engine repeat", NULL, 0, NULL);
    }
}

void SoundSystem::init() {
    if (initialized) return;
    
    char cwd[MAX_PATH];
    if (_getcwd(cwd, MAX_PATH)) {
        std::string cwdStr(cwd);
        std::string testPath1 = cwdStr + "\\sound";
        
        FILE* f = nullptr;
        std::string testFile = testPath1 + "\\coin.wav";
        
        if (fopen_s(&f, testFile.c_str(), "rb") == 0 && f) {
            fclose(f);
            basePath = testPath1;
        } else {
            // Try standard location if not found in cwd
            basePath = "C:\\Users\\Victus\\Documents\\v2\\Sky-bound\\sound";
        }
    } else {
        basePath = "C:\\Users\\Victus\\Documents\\v2\\Sky-bound\\sound";
    }
    
    printf("SoundSystem initialized. BasePath: %s\n", basePath.c_str());
    
    initialized = true;
    reset();
}

bool SoundSystem::update(bool isGrounded, float speed) {
    if (!initialized || crashed || landed) return false;
    
    // Check status of engine sound to restart if stopped unexpectedly
    char statusBuf[64];
    mciSendString("status engine mode", statusBuf, sizeof(statusBuf), NULL);
    if (currentSound != PlaneSound::NONE && strcmp(statusBuf, "playing") != 0) {
        // Force restart if it should be playing but isn't
        mciSendString("play engine repeat", NULL, 0, NULL);
    }
    
    previousSound = currentSound;
    PlaneSound targetSound;
    
    if (isGrounded && speed < 30.0f) {
        targetSound = PlaneSound::IDLE;
    } else {
        targetSound = PlaneSound::FLYING;
    }
    
    bool touchdown = (previousSound == PlaneSound::FLYING && targetSound == PlaneSound::IDLE);
    
    if (targetSound != currentSound) {
        currentSound = targetSound;
        
        if (targetSound == PlaneSound::IDLE) {
            playLoopingSound("propeller-plane-idle-01.wav");
        } else if (targetSound == PlaneSound::FLYING) {
            playLoopingSound("propeller-plane-flying-steady-01.wav");
        }
    }
    
    return touchdown;
}

void SoundSystem::playCrashSound() {
    crashed = true;
    currentSound = PlaneSound::NONE;
    
    mciSendString("close engine", NULL, 0, NULL);
    
    std::string fullPath = getFullPath("plane-crash.wav");
    std::string command = "open " + fullPath + " type mpegvideo alias crash";
    mciSendString(command.c_str(), NULL, 0, NULL);
    mciSendString("play crash", NULL, 0, NULL);
}

void SoundSystem::playLandingSound() {
    landed = true;
    currentSound = PlaneSound::NONE;
    
    mciSendString("close engine", NULL, 0, NULL);
    
    std::string fullPath = getFullPath("planlanding.wav");
    std::string command = "open " + fullPath + " type mpegvideo alias landing";
    mciSendString(command.c_str(), NULL, 0, NULL);
    mciSendString("play landing", NULL, 0, NULL);
}

void SoundSystem::playCoinSound() {
    std::string fullPath = getFullPath("coin.wav");
    mciSendString("close coin", NULL, 0, NULL); 
    std::string command = "open " + fullPath + " type mpegvideo alias coin";
    mciSendString(command.c_str(), NULL, 0, NULL);
    mciSendString("play coin", NULL, 0, NULL);
}

void SoundSystem::playTouchdownSound() {
    std::string fullPath = getFullPath("planlanding.wav");
    mciSendString("close touchdown", NULL, 0, NULL);
    std::string command = "open " + fullPath + " type mpegvideo alias touchdown";
    mciSendString(command.c_str(), NULL, 0, NULL);
    mciSendString("play touchdown", NULL, 0, NULL);
}

void SoundSystem::stopEngineSound() {
    mciSendString("close engine", NULL, 0, NULL);
    currentSound = PlaneSound::NONE;
}

void SoundSystem::stopAll() {
    mciSendString("close all", NULL, 0, NULL);
    currentSound = PlaneSound::NONE;
}

void SoundSystem::reset() {
    crashed = false;
    landed = false;
    currentSound = PlaneSound::NONE;
    previousSound = PlaneSound::NONE;
    mciSendString("close all", NULL, 0, NULL);
}
