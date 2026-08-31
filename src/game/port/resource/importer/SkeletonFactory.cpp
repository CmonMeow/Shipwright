#include <runtime/log/Log.hpp>
#include "port/resource/importer/SkeletonFactory.h"
#include "port/resource/type/Skeleton.h"
#include <runtime/runtime.h>

namespace SOH {
std::shared_ptr<Engine::IResource>
ResourceFactoryBinarySkeletonV0::ReadResource(std::shared_ptr<Engine::File> file,
                                              std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto skeleton = std::make_shared<Skeleton>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    skeleton->type = (SkeletonType)reader->ReadInt8();
    skeleton->limbType = (LimbType)reader->ReadInt8();
    skeleton->limbCount = reader->ReadUInt32();
    skeleton->dListCount = reader->ReadUInt32();
    skeleton->limbTableType = (LimbType)reader->ReadInt8();
    skeleton->limbTableCount = reader->ReadUInt32();

    skeleton->limbTable.reserve(skeleton->limbTableCount);
    for (int32_t i = 0; i < skeleton->limbTableCount; i++) {
        std::string limbPath = reader->ReadString();

        skeleton->limbTable.push_back(limbPath);
    }

    if (skeleton->type == SkeletonType::Curve) {
        skeleton->skeletonData.skelCurveLimbList.limbCount = skeleton->limbCount;
        skeleton->curveLimbArray.reserve(skeleton->skeletonData.skelCurveLimbList.limbCount);
    } else if (skeleton->type == SkeletonType::Flex) {
        skeleton->skeletonData.flexSkeletonHeader.dListCount = skeleton->dListCount;
    }

    if (skeleton->type == SkeletonType::Normal) {
        skeleton->skeletonData.skeletonHeader.limbCount = skeleton->limbCount;
        skeleton->standardLimbArray.reserve(skeleton->skeletonData.skeletonHeader.limbCount);
    } else if (skeleton->type == SkeletonType::Flex) {
        skeleton->skeletonData.flexSkeletonHeader.sh.limbCount = skeleton->limbCount;
        skeleton->standardLimbArray.reserve(skeleton->skeletonData.flexSkeletonHeader.sh.limbCount);
    }

    for (size_t i = 0; i < skeleton->limbTable.size(); i++) {
        std::string limbStr = skeleton->limbTable[i];
        auto limb = Engine::Context::GetInstance()->GetResourceManager()->LoadResourceProcess(limbStr.c_str());
        skeleton->skeletonHeaderSegments.push_back(limb ? limb->GetRawPointer() : nullptr);
    }

    if (skeleton->type == SkeletonType::Normal) {
        skeleton->skeletonData.skeletonHeader.segment = (void**)skeleton->skeletonHeaderSegments.data();
    } else if (skeleton->type == SkeletonType::Flex) {
        skeleton->skeletonData.flexSkeletonHeader.sh.segment = (void**)skeleton->skeletonHeaderSegments.data();
    } else if (skeleton->type == SkeletonType::Curve) {
        skeleton->skeletonData.skelCurveLimbList.limbs = (SkelCurveLimb**)skeleton->skeletonHeaderSegments.data();
    } else {
        WriteLog("unknown skeleton type {}", (uint32_t)skeleton->type);
    }

    skeleton->skeletonData.skeletonHeader.skeletonType = (uint8_t)skeleton->type;

    return skeleton;
}

} // namespace SOH
