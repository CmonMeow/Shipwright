#include <runtime/log/Log.hpp>
#include "engine/Context.h"
#include "fast/debug/GfxDebugger.h"
#include "engine/config/ConsoleVariable.h"
#include "engine/config/Config.h"
#include "engine/debug/Console.h"
#include "engine/debug/CrashHandler.h"
#include "engine/resource/ResourceManager.h"
#include "engine/window/FileDropMgr.h"
#include "engine/window/Window.h"

#ifdef _WIN32
#include <libloaderapi.h>
#include <tchar.h>
#include <windows.h>
#include <stringapiset.h>
#endif

namespace Engine {
std::weak_ptr<Context> Context::mContext;

std::shared_ptr<Context> Context::GetInstance() {
    return mContext.lock();
}

Context::~Context() {
    WriteLog("destruct context");
    GetWindow()->SaveWindowToConfig();

    // Explicitly release subsystems before saving configuration.
    mAudio = nullptr;
    mWindow = nullptr;
    mConsole = nullptr;
    mCrashHandler = nullptr;
    mResourceManager = nullptr;
    mConsoleVariables = nullptr;
    GetConfig()->Save();
    mConfig = nullptr;
}

std::shared_ptr<Context>
Context::CreateInstance(const std::string name, const std::string shortName, const std::string configFilePath,
                        const std::vector<std::string>& archivePaths, const std::unordered_set<uint32_t>& validHashes,
                        uint32_t reservedThreadCount, AudioSettings audioSettings, std::shared_ptr<Window> window) {
    if (mContext.expired()) {
        auto shared = std::make_shared<Context>(name, shortName, configFilePath);
        mContext = shared;
        if (shared->Init(archivePaths, validHashes, reservedThreadCount, audioSettings, window)) {
            return shared;
        } else {
            WriteLog("Failed to initialize");
            return nullptr;
        };
    }

    WriteLog("Trying to create a context when it already exists. Returning existing.");

    return GetInstance();
}

std::shared_ptr<Context> Context::CreateUninitializedInstance(const std::string name, const std::string shortName,
                                                              const std::string configFilePath) {
    if (mContext.expired()) {
        auto shared = std::make_shared<Context>(name, shortName, configFilePath);
        mContext = shared;
        return shared;
    }

    WriteLog("Trying to create an uninitialized context when it already exists. Returning existing.");

    return GetInstance();
}

Context::Context(std::string name, std::string shortName, std::string configFilePath)
    : mConfigFilePath(std::move(configFilePath)), mName(std::move(name)), mShortName(std::move(shortName)) {
}

bool Context::Init(const std::vector<std::string>& archivePaths, const std::unordered_set<uint32_t>& validHashes,
                   uint32_t reservedThreadCount, AudioSettings audioSettings, std::shared_ptr<Window> window) {
    return InitConfiguration() && InitConsoleVariables() &&
           InitResourceManager(archivePaths, validHashes, reservedThreadCount) && InitCrashHandler() && InitConsole() &&
           InitWindow(window) && InitAudio(audioSettings) && InitGfxDebugger() && InitFileDropMgr();
}

bool Context::InitConfiguration() {
    if (GetConfig() != nullptr) {
        return true;
    }

    mConfig = std::make_shared<Config>(GetPathRelativeToAppDirectory(mConfigFilePath));

    if (GetConfig() == nullptr) {
        WriteLog("Failed to initialize config");
        return false;
    }

#ifdef _WIN32
    // Remove settings for retired UI and controller systems so stale mappings and window state cannot survive.
    GetConfig()->EraseBlock("CVars.gSettings.Controllers");
    GetConfig()->EraseBlock("CVars.gSettings.AdvancedResolution");
    GetConfig()->EraseBlock("CVars.gSettings.Menu");
    GetConfig()->EraseBlock("CVars.gOpenWindows");
    GetConfig()->Erase("CVars.gSettings.OverlayFont");
    GetConfig()->Save();
#endif

    return true;
}

bool Context::InitConsoleVariables() {
    if (GetConsoleVariables() != nullptr) {
        return true;
    }

    mConsoleVariables = std::make_shared<ConsoleVariable>();

    if (GetConsoleVariables() == nullptr) {
        WriteLog("Failed to initialize console variables");
        return false;
    }

    return true;
}

bool Context::InitResourceManager(const std::vector<std::string>& archivePaths,
                                  const std::unordered_set<uint32_t>& validHashes, uint32_t reservedThreadCount,
                                  const bool allowEmptyPaths) {
    if (GetResourceManager() != nullptr) {
        return true;
    }

    mMainPath = GetConfig()->GetString("Game.Main Archive", GetAppDirectoryPath());
    mPatchesPath = GetConfig()->GetString("Game.Patches Archive", GetAppDirectoryPath() + "/mods");
    if (archivePaths.empty()) {
        std::vector<std::string> paths = std::vector<std::string>();
        paths.push_back(mMainPath);
        paths.push_back(mPatchesPath);

        mResourceManager = std::make_shared<ResourceManager>();
        GetResourceManager()->Init(paths, validHashes, reservedThreadCount);
    } else {
        mResourceManager = std::make_shared<ResourceManager>();
        GetResourceManager()->Init(archivePaths, validHashes, reservedThreadCount);
    }

    if (!allowEmptyPaths && !GetResourceManager()->IsLoaded()) {
#ifdef _WIN32
        MessageBoxA(nullptr, "Main OTR file not found. Please generate one", "OTR file not found",
                    MB_OK | MB_ICONERROR);
#else
        std::cerr << "OTR file not found: Main OTR file not found. Please generate one" << std::endl;
#endif
        WriteLog("Main OTR file not found!");
#ifdef __IOS__
        // We need this exit to close the app when we dismiss the dialog
        exit(0);
#endif
        return false;
    }

    return true;
}

bool Context::InitCrashHandler() {
    if (GetCrashHandler() != nullptr) {
        return true;
    }

    mCrashHandler = std::make_shared<CrashHandler>();

    if (GetCrashHandler() == nullptr) {
        WriteLog("Failed to initialize crash handler");
        return false;
    }

    return true;
}

bool Context::InitAudio(AudioSettings settings) {
    if (GetAudio() != nullptr) {
        return true;
    }

    mAudio = std::make_shared<Audio>(settings);

    if (GetAudio() == nullptr) {
        WriteLog("Failed to initialize audio");
        return false;
    }

    GetAudio()->Init();
    return true;
}

bool Context::InitGfxDebugger() {
    if (GetGfxDebugger() != nullptr) {
        return true;
    }

    mGfxDebugger = std::make_shared<Fast::GfxDebugger>();

    if (GetGfxDebugger() == nullptr) {
        WriteLog("Failed to initialize gfx debugger");
        return false;
    }

    return true;
}

bool Context::InitConsole() {
    if (GetConsole() != nullptr) {
        return true;
    }

    mConsole = std::make_shared<Console>();

    if (GetConsole() == nullptr) {
        WriteLog("Failed to initialize console");
        return false;
    }

    GetConsole()->Init();

    return true;
}

bool Context::InitWindow(std::shared_ptr<Window> window) {
    if (GetWindow() != nullptr) {
        return true;
    }

    mWindow = window;

    if (GetWindow() == nullptr) {
        WriteLog("Failed to initialize window");
        return false;
    }

    GetWindow()->Init();

    return true;
}

bool Context::InitFileDropMgr() {
    if (GetFileDropMgr() != nullptr) {
        return true;
    }

    mFileDropMgr = std::make_shared<FileDropMgr>();
    if (GetFileDropMgr() == nullptr) {
        WriteLog("Failed to initialize file drop manager");
        return false;
    }
    return true;
}

std::shared_ptr<ConsoleVariable> Context::GetConsoleVariables() {
    return mConsoleVariables;
}

std::shared_ptr<Config> Context::GetConfig() {
    return mConfig;
}

std::shared_ptr<ResourceManager> Context::GetResourceManager() {
    return mResourceManager;
}

std::shared_ptr<CrashHandler> Context::GetCrashHandler() {
    return mCrashHandler;
}

std::shared_ptr<Window> Context::GetWindow() {
    return mWindow;
}

std::shared_ptr<Console> Context::GetConsole() {
    return mConsole;
}

std::shared_ptr<Audio> Context::GetAudio() {
    return mAudio;
}

std::shared_ptr<Fast::GfxDebugger> Context::GetGfxDebugger() {
    return mGfxDebugger;
}

std::shared_ptr<FileDropMgr> Context::GetFileDropMgr() {
    return mFileDropMgr;
}

std::string Context::GetName() {
    return mName;
}

std::string Context::GetShortName() {
    return mShortName;
}

std::string Context::GetAppBundlePath() {
#ifdef _WIN32
    std::wstring progpath(MAX_PATH, '\0');

    int len = GetModuleFileNameW(NULL, &progpath[0], progpath.size());
    if (len != 0 && len < progpath.size()) {
        progpath.resize(len);

        // Find the last '\' and remove everything after it
        long unsigned int lastSlash = progpath.find_last_of('\\');
        if (lastSlash != std::string::npos) {
            progpath.erase(lastSlash);
        }

        // Convert wstring to string
        len = WideCharToMultiByte(CP_UTF8, 0, progpath.data(), (int)progpath.size(), nullptr, 0, nullptr, nullptr);
        std::string newProgpath(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, progpath.data(), (int)progpath.size(), &newProgpath[0], len, nullptr, nullptr);

        return newProgpath;
    }
#endif

    return ".";
}

std::string Context::GetAppDirectoryPath(std::string) {
    return ".";
}

std::string Context::GetPathRelativeToAppBundle(const std::string path) {
    return GetAppBundlePath() + "/" + path;
}

std::string Context::GetPathRelativeToAppDirectory(const std::string path, std::string appName) {
    return GetAppDirectoryPath(appName) + "/" + path;
}

std::string Context::LocateFileAcrossAppDirs(const std::string path, std::string appName) {
    std::string fpath;

    // app configuration dir
    fpath = GetPathRelativeToAppDirectory(path, appName);
    if (std::filesystem::exists(fpath)) {
        return fpath;
    }
    // app install dir
    fpath = GetPathRelativeToAppBundle(path);
    if (std::filesystem::exists(fpath)) {
        return fpath;
    }
    // current dir
    return "./" + std::string(path);
}

} // namespace Engine
