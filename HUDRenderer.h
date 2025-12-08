#pragma once

#include <string>
#include <glut.h>

struct HUDCounter {
    std::string label;
    int value = 0;
    int total = 0;
    bool showTotal = false;
    bool enabled = false;
};

struct HUDParams {
    int screenWidth = 0;
    int screenHeight = 0;
    float gameTimer = 0.0f;
    int score = 0;

    HUDCounter primary;   // e.g., Fuel or Rings
    HUDCounter secondary; // e.g., Toolkits (optional)

    bool showWarning = false;
    float warningPhase = 0.0f;    // use a sine phase for pulsing (e.g., arrowBobOffset * 5)
    std::string warningText;
};

class HUDRenderer {
public:
    static void render(const HUDParams& params);

private:
    static void drawText(float x, float y, const std::string& text, void* font = GLUT_BITMAP_HELVETICA_18);
    static void drawCounterBlock(float x0, float y0, float x1, float y1, const HUDCounter& counter);
};
