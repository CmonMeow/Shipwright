#pragma once

#include <cstdint>

namespace Game::Simulation {

struct EntityId {
    uint32_t index = 0;
    uint32_t generation = 0;

    constexpr bool Valid() const {
        return generation != 0;
    }

    constexpr bool operator==(const EntityId&) const = default;
};

} // namespace Game::Simulation
