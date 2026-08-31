#include "FishCatalog.h"

#include <array>

namespace Game::Simulation {
namespace {

constexpr int32_t kFishingPondScene = 0x49;
constexpr int32_t kFishingPondRoom = 3;

struct PondFishEntry {
    int32_t homeX;
    int32_t homeY;
    int32_t homeZ;
    float baseLength;
    bool isLoach;
};

constexpr std::array<PondFishEntry, 17> kPondFish = {{
    { 666, -45, 354, 38.0f, false },   { 681, -45, 240, 36.0f, false },
    { 670, -45, 90, 41.0f, false },    { 615, -45, -450, 35.0f, false },
    { 500, -45, -420, 39.0f, false },  { 420, -45, -550, 44.0f, false },
    { -264, -45, -640, 40.0f, false }, { -470, -45, -540, 34.0f, false },
    { -557, -45, -430, 54.0f, false }, { -260, -60, -330, 47.0f, false },
    { -500, -60, 330, 42.0f, false },  { 428, -40, -283, 33.0f, false },
    { 409, -70, -230, 57.0f, false },  { 450, -67, -300, 63.0f, false },
    { -136, -65, -196, 71.0f, false }, { -561, -35, -547, 45.0f, true },
    { 667, -35, 317, 43.0f, true },
}};

uint32_t Hash(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352D;
    value ^= value >> 15;
    value *= 0x846CA68B;
    return value ^ (value >> 16);
}

float Random01(uint32_t seed) {
    return static_cast<float>(Hash(seed) & 0x00FFFFFF) / 16777216.0f;
}

float CanonicalLength(uint32_t spawnKey, const PondFishEntry& fish) {
    const uint32_t seed = static_cast<uint32_t>(kFishingPondScene) * 0x9E3779B9U ^
                          spawnKey * 0x85EBCA6BU ^
                          static_cast<uint32_t>(fish.homeX) * 0xC2B2AE35U ^
                          static_cast<uint32_t>(fish.homeZ) * 0x27D4EB2FU;
    float length = fish.baseLength + Random01(seed) * 4.99999f;
    if (length >= 65.0f && Random01(seed ^ 0x63D83595U) < 0.05f) {
        length += Random01(seed ^ 0xA511E9B3U) * 7.99999f;
    }
    return length;
}

} // namespace

std::vector<FishDefinition> BuildFishingPondCatalog() {
    std::vector<FishDefinition> result;
    result.reserve(kPondFish.size());
    for (size_t index = 0; index < kPondFish.size(); ++index) {
        const PondFishEntry& fish = kPondFish[index];
        const uint32_t spawnKey = MakeFishSpawnKey(
            kFishingPondScene, kFishingPondRoom, fish.homeX, fish.homeY, fish.homeZ);
        FishDefinition definition{};
        definition.identity = { kFishingPondScene, spawnKey };
        definition.spawnPosition = { static_cast<float>(fish.homeX),
                                     static_cast<float>(fish.homeY),
                                     static_cast<float>(fish.homeZ) };
        definition.species = fish.isLoach ? FishSpecies::HylianLoach
                                          : FishSpecies::HylianBass;
        definition.length = CanonicalLength(spawnKey, fish);
        result.push_back(definition);
    }
    return result;
}

} // namespace Game::Simulation
