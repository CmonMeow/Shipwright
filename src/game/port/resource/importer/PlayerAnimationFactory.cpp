#include "port/resource/importer/PlayerAnimationFactory.h"
#include "port/resource/type/PlayerAnimation.h"
namespace SOH {
std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryPlayerAnimationV0::ReadResource(std::shared_ptr<Engine::File> file,
                                                     std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto playerAnimation = std::make_shared<PlayerAnimation>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    uint32_t numEntries = reader->ReadUInt32();
    playerAnimation->limbRotData.reserve(numEntries);

    for (uint32_t i = 0; i < numEntries; i++) {
        playerAnimation->limbRotData.push_back(reader->ReadInt16());
    }

    return playerAnimation;
};
} // namespace SOH
