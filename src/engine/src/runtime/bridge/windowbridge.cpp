#include "runtime/bridge/windowbridge.h"
#include "engine/window/Window.h"
#include "engine/Context.h"

extern "C" {

uint32_t WindowGetWidth() {
    return Engine::Context::GetInstance()->GetWindow()->GetWidth();
}

uint32_t WindowGetHeight() {
    return Engine::Context::GetInstance()->GetWindow()->GetHeight();
}

float WindowGetAspectRatio() {
    return Engine::Context::GetInstance()->GetWindow()->GetCurrentAspectRatio();
}

bool WindowIsRunning() {
    return Engine::Context::GetInstance()->GetWindow()->IsRunning();
}

int32_t WindowGetPosX() {
    return Engine::Context::GetInstance()->GetWindow()->GetPosX();
}

int32_t WindowGetPosY() {
    return Engine::Context::GetInstance()->GetWindow()->GetPosY();
}

bool WindowIsFullscreen() {
    return Engine::Context::GetInstance()->GetWindow()->IsFullscreen();
}
}
