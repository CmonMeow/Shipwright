#pragma once

#include <memory>

class cVoiceChat;

namespace Engine {
class ConsoleVariable;
}

struct Input;

namespace Game::Multiplayer {

class NetworkRuntime;

// Owns native capture/playback lifetime and maps voice settings plus the
// push-to-talk input into the network voice stream. Chat only supplies whether
// text entry currently owns keyboard input.
class NativeVoiceController final {
  public:
    NativeVoiceController(Engine::ConsoleVariable& variables, Input& input);
    ~NativeVoiceController();

    NativeVoiceController(const NativeVoiceController&) = delete;
    NativeVoiceController& operator=(const NativeVoiceController&) = delete;

    void Update(NetworkRuntime& runtime, bool textInputActive);
    void Shutdown();

  private:
    Engine::ConsoleVariable& mVariables;
    Input& mInput;
    std::unique_ptr<cVoiceChat> mVoice;
};

} // namespace Game::Multiplayer
