#include <runtime/log/Log.hpp>
#include "engine/resource/ResourceManager.h"
#include "engine/resource/File.h"
#include "engine/resource/archive/Archive.h"
#include <algorithm>
#include <thread>
#include "engine/utils/StringHelper.h"
#include "engine/utils/Utils.h"
#include "engine/config/ConsoleVariable.h"

namespace Engine {

ResourceFilter::ResourceFilter(const std::list<std::string>& includeMasks, const std::list<std::string>& excludeMasks,
                               const uintptr_t owner, const std::shared_ptr<Archive> parent)
    : IncludeMasks(includeMasks), ExcludeMasks(excludeMasks), Owner(owner), Parent(parent) {
}

size_t ResourceIdentifier::GetHash() const {
    return mHash;
}

ResourceIdentifier::ResourceIdentifier(const std::string& path, const uintptr_t owner,
                                       const std::shared_ptr<Archive> parent)
    : Path(path), Owner(owner), Parent(parent) {
    mHash = CalculateHash();
}

bool ResourceIdentifier::operator==(const ResourceIdentifier& rhs) const {
    return Owner == rhs.Owner && Path == rhs.Path && Parent == rhs.Parent;
}

size_t ResourceIdentifier::CalculateHash() {
    size_t hash = Math::HashCombine(std::hash<std::string>{}(Path), std::hash<std::uintptr_t>{}(Owner));
    if (Parent != nullptr) {
        hash = Math::HashCombine(hash, std::hash<std::string>{}(Parent->GetPath()));
    }
    return hash;
}

size_t ResourceIdentifierHash::operator()(const ResourceIdentifier& rcd) const {
    return rcd.GetHash();
}

ResourceManager::ResourceManager() {
}

void ResourceManager::Init(const std::vector<std::string>& archivePaths,
                           const std::unordered_set<uint32_t>& validHashes, int32_t reservedThreadCount) {
    mResourceLoader = std::make_shared<ResourceLoader>(*this);
    mArchiveManager = std::make_shared<ArchiveManager>();
    GetArchiveManager()->Init(archivePaths, validHashes);

    // Keep one worker free for the main thread and platform callbacks without
    // allowing unsigned subtraction to wrap on low-core systems.
    const BS::concurrency_t hardwareThreads = std::thread::hardware_concurrency();
    const BS::concurrency_t reservedThreads =
        reservedThreadCount > 0 ? static_cast<BS::concurrency_t>(reservedThreadCount) : 0U;
    const BS::concurrency_t threadCount =
        hardwareThreads > reservedThreads + 1U ? hardwareThreads - reservedThreads - 1U : 1U;
    mThreadPool = std::make_shared<BS::thread_pool>(threadCount);

    if (!IsLoaded()) {
        // Nothing ever unpauses the thread pool since nothing will ever try to load the archive again.
        mThreadPool->pause();
    }
}

ResourceManager::~ResourceManager() {
    WriteLog("destruct ResourceManager");
}

bool ResourceManager::IsLoaded() {
    return mArchiveManager != nullptr && mArchiveManager->IsLoaded();
}

std::shared_ptr<File> ResourceManager::LoadFileProcess(const std::string& filePath) {
    auto file = mArchiveManager->LoadFile(filePath);
    if (file != nullptr) {
        WriteLog("Loaded File {} on ResourceManager", filePath);
    } else {
        WriteLog("Could not load File {} in ResourceManager", filePath);
    }
    return file;
}

std::shared_ptr<File> ResourceManager::LoadFileProcess(const ResourceIdentifier& identifier) {
    if (identifier.Parent == nullptr) {
        return LoadFileProcess(identifier.Path);
    }
    auto archive = identifier.Parent;
    auto file = archive->LoadFile(identifier.Path);
    if (file != nullptr) {
        WriteLog("Loaded File {} on ResourceManager", identifier.Path);
    } else {
        WriteLog("Could not load File {} in ResourceManager", identifier.Path);
    }
    return file;
}

std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const ResourceIdentifier& identifier,
                                                                std::shared_ptr<ResourceInitData> initData) {
    // Strip the archive resource signature before resolving the path.
    if (HasResourceSignature(identifier.Path.c_str())) {
        const auto newFilePath = identifier.Path.substr(7);
        return LoadResourceProcess({ newFilePath, identifier.Owner, identifier.Parent }, initData);
    }

    // While waiting in the queue, another thread could have loaded the resource.
    // In a last attempt to avoid doing work that will be discarded, let's check if the cached version exists.
    auto cacheLine = CheckCache(identifier);
    auto cachedResource = GetCachedResource(cacheLine);
    if (cachedResource != nullptr) {
        return cachedResource;
    }

    // Load the serialized resource file from its archive.
    auto file = LoadFileProcess(identifier.Path);
    if (file == nullptr) {
        WriteLog("Failed to load resource file at path {}", identifier.Path);
        mResourceCache[identifier] = ResourceLoadError::NotFound;
        return nullptr;
    }

    // Transform the raw data into a resource
    auto resource = GetResourceLoader()->LoadResource(identifier.Path, file, initData);

    // Another thread could have loaded the resource while we were processing, so we want to check before setting to
    // the cache.
    cachedResource = GetCachedResource(identifier);

    {
        const std::lock_guard<std::mutex> lock(mMutex);

        if (cachedResource != nullptr) {
            // If another thread has already loaded this resource, discard the work we already did and return from
            // cache.
            resource = cachedResource;
        }

        // Set the cache to the loaded resource
        if (resource != nullptr) {
            mResourceCache[identifier] = resource;
        } else {
            mResourceCache[identifier] = ResourceLoadError::NotFound;
        }
    }

    if (resource != nullptr) {
        WriteLog("Loaded Resource {} on ResourceManager", identifier.Path);
    } else {
        WriteLog("Resource load FAILED {} on ResourceManager", identifier.Path);
    }

    return resource;
}

