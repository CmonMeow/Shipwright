#pragma once

#include "maff.h"

class Application;

class SettingsWindow;

struct Global {
    vec2i size = { 640, 480 };
    bool quit = false;
    bool invertCameraY = false;
    bool suppressWorldMouse = false;
    bool mouseCaptured = false;
    SettingsWindow* settings = nullptr;
    Application* client = nullptr;
};

extern Global App;
