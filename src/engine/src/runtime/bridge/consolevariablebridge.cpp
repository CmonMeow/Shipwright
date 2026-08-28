#include "runtime/bridge/consolevariablebridge.h"
#include "engine/Context.h"

std::shared_ptr<Engine::CVar> CVarGet(const char* name) {
    return Engine::Context::GetInstance()->GetConsoleVariables()->Get(name);
}

extern "C" {
int32_t CVarGetInteger(const char* name, int32_t defaultValue) {
    return Engine::Context::GetInstance()->GetConsoleVariables()->GetInteger(name, defaultValue);
}

float CVarGetFloat(const char* name, float defaultValue) {
    return Engine::Context::GetInstance()->GetConsoleVariables()->GetFloat(name, defaultValue);
}

const char* CVarGetString(const char* name, const char* defaultValue) {
    return Engine::Context::GetInstance()->GetConsoleVariables()->GetString(name, defaultValue);
}

Color_RGBA8 CVarGetColor(const char* name, Color_RGBA8 defaultValue) {
    return Engine::Context::GetInstance()->GetConsoleVariables()->GetColor(name, defaultValue);
}

Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 defaultValue) {
    return Engine::Context::GetInstance()->GetConsoleVariables()->GetColor24(name, defaultValue);
}

void CVarSetInteger(const char* name, int32_t value) {
    Engine::Context::GetInstance()->GetConsoleVariables()->SetInteger(name, value);
}

void CVarSetFloat(const char* name, float value) {
    Engine::Context::GetInstance()->GetConsoleVariables()->SetFloat(name, value);
}

void CVarSetString(const char* name, const char* value) {
    Engine::Context::GetInstance()->GetConsoleVariables()->SetString(name, value);
}

void CVarSetColor(const char* name, Color_RGBA8 value) {
    Engine::Context::GetInstance()->GetConsoleVariables()->SetColor(name, value);
}

void CVarSetColor24(const char* name, Color_RGB8 value) {
    Engine::Context::GetInstance()->GetConsoleVariables()->SetColor24(name, value);
}

void CVarRegisterInteger(const char* name, int32_t defaultValue) {
    Engine::Context::GetInstance()->GetConsoleVariables()->RegisterInteger(name, defaultValue);
}

void CVarRegisterFloat(const char* name, float defaultValue) {
    Engine::Context::GetInstance()->GetConsoleVariables()->RegisterFloat(name, defaultValue);
}

void CVarRegisterString(const char* name, const char* defaultValue) {
    Engine::Context::GetInstance()->GetConsoleVariables()->RegisterString(name, defaultValue);
}

void CVarRegisterColor(const char* name, Color_RGBA8 defaultValue) {
    Engine::Context::GetInstance()->GetConsoleVariables()->RegisterColor(name, defaultValue);
}

void CVarRegisterColor24(const char* name, Color_RGB8 defaultValue) {
    Engine::Context::GetInstance()->GetConsoleVariables()->RegisterColor24(name, defaultValue);
}

void CVarClear(const char* name) {
    Engine::Context::GetInstance()->GetConsoleVariables()->ClearVariable(name);
}

void CVarClearBlock(const char* name) {
    Engine::Context::GetInstance()->GetConsoleVariables()->ClearBlock(name);
}

void CVarCopy(const char* from, const char* to) {
    Engine::Context::GetInstance()->GetConsoleVariables()->CopyVariable(from, to);
}

void CVarLoad() {
    Engine::Context::GetInstance()->GetConsoleVariables()->Load();
}

void CVarSave() {
    Engine::Context::GetInstance()->GetConsoleVariables()->Save();
}
}