std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const std::string& filePath,
                                                                std::shared_ptr<ResourceInitData> initData) {
    return LoadResourceProcess({ filePath, mDefaultCacheOwner, mDefaultCacheArchive }, initData);
}

std::shared_future<std::shared_ptr<IResource>>
ResourceManager::LoadResourceAsync(const ResourceIdentifier& identifier, BS::priority_t priority,
                                   std::shared_ptr<ResourceInitData> initData) {
    // Strip the archive resource signature before resolving the path.
    if (HasResourceSignature(identifier.Path.c_str())) {
        auto newFilePath = identifier.Path.substr(7);
        return LoadResourceAsync({ newFilePath, identifier.Owner, identifier.Parent }, priority, initData);
    }

    // Check the cache before queueing the job.
    auto cacheCheck = GetCachedResource(identifier);
    if (cacheCheck) {
        auto promise = std::make_shared<std::promise<std::shared_ptr<IResource>>>();
        promise->set_value(cacheCheck);
        return promise->get_future().share();
    }

    return mThreadPool->submit_task(
        [this, identifier, initData]() -> std::shared_ptr<IResource> {
            return LoadResourceProcess(identifier, initData);
        },
        priority);
}

std::shared_future<std::shared_ptr<IResource>>
ResourceManager::LoadResourceAsync(const std::string& filePath, BS::priority_t priority,
                                   std::shared_ptr<ResourceInitData> initData) {
    return LoadResourceAsync({ filePath, mDefaultCacheOwner, mDefaultCacheArchive }, priority, initData);
}

std::shared_ptr<IResource> ResourceManager::LoadResource(const ResourceIdentifier& identifier,
                                                         std::shared_ptr<ResourceInitData> initData) {
    auto resource = LoadResourceAsync(identifier, BS::pr::highest, initData).get();
    if (resource == nullptr) {
        WriteLog("Failed to load resource file at path {}", identifier.Path);
    }
    return resource;
}

std::shared_ptr<IResource> ResourceManager::LoadResource(const std::string& filePath,
                                                         std::shared_ptr<ResourceInitData> initData) {
    return LoadResource({ filePath, mDefaultCacheOwner, mDefaultCacheArchive }, initData);
}

std::shared_ptr<IResource> ResourceManager::LoadResource(uint64_t crc,
                                                         std::shared_ptr<ResourceInitData> initData) {
    const std::string* hashStr = GetArchiveManager()->HashToString(crc);
    if (hashStr == nullptr || hashStr->length() == 0) {
        WriteLog("ResourceLoad: Unknown crc {}\n", crc);
        return nullptr;
    }

    return LoadResource(*hashStr, initData);
}

