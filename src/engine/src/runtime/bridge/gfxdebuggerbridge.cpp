#include "runtime/bridge/gfxdebuggerbridge.h"
#include "engine/Context.h"
#include "fast/debug/GfxDebugger.h"

void GfxDebuggerRequestDebugging() {
    Engine::Context::GetInstance()->GetGfxDebugger()->RequestDebugging();
}
bool GfxDebuggerIsDebugging() {
    return Engine::Context::GetInstance()->GetGfxDebugger()->IsDebugging();
}
bool GfxDebuggerIsDebuggingRequested() {
    return Engine::Context::GetInstance()->GetGfxDebugger()->IsDebuggingRequested();
}
void GfxDebuggerDebugDisplayList(void* cmds) {
    Engine::Context::GetInstance()->GetGfxDebugger()->DebugDisplayList((Fast::F3DGfx*)cmds);
}
