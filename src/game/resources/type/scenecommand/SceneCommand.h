#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <engine/resource/Resource.h>
#include <runtime/libultra/types.h>

namespace Game::Resources {

enum class SceneCommandID : uint8_t {
    SetStartPositionList = 0x00,
    SetCollisionHeader = 0x03,
    SetRoomList = 0x04,
    SetEntranceList = 0x06,
    SetSpecialObjects = 0x07,
    SetRoomBehavior = 0x08,
    SetMesh = 0x0A,
    SetLightingSettings = 0x0F,
    SetTimeSettings = 0x10,
    SetSkyboxSettings = 0x11,
    SetSkyboxModifier = 0x12,
    EndMarker = 0x14,
    SetSoundSettings = 0x15,
    SetEchoSettings = 0x16,
    SetCameraSettings = 0x19,

    Error = 0xFF
};

class ISceneCommand : public Engine::IResource {
  public:
    using IResource::IResource;
    ISceneCommand() : IResource(std::shared_ptr<Engine::ResourceInitData>()) {
    }
    SceneCommandID cmdId;
};

template <class T> class SceneCommand : public ISceneCommand {
  public:
    using ISceneCommand::ISceneCommand;
    virtual T* GetPointer() = 0;
    void* GetRawPointer() override {
        return static_cast<void*>(GetPointer());
    }
};

}; // namespace Game::Resources
