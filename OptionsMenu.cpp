#include "OptionsMenu.h"
#include "GameManager.h"
#include "PlaneSelectionLevel.h"
#include <glut.h>
#include <stdio.h>
#include <cmath>
#include <cstring> // For strlen

OptionsMenu::OptionsMenu()
    : Level(), currentSection(LEVEL_SELECT), selectedLevelIndex(0), selectedPlaneIndex(0),
      pulseTimer(0.0f), screenWidth(1280), screenHeight(720) {
}

OptionsMenu::~OptionsMenu() {
    cleanup();
}

void OptionsMenu::init() {
    printf("Initializing Options Menu...\n");
    // Match PlaneSelectionLevel clear color
    glClearColor(0.08f, 0.12f, 0.22f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Initialize selection indices
    selectedLevelIndex = 0;
    selectedPlaneIndex = PlaneSelectionLevel::getSelectedPlane();
    changePlaneHovered = false;
    currentSection = LEVEL_SELECT;
}

void OptionsMenu::update(float deltaTime) {
    pulseTimer += deltaTime * 2.0f;
}

void OptionsMenu::render() {
    if (!active) return;
    
    // Sync viewport
    int winW = glutGet(GLUT_WINDOW_WIDTH);
    int winH = glutGet(GLUT_WINDOW_HEIGHT);
    if (winW > 0 && winH > 0) {
        screenWidth = winW;
        screenHeight = winH;
    }
    glViewport(0, 0, screenWidth, screenHeight);
    
    // Clear background
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 2D UI setup
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, screenWidth, 0, screenHeight);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Draw background gradient panels (Matching PlaneSelectionLevel)
    glBegin(GL_QUADS);
    glColor4f(0.1f, 0.15f, 0.25f, 1.0f);
    glVertex2f(0, 0);
    glVertex2f(screenWidth, 0);
    glColor4f(0.05f, 0.08f, 0.15f, 1.0f);
    glVertex2f(screenWidth, screenHeight);
    glVertex2f(0, screenHeight);
    glEnd();
    
    // Draw title
    drawTitle();
    
    // Draw sections based on current view
    if (currentSection == LEVEL_SELECT) {
        drawLevelSelect();
    } else if (currentSection == PLANE_SELECT) {
        drawPlaneSelect();
    }
    
    // Access instructions footer (Matching PlaneSelectionLevel)
    // Instructions bar at bottom
    glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(screenWidth, 0);
    glVertex2f(screenWidth, 60);
    glVertex2f(0, 60);
    glEnd();

    glColor4f(0.9f, 0.9f, 0.9f, 1.0f);
    glRasterPos2f(screenWidth * 0.5f - 180, 25);
    const char* hint = "[LEFT/RIGHT] Navigate   |   [ENTER] Select   |   [ESC] Back";
    for (const char* c = hint; *c != '\0'; c++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    }
}

