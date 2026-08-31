#include "StrategicWorldTopology.h"

#include <algorithm>
#include <utility>

namespace Game::Simulation {

bool StrategicWorldTopology::EnsureSite(const StrategicSiteDefinition& definition) {
    if (!IsValidSite(definition)) return false;
    const auto existing = std::find_if(mSites.begin(), mSites.end(), [&](const auto& site) {
        return site.objectiveKey == definition.objectiveKey;
    });
    if (existing == mSites.end()) {
        if (mSites.size() >= kMaximumSites) return false;
        if (SiteForInfluenceRegion(definition.influenceRegionKey)) return false;
        mSites.push_back(definition);
        return true;
    }
    return existing->kind == definition.kind &&
           existing->influenceRegionKey == definition.influenceRegionKey;
}

bool StrategicWorldTopology::RemoveSite(int32_t objectiveKey) {
    const auto site = std::find_if(mSites.begin(), mSites.end(), [&](const auto& candidate) {
        return candidate.objectiveKey == objectiveKey;
    });
    if (site == mSites.end()) return false;
    const int32_t influenceRegionKey = site->influenceRegionKey;
    mSites.erase(site);
    std::erase_if(mSupplyRoutes, [&](const auto& route) {
        return route.sourceObjectiveKey == objectiveKey ||
               route.destinationObjectiveKey == objectiveKey;
    });
    std::erase_if(mInfluenceAdjacencies, [&](const auto& adjacency) {
        return adjacency.lowerRegionKey == influenceRegionKey ||
               adjacency.upperRegionKey == influenceRegionKey;
    });
    return true;
}

bool StrategicWorldTopology::EnsureSupplyRoute(const SupplyRouteDefinition& definition) {
    if (!IsValidRoute(definition)) return false;
    const auto existing = std::find_if(mSupplyRoutes.begin(), mSupplyRoutes.end(), [&](const auto& route) {
        return route.routeKey == definition.routeKey;
    });
    if (existing == mSupplyRoutes.end()) {
        if (mSupplyRoutes.size() >= kMaximumSupplyRoutes) return false;
        mSupplyRoutes.push_back(definition);
        return true;
    }
    return existing->sourceObjectiveKey == definition.sourceObjectiveKey &&
           existing->destinationObjectiveKey == definition.destinationObjectiveKey;
}

bool StrategicWorldTopology::RemoveSupplyRoute(int32_t routeKey) {
    const auto route = std::find_if(mSupplyRoutes.begin(), mSupplyRoutes.end(), [&](const auto& candidate) {
        return candidate.routeKey == routeKey;
    });
    if (route == mSupplyRoutes.end()) return false;
    mSupplyRoutes.erase(route);
    return true;
}

bool StrategicWorldTopology::EnsureInfluenceAdjacency(
    const InfluenceRegionAdjacencyDefinition& definition) {
    if (!IsValidInfluenceAdjacency(definition)) return false;
    const auto existing = std::find_if(
        mInfluenceAdjacencies.begin(), mInfluenceAdjacencies.end(),
        [&](const auto& adjacency) {
            return adjacency.adjacencyKey == definition.adjacencyKey;
        });
    if (existing == mInfluenceAdjacencies.end()) {
        if (mInfluenceAdjacencies.size() >= kMaximumInfluenceAdjacencies) {
            return false;
        }
        mInfluenceAdjacencies.push_back(definition);
        return true;
    }
    return existing->lowerRegionKey == definition.lowerRegionKey &&
           existing->upperRegionKey == definition.upperRegionKey;
}

bool StrategicWorldTopology::RemoveInfluenceAdjacency(int32_t adjacencyKey) {
    const auto adjacency = std::find_if(
        mInfluenceAdjacencies.begin(), mInfluenceAdjacencies.end(),
        [&](const auto& candidate) {
            return candidate.adjacencyKey == adjacencyKey;
        });
    if (adjacency == mInfluenceAdjacencies.end()) return false;
    mInfluenceAdjacencies.erase(adjacency);
    return true;
}

void StrategicWorldTopology::Reset() {
    mSites.clear();
    mSupplyRoutes.clear();
    mInfluenceAdjacencies.clear();
}

std::optional<StrategicSiteDefinition>
StrategicWorldTopology::SiteForInfluenceRegion(int32_t influenceRegionKey) const {
    const auto site = std::find_if(mSites.begin(), mSites.end(),
                                   [&](const auto& candidate) {
        return candidate.influenceRegionKey == influenceRegionKey;
    });
    return site == mSites.end()
               ? std::nullopt
               : std::optional<StrategicSiteDefinition>(*site);
}

std::optional<StrategicSiteDefinition> StrategicWorldTopology::SiteForObjective(int32_t objectiveKey) const {
    const auto site = std::find_if(mSites.begin(), mSites.end(), [&](const auto& candidate) {
        return candidate.objectiveKey == objectiveKey;
    });
    return site == mSites.end() ? std::nullopt : std::optional<StrategicSiteDefinition>(*site);
}

std::optional<SupplyRouteDefinition> StrategicWorldTopology::SupplyRoute(int32_t routeKey) const {
    const auto route = std::find_if(mSupplyRoutes.begin(), mSupplyRoutes.end(), [&](const auto& candidate) {
        return candidate.routeKey == routeKey;
    });
    return route == mSupplyRoutes.end() ? std::nullopt : std::optional<SupplyRouteDefinition>(*route);
}

std::vector<StrategicSiteDefinition> StrategicWorldTopology::Sites() const {
    return mSites;
}

std::vector<SupplyRouteDefinition> StrategicWorldTopology::SupplyRoutes() const {
    return mSupplyRoutes;
}

std::vector<InfluenceRegionAdjacencyDefinition>
StrategicWorldTopology::InfluenceAdjacencies() const {
    return mInfluenceAdjacencies;
}

std::vector<int32_t> StrategicWorldTopology::SupplySourcesFor(int32_t destinationObjectiveKey) const {
    std::vector<int32_t> sources;
    for (const auto& route : mSupplyRoutes) {
        if (route.destinationObjectiveKey == destinationObjectiveKey) {
            sources.push_back(route.sourceObjectiveKey);
        }
    }
    return sources;
}

std::vector<int32_t> StrategicWorldTopology::AdjacentRegionsFor(
    int32_t influenceRegionKey) const {
    std::vector<int32_t> adjacentRegions;
    for (const auto& adjacency : mInfluenceAdjacencies) {
        if (adjacency.lowerRegionKey == influenceRegionKey) {
            adjacentRegions.push_back(adjacency.upperRegionKey);
        } else if (adjacency.upperRegionKey == influenceRegionKey) {
            adjacentRegions.push_back(adjacency.lowerRegionKey);
        }
    }
    return adjacentRegions;
}

bool StrategicWorldTopology::Restore(const std::vector<StrategicSiteDefinition>& sites,
                                     const std::vector<SupplyRouteDefinition>& routes,
                                     const std::vector<InfluenceRegionAdjacencyDefinition>& adjacencies) {
    StrategicWorldTopology restored;
    for (const auto& site : sites) {
        if (!restored.EnsureSite(site)) return false;
    }
    for (const auto& route : routes) {
        if (!restored.EnsureSupplyRoute(route)) return false;
    }
    for (const auto& adjacency : adjacencies) {
        if (!restored.EnsureInfluenceAdjacency(adjacency)) return false;
    }
    *this = std::move(restored);
    return true;
}

bool StrategicWorldTopology::IsValidInfluenceAdjacency(
    const InfluenceRegionAdjacencyDefinition& definition) const {
    if (definition.adjacencyKey < 0 || definition.lowerRegionKey < 0 ||
        definition.lowerRegionKey >= definition.upperRegionKey ||
        !SiteForInfluenceRegion(definition.lowerRegionKey) ||
        !SiteForInfluenceRegion(definition.upperRegionKey)) {
        return false;
    }
    return std::none_of(
        mInfluenceAdjacencies.begin(), mInfluenceAdjacencies.end(),
        [&](const auto& adjacency) {
            return adjacency.adjacencyKey != definition.adjacencyKey &&
                   adjacency.lowerRegionKey == definition.lowerRegionKey &&
                   adjacency.upperRegionKey == definition.upperRegionKey;
        });
}

bool StrategicWorldTopology::IsValidSite(const StrategicSiteDefinition& definition) {
    return definition.objectiveKey >= 0 && definition.influenceRegionKey >= 0 &&
           definition.kind <= StrategicSiteKind::Keep;
}

bool StrategicWorldTopology::IsValidRoute(const SupplyRouteDefinition& definition) const {
    if (definition.routeKey < 0 || definition.sourceObjectiveKey < 0 ||
        definition.destinationObjectiveKey < 0 ||
        definition.sourceObjectiveKey == definition.destinationObjectiveKey) {
        return false;
    }
    const auto source = SiteForObjective(definition.sourceObjectiveKey);
    const auto destination = SiteForObjective(definition.destinationObjectiveKey);
    const bool duplicateEdge = std::any_of(mSupplyRoutes.begin(), mSupplyRoutes.end(),
                                           [&](const auto& route) {
        return route.sourceObjectiveKey == definition.sourceObjectiveKey &&
               route.destinationObjectiveKey == definition.destinationObjectiveKey &&
               route.routeKey != definition.routeKey;
    });
    return source && destination && !duplicateEdge &&
           source->kind == StrategicSiteKind::Camp &&
           (destination->kind == StrategicSiteKind::Tower ||
            destination->kind == StrategicSiteKind::Keep);
}

} // namespace Game::Simulation
