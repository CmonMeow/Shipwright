#pragma once

#include "FishingSimulation.h"

#include <vector>

namespace Game::Simulation {

// Canonical map data for the retained fishing pond. Runtime packets never
// supply species or weight; those values are derived from this server table.
std::vector<FishDefinition> BuildFishingPondCatalog();

} // namespace Game::Simulation
