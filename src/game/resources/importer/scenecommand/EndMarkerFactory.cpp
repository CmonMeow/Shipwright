#include "resources/importer/scenecommand/EndMarkerFactory.h"
#include "resources/type/scenecommand/EndMarker.h"

namespace Game::Resources {
std::shared_ptr<Engine::IResource> EndMarkerFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                std::shared_ptr<Engine::BinaryReader> reader) {
    auto endMarker = std::make_shared<EndMarker>(initData);

    ReadCommandId(endMarker, reader);

    

    return endMarker;
}

} // namespace Game::Resources
