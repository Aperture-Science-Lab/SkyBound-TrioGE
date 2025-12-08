#include "HUDRenderer.h"
#include <cmath>

void HUDRenderer::drawText(float x, float y, const std::string& text, void* font) {
    glRasterPos2f(x, y);
    for (char c : text) {
        glutBitmapCharacter(font, c);
    }
}

void HUDRenderer::drawCounterBlock(float x0, float y0, float x1, float y1, const HUDCounter& counter) {
    if (!counter.enabled) return;

    // Background
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();

    // Text
    glColor3f(1.0f, 1.0f, 1.0f);
    std::string line;
    if (counter.showTotal) {
        line = counter.label + ": " + std::to_string(counter.value) + " / " + std::to_string(counter.total);
    } else {
        line = counter.label + ": " + std::to_string(counter.value);
    }
    drawText(x0 + 12.0f, y1 - 20.0f, line);
}

void HUDRenderer::render(const HUDParams& params) {
    // Save states
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    // Ortho setup
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, params.screenWidth, 0, params.screenHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);

    // Blending for translucent panels
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    char buffer[64];

    // Top-left block: primary + optional secondary stacked
    float leftX0 = 10.0f;
    float leftX1 = 210.0f;
    float lineHeight = 26.0f;
    int lines = params.primary.enabled + params.secondary.enabled;
    if (lines == 0) lines = 1; // avoid zero height
    float leftY1 = params.screenHeight - 10.0f;
    float leftY0 = leftY1 - (lines * lineHeight + 12.0f);

    if (params.primary.enabled || params.secondary.enabled) {
        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
        glBegin(GL_QUADS);
        glVertex2f(leftX0, leftY1);
        glVertex2f(leftX1, leftY1);
        glVertex2f(leftX1, leftY0);
        glVertex2f(leftX0, leftY0);
        glEnd();

        float textY = leftY1 - 24.0f;
        if (params.primary.enabled) {
            std::string line;
            if (params.primary.showTotal) {
                line = params.primary.label + ": " + std::to_string(params.primary.value) + " / " + std::to_string(params.primary.total);
            } else {
                line = params.primary.label + ": " + std::to_string(params.primary.value);
            }
            glColor3f(1.0f, 1.0f, 1.0f);
            drawText(leftX0 + 12.0f, textY, line);
            textY -= lineHeight;
        }
        if (params.secondary.enabled) {
            std::string line;
            if (params.secondary.showTotal) {
                line = params.secondary.label + ": " + std::to_string(params.secondary.value) + " / " + std::to_string(params.secondary.total);
            } else {
                line = params.secondary.label + ": " + std::to_string(params.secondary.value);
            }
            glColor3f(1.0f, 0.9f, 0.3f);
            drawText(leftX0 + 12.0f, textY, line);
        }
    }

    // Timer box (top center)
    float timerBoxX = (params.screenWidth - 180.0f) * 0.5f;
    float timerBoxY1 = params.screenHeight - 10.0f;
    float timerBoxY0 = timerBoxY1 - 50.0f;
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(timerBoxX, timerBoxY1);
    glVertex2f(timerBoxX + 180.0f, timerBoxY1);
    glVertex2f(timerBoxX + 180.0f, timerBoxY0);
    glVertex2f(timerBoxX, timerBoxY0);
    glEnd();

    // Timer text with color shift
    if (params.gameTimer > 30.0f) {
        glColor3f(1.0f, 1.0f, 1.0f);
    } else if (params.gameTimer > 10.0f) {
        glColor3f(1.0f, 1.0f, 0.0f);
    } else {
        glColor3f(1.0f, 0.3f, 0.3f);
    }
    int minutes = (int)(params.gameTimer) / 60;
    int seconds = (int)(params.gameTimer) % 60;
    snprintf(buffer, sizeof(buffer), "Time: %d:%02d", minutes, seconds);
    drawText(timerBoxX + 24.0f, timerBoxY1 - 32.0f, buffer);

    // Score (top right)
    float scoreX1 = params.screenWidth - 10.0f;
    float scoreX0 = scoreX1 - 180.0f;
    float scoreY1 = params.screenHeight - 10.0f;
    float scoreY0 = scoreY1 - 50.0f;
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    glBegin(GL_QUADS);
    glVertex2f(scoreX0, scoreY1);
    glVertex2f(scoreX1, scoreY1);
    glVertex2f(scoreX1, scoreY0);
    glVertex2f(scoreX0, scoreY0);
    glEnd();

    glColor3f(1.0f, 0.9f, 0.2f);
    snprintf(buffer, sizeof(buffer), "Score: %d", params.score);
    drawText(scoreX0 + 12.0f, scoreY1 - 32.0f, buffer);

    // Altitude warning (optional)
    if (params.showWarning && !params.warningText.empty()) {
        float pulse = 0.7f + 0.3f * sinf(params.warningPhase);
        glColor4f(1.0f, 0.0f, 0.0f, pulse);
        float warnX = (params.screenWidth - 220.0f) * 0.5f;
        drawText(warnX, params.screenHeight - 80.0f, params.warningText);
    }

    // Restore matrices/state
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}
