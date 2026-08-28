#include <runtime/log/Log.hpp>
#include "engine/resource/Resource.h"
namespace Engine {
IResource::IResource(std::shared_ptr<ResourceInitData> initData) : mInitData(initData) {
}

IResource::~IResource() {
    WriteLog("Resource Unloaded: {}\n", GetInitData()->Path);
}

bool IResource::IsDirty() {
    return mIsDirty;
}

void IResource::Dirty() {
    mIsDirty = true;
}

std::shared_ptr<ResourceInitData> IResource::GetInitData() {
    return mInitData;
}
} // namespace Engine
