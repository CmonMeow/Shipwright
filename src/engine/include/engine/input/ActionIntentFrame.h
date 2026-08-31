#pragma once

#include <array>
#include <cstddef>

namespace Engine {

// One controller sample's one-shot action edges. BeginSample discards every
// unconsumed edge from the previous sample, so native state eligibility can
// never turn an old key press into a later action.
template <size_t ActionCount>
class ActionIntentFrame {
  public:
    void BeginSample() {
        mPending.fill(false);
        mRequested.fill(false);
    }

    void Request(size_t action) {
        if (action < ActionCount) {
            mPending[action] = true;
            mRequested[action] = true;
        }
    }

    bool Pending(size_t action) const {
        return action < ActionCount && mPending[action];
    }

    // Non-destructive observation for another system sampling the same input
    // frame. Native gameplay may consume Pending without stealing the edge
    // from command generation; both views expire at BeginSample.
    bool Requested(size_t action) const {
        return action < ActionCount && mRequested[action];
    }

    bool Consume(size_t action) {
        if (!Pending(action)) {
            return false;
        }
        mPending[action] = false;
        return true;
    }

    void Cancel(size_t action) {
        if (action < ActionCount) {
            mPending[action] = false;
            mRequested[action] = false;
        }
    }

    void Clear() {
        mPending.fill(false);
        mRequested.fill(false);
    }

  private:
    std::array<bool, ActionCount> mPending{};
    std::array<bool, ActionCount> mRequested{};
};

} // namespace Engine
