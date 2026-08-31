#pragma once

#include <cstdint>
#include <optional>

namespace Game::Client {

enum class LocalFishIntentAction : uint8_t {
    Hook,
    Release,
};

struct LocalFishIntentRequest {
    LocalFishIntentAction action = LocalFishIntentAction::Hook;
};

struct LocalFishIntent {
    uint32_t sequence = 0;
    LocalFishIntentRequest request{};
};

// Owns the semantic local hook lifetime and its reliable command sequence.
// Native actor updates may present that lifetime, but only BeginHook/EndHook
// are allowed to create gameplay actions.
class LocalFishIntentStream final {
  public:
    std::optional<LocalFishIntent> BeginHook();
    std::optional<LocalFishIntent> EndHook();
    bool Resolve(uint32_t sequence, bool submitted);
    void Reset();

    bool HookActive() const { return mHookActive; }

  private:
    uint32_t TakeSequence();

    uint32_t mNextSequence = 1;
    bool mHookActive = false;
    struct PendingIntent {
        uint32_t sequence = 0;
        bool previousHookActive = false;
    };
    std::optional<PendingIntent> mPending;
};

} // namespace Game::Client
