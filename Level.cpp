#include "Level.h"

Level::Level() : active(false) {
}

Level::~Level() {
}

void Level::onEnter() {
    active = true;
}

void Level::onExit() {
    active = false;
}