void OptionsMenu::drawTitle() {
    // Title with shadow
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glRasterPos2f(screenWidth * 0.5f - 108, screenHeight - 68);
    const char* title = "OPTIONS MENU";
    for (const char* c = title; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(screenWidth * 0.5f - 110, screenHeight - 70);
    for (const char* c = title; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *c);
    
    // Subtitle
    glColor4f(0.7f, 0.8f, 0.9f, 0.9f);
    glRasterPos2f(screenWidth * 0.5f - 60, screenHeight - 100);
    const char* subtitle = "Configure your flight";
    for (const char* c = subtitle; *c; ++c) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
}

void OptionsMenu::drawLevelSelect() {
    float startY = screenHeight * 0.5f;
    float centerX = screenWidth / 2.0f;
    
    float buttonWidth = 240.0f;
    float buttonHeight = 160.0f;
    float gap = 40.0f;
    
    // Level 1 Card
    bool level1Selected = (selectedLevelIndex == 0);
    float level1X = centerX - buttonWidth - gap / 2.0f;
    float levelY = startY - buttonHeight / 2.0f;
    
    drawButton("LEVEL 1\nOCEAN", level1X, levelY, buttonWidth, buttonHeight, level1Selected, false);
    
    // Level 2 Card
    bool level2Selected = (selectedLevelIndex == 1);
    float level2X = centerX + gap / 2.0f;
    drawButton("LEVEL 2\nCITY", level2X, levelY, buttonWidth, buttonHeight, level2Selected, false);
    
    // Secondary Buttons (Change Plane, Back) - Smaller styling
    float lowerButtonY = levelY - 80.0f;
    float smallBtnW = 200.0f;
    float smallBtnH = 40.0f;
    
    // Change Plane - Just a text indicator or button? Let's make it a button but clearly secondary
    // We don't really have a "selected" state for these in the current logic unless we add indices 2 and 3?
    // The original logic only had 0 and 1 for levels.
    // Let's implement them as clickable areas but not keyboard navigable for now to keep logic simple, OR rely on mouse.
    
    drawButton("CHANGE PLANE", centerX - smallBtnW / 2.0f, lowerButtonY, smallBtnW, smallBtnH, changePlaneHovered, false);
}

void OptionsMenu::drawPlaneSelect() {
    float startY = screenHeight * 0.5f;
    float centerX = screenWidth / 2.0f;
    
    float buttonWidth = 160.0f;
    float buttonHeight = 220.0f; // Tall cards like plane selection
    float gap = 30.0f;
    float totalWidth = 3.0f * buttonWidth + 2.0f * gap;
    float startX = centerX - totalWidth / 2.0f;
    float cardY = startY - buttonHeight / 2.0f;
    
    const char* planeNames[3] = {"ZERO", "INTERCEPTOR", "STEALTH"};
    
    for(int i=0; i<3; ++i) {
        float x = startX + i * (buttonWidth + gap);
        bool selected = (selectedPlaneIndex == i);
        drawButton(planeNames[i], x, cardY, buttonWidth, buttonHeight, selected, false);
    }
    
    // Apply Button
    float applyY = cardY - 60.0f;
    drawButton("APPLY SELECTION", centerX - 100.0f, applyY, 200.0f, 40.0f, false, false);
}

// Redesigned to look like 'Cards' from PlaneSelectionLevel
void OptionsMenu::drawButton(const char* text, float x, float y, float w, float h, bool highlighted, bool pressed) {
    float scale = highlighted ? 1.05f : 1.0f;
    float pulse = highlighted ? (0.15f * sin(pulseTimer * 3.0f)) : 0.0f;
    
    float actualW = w * scale;
    float actualH = h * scale;
    float offX = (actualW - w) * 0.5f;
    float offY = (actualH - h) * 0.5f;
    float dX = x - offX;
    float dY = y - offY;
    
    // Shadow
    glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
    glBegin(GL_QUADS);
    glVertex2f(dX + 6, dY - 6);
    glVertex2f(dX + actualW + 6, dY - 6);
    glVertex2f(dX + actualW + 6, dY + actualH - 6);
    glVertex2f(dX + 6, dY + actualH - 6);
    glEnd();
    
    // Card Body
    if (highlighted) {
        glColor4f(0.2f + pulse, 0.3f + pulse, 0.5f + pulse, 0.9f);
    } else {
        glColor4f(0.15f, 0.2f, 0.35f, 0.8f);
    }
    
    glBegin(GL_QUADS);
    glVertex2f(dX, dY);
    glVertex2f(dX + actualW, dY);
    glVertex2f(dX + actualW, dY + actualH);
    glVertex2f(dX, dY + actualH);
    glEnd();
    
    // Border
    if (highlighted) {
        glColor4f(1.0f, 0.9f, 0.4f, 1.0f); // Gold highlight
        glLineWidth(3.0f);
    } else {
        glColor4f(0.4f, 0.5f, 0.7f, 0.5f);
        glLineWidth(1.0f);
    }
    
    glBegin(GL_LINE_LOOP);
    glVertex2f(dX, dY);
    glVertex2f(dX + actualW, dY);
    glVertex2f(dX + actualW, dY + actualH);
    glVertex2f(dX, dY + actualH);
    glEnd();
    glLineWidth(1.0f);
    
    // Text
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Simple centering logic
    // We'll just draw the text centered for now.
    // If text has newline, split it.
    char buffer[128];
    strncpy(buffer, text, 127);
    char* line = strtok(buffer, "\n");
    
    float totalTextHeight = 0;
    int lineCount = 0;
    while(line) {
        totalTextHeight += 15; // Approx height
        lineCount++;
        line = strtok(NULL, "\n");
    }
    
    strncpy(buffer, text, 127);
    line = strtok(buffer, "\n");
    float currentY = dY + actualH / 2.0f + (totalTextHeight/2.0f) - 6;
    
    while(line) {
        float textW = strlen(line) * 9.0f; // Approx width for Helvetica 18
         // Use smaller font for secondary lines if desired, strictly mimicking original code used Times Roman 10 which is very small.
         // Let's use Helvetica 18 for Main and 12 for sub?
         // For cleanliness, we use Helvetica 18 for all for now or Times Roman 24 for headers.
         
        void* font = GLUT_BITMAP_HELVETICA_18;
        float textX = dX + (actualW - strlen(line) * 9.0f) * 0.5f; 
        
        glRasterPos2f(textX, currentY);
        for (const char* c = line; *c; ++c) {
             glutBitmapCharacter(font, *c);
        }
        currentY -= 24.0f; // line spacing
        line = strtok(NULL, "\n");
    }
    
    // Selection Indicator Arrow
    if (highlighted) {
        float arrowX = dX + actualW * 0.5f;
        float arrowY = dY + actualH + 15 + sin(pulseTimer * 4.0f) * 5.0f;
        glColor4f(1.0f, 0.9f, 0.3f, 1.0f);
        glBegin(GL_TRIANGLES);
        glVertex2f(arrowX, arrowY - 10);
        glVertex2f(arrowX - 8, arrowY + 5);
        glVertex2f(arrowX + 8, arrowY + 5);
        glEnd();
    }
}

bool OptionsMenu::isMouseOverButton(int mx, int my, float x, float y, float w, float h) {
    return (mx >= x && mx <= x + w && my >= y && my <= y + h);
}

void OptionsMenu::handleKeyboard(unsigned char key, bool pressed) {
    if (!active || !pressed) return;
    
    if (key == 27) {  // ESC
        if (currentSection == PLANE_SELECT) {
             currentSection = LEVEL_SELECT;
        } else {
             GameManager::getInstance().switchToLevel("level1");
        }
        return;
    }
    
    if (key == 13) {  // ENTER
        if (currentSection == LEVEL_SELECT) {
            if (selectedLevelIndex == 0) {
                GameManager::getInstance().switchToLevel("level1");
            } else if (selectedLevelIndex == 1) {
                GameManager::getInstance().switchToLevel("level2");
            }
        } else if (currentSection == PLANE_SELECT) {
            PlaneSelectionLevel::setSelectedPlane(selectedPlaneIndex);
            currentSection = LEVEL_SELECT;
        }
        return;
    }
    
    // Navigation with arrow keys
    if (key == 'a' || key == 'A') {  // Left
        if (currentSection == LEVEL_SELECT) {
            selectedLevelIndex = (selectedLevelIndex - 1 + 2) % 2;
        } else if (currentSection == PLANE_SELECT) {
            selectedPlaneIndex = (selectedPlaneIndex - 1 + 3) % 3;
        }
    }
    
    if (key == 'd' || key == 'D') {  // Right
        if (currentSection == LEVEL_SELECT) {
            selectedLevelIndex = (selectedLevelIndex + 1) % 2;
        } else if (currentSection == PLANE_SELECT) {
            selectedPlaneIndex = (selectedPlaneIndex + 1) % 3;
        }
    }
}

void OptionsMenu::handleKeyboardUp(unsigned char key) {
}

void OptionsMenu::handleMouse(int x, int y) {
    if (!active) return;
    screenWidth = glutGet(GLUT_WINDOW_WIDTH); // Ensure dimensions are current
    screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    
    // Implement hover logic by updating selected indices based on mouse pos
    // This makes the UI feel responsive like the PlaneSelectionLevel usually would
    
    float glY = screenHeight - y;
    float centerX = screenWidth / 2.0f;
    
    if (currentSection == LEVEL_SELECT) {
        float startY = screenHeight * 0.5f;
        float buttonWidth = 240.0f;
        float buttonHeight = 160.0f;
        float gap = 40.0f;
        
        float level1X = centerX - buttonWidth - gap / 2.0f;
        float levelY = startY - buttonHeight / 2.0f;
        
        if (isMouseOverButton(x, glY, level1X, levelY, buttonWidth, buttonHeight)) {
            selectedLevelIndex = 0;
        }
        
        float level2X = centerX + gap / 2.0f;
        if (isMouseOverButton(x, glY, level2X, levelY, buttonWidth, buttonHeight)) {
            selectedLevelIndex = 1;
        }
        
        // Change Plane Hover Check
        float lowerButtonY = levelY - 80.0f;
        changePlaneHovered = isMouseOverButton(x, glY, centerX - 100.0f, lowerButtonY, 200.0f, 40.0f);
        
    } else if (currentSection == PLANE_SELECT) {
        float startY = screenHeight * 0.5f;
        float buttonWidth = 160.0f;
        float buttonHeight = 220.0f;
        float gap = 30.0f;
        float totalWidth = 3.0f * buttonWidth + 2.0f * gap;
        float startX = centerX - totalWidth / 2.0f;
        float cardY = startY - buttonHeight / 2.0f;
        
        for(int i=0; i<3; ++i) {
             float btnX = startX + i * (buttonWidth + gap);
             if (isMouseOverButton(x, glY, btnX, cardY, buttonWidth, buttonHeight)) {
                 selectedPlaneIndex = i;
             }
        }
    }
}

void OptionsMenu::handleMouseClick(int button, int state, int x, int y) {
    if (!active || button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    
    screenWidth = glutGet(GLUT_WINDOW_WIDTH);
    screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    
    float glY = screenHeight - y;
    float centerX = screenWidth / 2.0f;
    
    if (currentSection == LEVEL_SELECT) {
        // ... (Check clicks similar to handleMouse but execute action)
        float startY = screenHeight * 0.5f;
        float buttonWidth = 240.0f;
        float buttonHeight = 160.0f;
        float gap = 40.0f;
        float levelY = startY - buttonHeight / 2.0f;
        
        // Level 1
        float level1X = centerX - buttonWidth - gap / 2.0f;
        if (isMouseOverButton(x, glY, level1X, levelY, buttonWidth, buttonHeight)) {
            selectedLevelIndex = 0;
            GameManager::getInstance().switchToLevel("level1");
            return;
        }
        // Level 2
        float level2X = centerX + gap / 2.0f;
        if (isMouseOverButton(x, glY, level2X, levelY, buttonWidth, buttonHeight)) {
             selectedLevelIndex = 1;
             GameManager::getInstance().switchToLevel("level2");
             return;
        }
        
        // Change Plane
        float lowerButtonY = levelY - 80.0f;
        // Debug Interaction
        // printf("Click at %d,%d (GL Y: %.2f). Target Button: %.2f, %.2f, 200x40\n", x, y, glY, centerX - 100.0f, lowerButtonY);
        
        if (isMouseOverButton(x, glY, centerX - 100.0f, lowerButtonY, 200.0f, 40.0f)) {
            printf("Change Plane Clicked!\n");
            currentSection = PLANE_SELECT;
            return;
        }
        
    } else if (currentSection == PLANE_SELECT) {
        // Plane checks handled by hover mostly, but click to confirm
        float startY = screenHeight * 0.5f;
        float buttonWidth = 160.0f;
        float buttonHeight = 220.0f;
        float gap = 30.0f;
        float totalWidth = 3.0f * buttonWidth + 2.0f * gap;
        float startX = centerX - totalWidth / 2.0f;
        float cardY = startY - buttonHeight / 2.0f;
        
        for(int i=0; i<3; ++i) {
             float btnX = startX + i * (buttonWidth + gap);
             if (isMouseOverButton(x, glY, btnX, cardY, buttonWidth, buttonHeight)) {
                 selectedPlaneIndex = i;
                 // Don't confirm immediately? Or do? Let's select.
             }
        }
        
        // Apply
        float applyY = cardY - 60.0f;
        if (isMouseOverButton(x, glY, centerX - 100.0f, applyY, 200.0f, 40.0f)) {
             PlaneSelectionLevel::setSelectedPlane(selectedPlaneIndex);
             currentSection = LEVEL_SELECT;
             return;
        }
    }
}

void OptionsMenu::cleanup() {
    printf("Cleaning up Options Menu...\n");
}
