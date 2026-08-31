#include "NativeVoiceController.h"

#include "NetworkRuntime.h"
#include "VoiceChat.h"

#include <engine/Context.h>
#include <engine/config/ConsoleVariable.h>
#include <engine/input/Win32Input.h>
#include <runtime/log/Log.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace SoH::Network {
namespace {

constexpr const char* kVoiceEnabled = "gSettings.MultiplayerVoiceEnabled";
constexpr const char* kVoicePushToTalk =
    "gSettings.MultiplayerVoicePushToTalk";

Engine::ConsoleVariable& Variables() {
    return *Engine::Context::GetInstance()->GetConsoleVariables();
}

void DiscardIncomingVoice(NetworkRuntime& runtime) {
    NetworkVoicePacket packet;
    while (runtime.PollVoice(packet)) {
    }
}

} // namespace

NativeVoiceController::NativeVoiceController() = default;

NativeVoiceController::~NativeVoiceController() {
    Shutdown();
}

void NativeVoiceController::Update(
    NetworkRuntime& runtime, bool textInputActive) {
    const bool enabled = Variables().GetInteger(kVoiceEnabled, 1) != 0;
    if (enabled && !mVoice) {
        mVoice = std::make_unique<cVoiceChat>();
        Error("Voice chat initialized");
    }
    if (!mVoice) {
        // A disabled client must still consume admitted voice packets so its
        // bounded inbox cannot retain stale audio until voice is enabled.
        DiscardIncomingVoice(runtime);
        return;
    }

    const bool pushToTalk =
        Variables().GetInteger(kVoicePushToTalk, 0) != 0;
    auto& input = Engine::GetWin32Input();
    const bool talkDown = input.Pressed(VK_SHIFT) && !textInputActive &&
                          !input.IsGameInputBlocked();
    mVoice->update(runtime, talkDown, enabled, pushToTalk);
}

void NativeVoiceController::Shutdown() {
    mVoice.reset();
}

} // namespace SoH::Network
