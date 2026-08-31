#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace Game::Simulation {

enum class StrategicSiteKind : uint8_t {
    Camp,
    Tower,
    Keep,
};

struct StrategicSiteDefinition {
    int32_t objectiveKey = -1;
    StrategicSiteKind kind = StrategicSiteKind::Camp;
    int32_t influenceRegionKey = -1;

    bool operator==(const StrategicSiteDefinition&) const = default;
};

struct SupplyRouteDefinition {
    int32_t routeKey = -1;
    int32_t sourceObjectiveKey = -1;
    int32_t destinationObjectiveKey = -1;

    bool operator==(const SupplyRouteDefinition&) const = default;
};

// Undirected authored border between two influence regions. Region keys are
// stored in ascending order so the same physical border has one canonical
// representation on the server, disk, and wire.
struct InfluenceRegionAdjacencyDefinition {
    int32_t adjacencyKey = -1;
    int32_t lowerRegionKey = -1;
    int32_t upperRegionKey = -1;

    bool operator==(const InfluenceRegionAdjacencyDefinition&) const = default;
};

// Server-authored strategic graph. It deliberately contains no score or
// points-per-tick state: ownership and supply are world state, not a match
// scoreboard.
class StrategicWorldTopology final {
  public:
    static constexpr std::size_t kMaximumSites = 4096;
    static constexpr std::size_t kMaximumSupplyRoutes = 8192;
    static constexpr std::size_t kMaximumInfluenceAdjacencies = 8192;

    bool EnsureSite(const StrategicSiteDefinition& definition);
    bool RemoveSite(int32_t objectiveKey);
    bool EnsureSupplyRoute(const SupplyRouteDefinition& definition);
    bool RemoveSupplyRoute(int32_t routeKey);
    bool EnsureInfluenceAdjacency(
        const InfluenceRegionAdjacencyDefinition& definition);
    bool RemoveInfluenceAdjacency(int32_t adjacencyKey);
    void Reset();

    std::optional<StrategicSiteDefinition> SiteForObjective(int32_t objectiveKey) const;
    std::optional<StrategicSiteDefinition> SiteForInfluenceRegion(
        int32_t influenceRegionKey) const;
    std::optional<SupplyRouteDefinition> SupplyRoute(int32_t routeKey) const;
    std::vector<StrategicSiteDefinition> Sites() const;
    std::vector<SupplyRouteDefinition> SupplyRoutes() const;
    std::vector<InfluenceRegionAdjacencyDefinition> InfluenceAdjacencies() const;
    std::vector<int32_t> SupplySourcesFor(int32_t destinationObjectiveKey) const;
    std::vector<int32_t> AdjacentRegionsFor(int32_t influenceRegionKey) const;

    bool Restore(const std::vector<StrategicSiteDefinition>& sites,
                 const std::vector<SupplyRouteDefinition>& routes,
                 const std::vector<InfluenceRegionAdjacencyDefinition>& adjacencies = {});

  private:
    static bool IsValidSite(const StrategicSiteDefinition& definition);
    bool IsValidRoute(const SupplyRouteDefinition& definition) const;
    bool IsValidInfluenceAdjacency(
        const InfluenceRegionAdjacencyDefinition& definition) const;

    std::vector<StrategicSiteDefinition> mSites;
    std::vector<SupplyRouteDefinition> mSupplyRoutes;
    std::vector<InfluenceRegionAdjacencyDefinition> mInfluenceAdjacencies;
};

} // namespace Game::Simulation
