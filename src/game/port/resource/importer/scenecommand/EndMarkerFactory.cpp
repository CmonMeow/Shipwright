#include "port/resource/importer/scenecommand/EndMarkerFactory.h"
#include "port/resource/type/scenecommand/EndMarker.h"
#include <tinyxml2.h>

namespace SOH {
std::shared_ptr<Engine::IResource> EndMarkerFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                std::shared_ptr<Engine::BinaryReader> reader) {
    auto endMarker = std::make_shared<EndMarker>(initData);

    ReadCommandId(endMarker, reader);

    

    return endMarker;
}

std::shared_ptr<Engine::IResource> EndMarkerFactoryXML::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                   tinyxml2::XMLElement*) {
    auto endMarker = std::make_shared<EndMarker>(initData);

    endMarker->cmdId = SceneCommandID::EndMarker;

    return endMarker;
}
} // namespace SOH
