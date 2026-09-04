#pragma once

#include <cstdint>
#include <vector>

namespace Engine {
class ResourceLoader;
}

namespace Game::Client {

void RegisterGameResourceFactories(Engine::ResourceLoader& loader);
bool SupportsGameVersions(const std::vector<uint32_t>& versions);

} // namespace Game::Client
