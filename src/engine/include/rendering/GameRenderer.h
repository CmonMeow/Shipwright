#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "rendering/interpreter.h"

union Gfx;

namespace Engine::Rendering {
class GfxRenderingAPI;
class GfxDebugger;
}

class OpenGLPresentation;

namespace Engine {

class ConsoleVariable;
class ResourceManager;

namespace Rendering {

class GameRenderer final {
  public:
    GameRenderer(ConsoleVariable& variables, ResourceManager& resources, GfxDebugger& debugger,
                 OpenGLPresentation& presentation);
    ~GameRenderer();

    void Init();
    uint32_t GetWidth();
    uint32_t GetHeight();
    float GetAspectRatio();
    uint32_t GetCurrentRefreshRate();
    bool CanDisableVerticalSync();

    void SetTargetFps(int32_t fps);
    void GetPixelDepthPrepare(float x, float y);
    uint16_t GetPixelDepth(float x, float y);
    bool DrawAndRunGraphicsCommands(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& matrixReplacements);

    Interpreter* GetInterpreter() const;

  private:
    void SetTextureFilter(FilteringMode filteringMode);

    ConsoleVariable& mVariables;
    OpenGLPresentation& mPresentation;
    std::unique_ptr<GfxRenderingAPI> mRenderingApi;
    std::unique_ptr<Interpreter> mInterpreter;
    bool mRendererInitialized = false;
};

} // namespace Rendering
} // namespace Engine
