#pragma once

#include <algorithm>
#include <cmath>

namespace Game::Replication {

struct InterestRadii {
    float enter = 0.0f;
    float leave = 0.0f;
};

// A visible entity gets a modest outer leave radius so tiny position changes
// around the enter boundary do not generate reliable leave/enter churn. The
// margin is deterministic and bounded so it does not grow without limit on
// large MMO interest areas.
inline InterestRadii MakeInterestRadii(float enterRadius) {
    if (!std::isfinite(enterRadius) || enterRadius <= 0.0f) return {};
    const float margin = std::clamp(enterRadius * 0.1f, 50.0f, 500.0f);
    return { enterRadius, enterRadius + margin };
}

inline bool WithinInterest(float distanceSquared, bool alreadyVisible,
                           const InterestRadii& radii) {
    const float radius = alreadyVisible ? radii.leave : radii.enter;
    return std::isfinite(distanceSquared) &&
           distanceSquared <= radius * radius;
}

} // namespace Game::Replication
