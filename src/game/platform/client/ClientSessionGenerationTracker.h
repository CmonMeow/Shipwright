#pragma once

#include <cstdint>
#include <optional>

namespace Game::Client {

enum class ClientSessionGenerationUpdate : uint8_t {
    Invalid,
    Established,
    Unchanged,
    Replaced,
};

// Defines the one client-state lifetime boundary driven by transport session
// replacement. Presentation code must reset its session-owned state whenever
// Observe returns Established or Replaced.
class ClientSessionGenerationTracker final {
  public:
    ClientSessionGenerationUpdate Observe(uint64_t generation);
    void Reset();

    std::optional<uint64_t> Current() const { return mCurrent; }

    static bool RequiresStateReset(ClientSessionGenerationUpdate update);

  private:
    std::optional<uint64_t> mCurrent;
};

} // namespace Game::Client
