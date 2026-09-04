#include "platform/client/GameResourceRegistry.h"

#include <memory>
#include <unordered_set>

#include <engine/resource/ResourceLoader.h>
#include <engine/resource/factory/BlobFactory.h>
#include <engine/resource/type/Blob.h>
#include <rendering/resource/ResourceType.h>
#include <rendering/resource/factory/DisplayListFactory.h>
#include <rendering/resource/factory/MatrixFactory.h>
#include <rendering/resource/factory/TextureFactory.h>
#include <rendering/resource/factory/VertexFactory.h>
#include <rendering/resource/type/DisplayList.h>
#include <rendering/resource/type/Matrix.h>
#include <rendering/resource/type/Texture.h>
#include <rendering/resource/type/Vertex.h>
#include <resources/GameVersions.h>

#include "resources/importer/AnimationFactory.h"
#include "resources/importer/ArrayFactory.h"
#include "resources/importer/AudioSampleFactory.h"
#include "resources/importer/AudioSequenceFactory.h"
#include "resources/importer/AudioSoundFontFactory.h"
#include "resources/importer/CollisionHeaderFactory.h"
#include "resources/importer/PlayerAnimationFactory.h"
#include "resources/importer/SceneFactory.h"
#include "resources/importer/SkeletonFactory.h"
#include "resources/importer/SkeletonLimbFactory.h"
#include "resources/importer/TextFactory.h"
#include "resources/type/Animation.h"
#include "resources/type/Array.h"
#include "resources/type/AudioSample.h"
#include "resources/type/AudioSequence.h"
#include "resources/type/AudioSoundFont.h"
#include "resources/type/CollisionHeader.h"
#include "resources/type/PlayerAnimation.h"
#include "resources/type/Scene.h"
#include "resources/type/Skeleton.h"
#include "resources/type/SkeletonLimb.h"
#include "resources/type/GameResourceType.h"
#include "resources/type/Text.h"

namespace Game::Client {

void RegisterGameResourceFactories(Engine::ResourceLoader& loader) {
    loader.RegisterResourceFactory(std::make_shared<Engine::Rendering::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY,
                                   "Texture", static_cast<uint32_t>(Engine::Rendering::ResourceType::Texture), 0);
    loader.RegisterResourceFactory(std::make_shared<Engine::Rendering::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY,
                                   "Texture", static_cast<uint32_t>(Engine::Rendering::ResourceType::Texture), 1);
    loader.RegisterResourceFactory(std::make_shared<Engine::Rendering::ResourceFactoryBinaryVertexV0>(), RESOURCE_FORMAT_BINARY,
                                   "Vertex", static_cast<uint32_t>(Engine::Rendering::ResourceType::Vertex), 0);
    loader.RegisterResourceFactory(std::make_shared<Engine::Rendering::ResourceFactoryBinaryDisplayListV0>(),
                                   RESOURCE_FORMAT_BINARY, "DisplayList",
                                   static_cast<uint32_t>(Engine::Rendering::ResourceType::DisplayList), 0);
    loader.RegisterResourceFactory(std::make_shared<Engine::Rendering::ResourceFactoryBinaryMatrixV0>(), RESOURCE_FORMAT_BINARY,
                                   "Matrix", static_cast<uint32_t>(Engine::Rendering::ResourceType::Matrix), 0);
    loader.RegisterResourceFactory(std::make_shared<Engine::ResourceFactoryBinaryBlobV0>(), RESOURCE_FORMAT_BINARY,
                                   "Blob", static_cast<uint32_t>(Engine::ResourceType::Blob), 0);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinaryArrayV0>(), RESOURCE_FORMAT_BINARY,
                                   "Array", static_cast<uint32_t>(Game::Resources::ResourceType::Array), 0);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinaryAnimationV0>(), RESOURCE_FORMAT_BINARY,
                                   "Animation", static_cast<uint32_t>(Game::Resources::ResourceType::Animation), 0);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinaryPlayerAnimationV0>(),
                                   RESOURCE_FORMAT_BINARY, "PlayerAnimation",
                                   static_cast<uint32_t>(Game::Resources::ResourceType::PlayerAnimation), 0);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinarySceneV0>(), RESOURCE_FORMAT_BINARY,
                                   "Room", static_cast<uint32_t>(Game::Resources::ResourceType::Room), 0);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinaryCollisionHeaderV0>(),
                                   RESOURCE_FORMAT_BINARY, "CollisionHeader",
                                   static_cast<uint32_t>(Game::Resources::ResourceType::CollisionHeader), 0);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinarySkeletonV0>(), RESOURCE_FORMAT_BINARY,
                                   "Skeleton", static_cast<uint32_t>(Game::Resources::ResourceType::Skeleton), 0);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinarySkeletonLimbV0>(),
                                   RESOURCE_FORMAT_BINARY, "SkeletonLimb",
                                   static_cast<uint32_t>(Game::Resources::ResourceType::SkeletonLimb), 0);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinaryTextV0>(), RESOURCE_FORMAT_BINARY,
                                   "Text", static_cast<uint32_t>(Game::Resources::ResourceType::Text), 0);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinaryAudioSampleV2>(), RESOURCE_FORMAT_BINARY,
                                   "AudioSample", static_cast<uint32_t>(Game::Resources::ResourceType::AudioSample), 2);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinaryAudioSoundFontV2>(),
                                   RESOURCE_FORMAT_BINARY, "AudioSoundFont",
                                   static_cast<uint32_t>(Game::Resources::ResourceType::AudioSoundFont), 2);
    loader.RegisterResourceFactory(std::make_shared<Game::Resources::ResourceFactoryBinaryAudioSequenceV2>(),
                                   RESOURCE_FORMAT_BINARY, "AudioSequence",
                                   static_cast<uint32_t>(Game::Resources::ResourceType::AudioSequence), 2);
}

bool SupportsGameVersions(const std::vector<uint32_t>& versions) {
    static const std::unordered_set<uint32_t> supportedVersions = {
        OOT_PAL_MQ,     OOT_NTSC_JP_MQ, OOT_NTSC_US_MQ, OOT_PAL_GC_MQ_DBG, OOT_NTSC_US_10,
        OOT_NTSC_US_11, OOT_NTSC_US_12, OOT_PAL_10,     OOT_PAL_11,        OOT_NTSC_JP_GC_CE,
        OOT_NTSC_JP_GC, OOT_NTSC_US_GC, OOT_PAL_GC,     OOT_PAL_GC_DBG1,   OOT_PAL_GC_DBG2,
    };

    for (const uint32_t version : versions) {
        if (!supportedVersions.contains(version)) {
            return false;
        }
    }
    return true;
}

} // namespace Game::Client
