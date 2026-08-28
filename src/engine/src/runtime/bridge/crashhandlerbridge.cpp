#include "runtime/bridge/crashhandlerbridge.h"
#include "engine/Context.h"
#include "engine/debug/CrashHandler.h"

void CrashHandlerRegisterCallback(CrashHandlerCallback callback) {
    Engine::Context::GetInstance()->GetCrashHandler()->RegisterCallback(callback);
}