std::variant<ResourceManager::ResourceLoadError, std::shared_ptr<IResource>>
ResourceManager::CheckCache(const ResourceIdentifier& identifier) {
    const std::lock_guard<std::mutex> lock(mMutex);

    auto cacheFind = mResourceCache.find(identifier);
    if (cacheFind == mResourceCache.end()) {
        return ResourceLoadError::NotCached;
    }

    return cacheFind->second;
}

std::variant<ResourceManager::ResourceLoadError, std::shared_ptr<IResource>>
ResourceManager::CheckCache(const std::string& filePath) {
    return CheckCache({ filePath, mDefaultCacheOwner, mDefaultCacheArchive });
}

std::shared_ptr<IResource> ResourceManager::GetCachedResource(const ResourceIdentifier& identifier) {
    // Gets the cached resource based on filePath.
    return GetCachedResource(CheckCache(identifier));
}

std::shared_ptr<IResource> ResourceManager::GetCachedResource(const std::string& filePath) {
    // Gets the cached resource based on filePath.
    return GetCachedResource({ filePath, mDefaultCacheOwner, mDefaultCacheArchive });
}

std::shared_ptr<IResource>
ResourceManager::GetCachedResource(std::variant<ResourceLoadError, std::shared_ptr<IResource>> cacheLine) {
    // Gets the cached resource based on a cache line std::variant from the cache map.
    if (std::holds_alternative<std::shared_ptr<IResource>>(cacheLine)) {
        try {
            auto resource = std::get<std::shared_ptr<IResource>>(cacheLine);

            if (resource.use_count() <= 0) {
                return nullptr;
            }

            if (resource->IsDirty()) {
                return nullptr;
            }

            return resource;
        } catch (std::bad_variant_access const&) {
            // Ignore the exception
        }
    }

    return nullptr;
}

std::shared_ptr<std::vector<std::shared_ptr<IResource>>>
ResourceManager::LoadResourcesProcess(const ResourceFilter& filter) {
    auto loadedList = std::make_shared<std::vector<std::shared_ptr<IResource>>>();
    auto fileList = GetArchiveManager()->ListFiles(filter.IncludeMasks, filter.ExcludeMasks);
    loadedList->reserve(fileList->size());

    for (size_t i = 0; i < fileList->size(); i++) {
        auto fileName = std::string(fileList->operator[](i));
        auto resource = LoadResource({ fileName, filter.Owner, filter.Parent });
        loadedList->push_back(resource);
    }

    return loadedList;
}

std::shared_future<std::shared_ptr<std::vector<std::shared_ptr<IResource>>>>
ResourceManager::LoadResourcesAsync(const ResourceFilter& filter, BS::priority_t priority) {
    return mThreadPool->submit_task(
        [this, filter]() -> std::shared_ptr<std::vector<std::shared_ptr<IResource>>> {
            return LoadResourcesProcess(filter);
        },
        priority);
}

std::shared_future<std::shared_ptr<std::vector<std::shared_ptr<IResource>>>>
ResourceManager::LoadResourcesAsync(const std::string& searchMask, BS::priority_t priority) {
    return LoadResourcesAsync({ { searchMask }, {}, mDefaultCacheOwner, mDefaultCacheArchive }, priority);
}

std::shared_ptr<std::vector<std::shared_ptr<IResource>>> ResourceManager::LoadResources(const std::string& searchMask) {
    return LoadResources({ { searchMask }, {}, mDefaultCacheOwner, mDefaultCacheArchive });
}

std::shared_ptr<std::vector<std::shared_ptr<IResource>>> ResourceManager::LoadResources(const ResourceFilter& filter) {
    return LoadResourcesAsync(filter, BS::pr::highest).get();
}

void ResourceManager::DirtyResources(const ResourceFilter& filter) {
    mThreadPool->submit_task([this, filter]() -> void {
        auto list = GetArchiveManager()->ListFiles(filter.IncludeMasks, filter.ExcludeMasks);

        for (const auto& key : *list.get()) {
            auto resource = GetCachedResource({ key, filter.Owner, filter.Parent });
            // If it's a resource, we will set the dirty flag, else we will just unload it.
            if (resource != nullptr) {
                resource->Dirty();
            } else {
                UnloadResource({ key, filter.Owner, filter.Parent });
            }
        }
    });
}

