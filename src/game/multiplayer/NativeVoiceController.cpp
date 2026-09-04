#include "NativeVoiceController.h"

#include "NetworkRuntime.h"
#include "VoiceChat.h"

#include <engine/config/ConsoleVariable.h>
#include <platform/win32/Input.h>
#include <runtime/log/Log.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Game::Multiplayer {
namespace {

constexpr const char* kVoiceEnabled = "gSettings.MultiplayerVoiceEnabled";
constexpr const char* kVoicePushToTalk =
    "gSettings.MultiplayerVoicePushToTalk";

void DiscardIncomingVoice(NetworkRuntime& runtime) {
    NetworkVoicePacket packet;
    while (runtime.PollVoice(packet)) {
    }
}

} // namespace

NativeVoiceController::NativeVoiceController(Engine::ConsoleVariable& variables, Input& input)
    : mVariables(variables), mInput(input) {
}

NativeVoiceController::~NativeVoiceController() {
    Shutdown();
}

void NativeVoiceController::Update(
    NetworkRuntime& runtime, bool textInputActive) {
    const bool enabled = mVariables.GetInteger(kVoiceEnabled, 1) != 0;
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
        mVariables.GetInteger(kVoicePushToTalk, 0) != 0;
    const bool talkDown = mInput.key[VK_SHIFT] && !textInputActive &&
                          !mInput.IsGameInputBlocked();
    mVoice->update(runtime, talkDown, enabled, pushToTalk);
}

void NativeVoiceController::Shutdown() {
    mVoice.reset();
}

} // namespace Game::Multiplayer
