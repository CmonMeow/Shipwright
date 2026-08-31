#pragma once

#include <cstdint>

namespace Game::Sequence {

// RFC 1982-style ordering for nonzero 32-bit counters. Differences of exactly
// half the sequence space are intentionally unordered; accepting either side
// would make replay decisions depend on argument order.
constexpr bool IsNewer(uint32_t candidate, uint32_t current) noexcept {
    const uint32_t delta = candidate - current;
    return delta != 0 && delta < 0x80000000U;
}

constexpr bool IsAtOrAfter(uint32_t candidate, uint32_t current) noexcept {
    return candidate == current || IsNewer(candidate, current);
}

} // namespace Game::Sequence