void ResourceManager::DirtyResources(const std::string& searchMask) {
    DirtyResources({ { searchMask }, {}, mDefaultCacheOwner, mDefaultCacheArchive });
}

void ResourceManager::UnloadResourcesAsync(const std::string& searchMask, BS::priority_t priority) {
    UnloadResourcesAsync({ { searchMask }, {}, mDefaultCacheOwner, mDefaultCacheArchive }, priority);
}

void ResourceManager::UnloadResourcesAsync(const ResourceFilter& filter, BS::priority_t priority) {
    mThreadPool->submit_task([this, filter]() -> void { UnloadResourcesProcess(filter); }, priority);
}

void ResourceManager::UnloadResources(const std::string& searchMask) {
    UnloadResources({ { searchMask }, {}, mDefaultCacheOwner, mDefaultCacheArchive });
}

void ResourceManager::UnloadResources(const ResourceFilter& filter) {
    UnloadResourcesProcess(filter);
}

void ResourceManager::UnloadResourcesProcess(const ResourceFilter& filter) {
    auto list = GetArchiveManager()->ListFiles(filter.IncludeMasks, filter.ExcludeMasks);

    for (const auto& key : *list.get()) {
        UnloadResource({ key, mDefaultCacheOwner, mDefaultCacheArchive });
    }
}

std::shared_ptr<ArchiveManager> ResourceManager::GetArchiveManager() {
    return mArchiveManager;
}

std::shared_ptr<ResourceLoader> ResourceManager::GetResourceLoader() {
    return mResourceLoader;
}

size_t ResourceManager::UnloadResource(const ResourceIdentifier& identifier) {
    // Store a shared pointer here so that erase doesn't destruct the resource.
    // The resource will attempt to load other resources on the destructor, and this will fail because we already hold
    // the mutex.
    std::variant<ResourceLoadError, std::shared_ptr<IResource>> value = nullptr;
    size_t ret = 0;
    // We can only erase the resource if we have any resources for that owner.
    if (mResourceCache.contains(identifier)) {
        const std::lock_guard<std::mutex> lock(mMutex);
        mResourceCache.erase(identifier);
    }

    return ret;
}

size_t ResourceManager::UnloadResource(const std::string& filePath) {
    return UnloadResource({ filePath, mDefaultCacheOwner, mDefaultCacheArchive });
}

bool ResourceManager::HasResourceSignature(const char* fileName) {
    static constexpr char kResourceSignature[] = "__OTR__";
    return strncmp(fileName, kResourceSignature, sizeof(kResourceSignature) - 1) == 0;
}

size_t ResourceManager::GetResourceSize(std::shared_ptr<IResource> resource) {
    if (resource == nullptr) {
        return 0;
    }

    return resource->GetPointerSize();
}

size_t ResourceManager::GetResourceSize(const char* name) {
    auto resource = LoadResource(name);

    return GetResourceSize(resource);
}

size_t ResourceManager::GetResourceSize(uint64_t crc) {
    auto resource = LoadResource(crc);

    return GetResourceSize(resource);
}

bool ResourceManager::GetResourceIsCustom(std::shared_ptr<IResource> resource) {
    if (resource == nullptr) {
        return false;
    }

    return resource->GetInitData()->IsCustom;
}

bool ResourceManager::GetResourceIsCustom(const char* name) {
    auto resource = LoadResource(name);

    return GetResourceIsCustom(resource);
}

bool ResourceManager::GetResourceIsCustom(uint64_t crc) {
    auto resource = LoadResource(crc);

    return GetResourceIsCustom(resource);
}

void* ResourceManager::GetResourceRawPointer(std::shared_ptr<IResource> resource) {
    if (resource == nullptr) {
        return nullptr;
    }

    return resource->GetRawPointer();
}

void* ResourceManager::GetResourceRawPointer(const char* name) {
    auto resource = LoadResource(name);

    return GetResourceRawPointer(resource);
}

void* ResourceManager::GetResourceRawPointer(uint64_t crc) {
    auto resource = LoadResource(crc);

    return GetResourceRawPointer(resource);
}

} // namespace Engine
