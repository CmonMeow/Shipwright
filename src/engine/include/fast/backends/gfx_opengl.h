#ifdef ENABLE_OPENGL
#pragma once

#include "gfx_rendering_api.h"
#include "../interpreter.h"

#include "fast/backends/OpenGLExtensions.h"
namespace Fast {
struct ShaderProgram {
    GLuint openglProgramId;
    uint8_t numInputs;
    bool usedTextures[SHADER_MAX_TEXTURES];
    uint8_t numFloats;
    GLint attribLocations[16];
    uint8_t attribSizes[16];
    uint8_t numAttribs;
    GLint frameCountLocation;
    GLint noiseScaleLocation;
    GLint texture_width_location;
    GLint texture_height_location;
    GLint texture_filtering_location;
    bool isWater;
    GLint waterNormalLocation;
    GLint waterSceneLocation;
    GLint waterScreenSizeLocation;
    GLint waterTimeLocation;
    int waterLevel;
};

struct FramebufferOGL {
    uint32_t width, height;
    bool has_depth_buffer;
    uint32_t msaa_level;
    bool invertY;

    GLuint fbo, clrbuf, clrbufMsaa, rbo;
};

class GfxRenderingAPIOGL final : public GfxRenderingAPI {
  public:
    ~GfxRenderingAPIOGL() override;
    const char* GetName() override;
    int GetMaxTextureSize() override;
    GfxClipParameters GetClipParameters() override;
    void UnloadShader(ShaderProgram* oldPrg) override;
    void LoadShader(ShaderProgram* newPrg) override;
    ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint32_t shaderId1) override;
    ShaderProgram* LookupShader(uint64_t shaderId0, uint32_t shaderId1) override;
    void ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) override;
    uint32_t NewTexture() override;
    void SelectTexture(int tile, uint32_t textureId) override;
    void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) override;
    void SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) override;
    void SetDepthTestAndMask(bool depth_test, bool z_upd) override;
    void SetZmodeDecal(bool decal) override;
    void SetViewport(int x, int y, int width, int height) override;
    void SetScissor(int x, int y, int width, int height) override;
    void SetUseAlpha(bool useAlpha) override;
    void DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) override;
    void Init() override;
    void Shutdown() override;
    void OnResize() override;
    void StartFrame() override;
    void EndFrame() override;
    void FinishRender() override;
    int CreateFramebuffer() override;
    void UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                     bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                     bool can_extract_depth) override;
    void StartDrawToFramebuffer(int fbId, float noiseScale) override;
    void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0,
                         int dstX1, int dstY1) override;
    void ClearFramebuffer(bool color, bool depth) override;
    void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) override;
    void ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) override;
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
    GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) override;
    void* GetFramebufferTextureId(int fbId) override;
    void SelectTextureFb(int fbId) override;
    void DeleteTexture(uint32_t texId) override;
    void SetTextureFilter(FilteringMode mode) override;
    FilteringMode GetTextureFilter() override;
    void SetSrgbMode() override;

  private:
    void SetUniforms(ShaderProgram* prg) const;
    std::string BuildFsShader(const CCFeatures& cc_features);
    void SetPerDrawUniforms();
    bool EnsureWaterResources();
    bool EnsureWaterMaskTexture(int level);
    void StepWaterSimulation(float time);
    void CaptureWaterScene();
    void PrepareWaterShader(ShaderProgram* prg);
    GLuint CompileWaterProgram(const char* vertexSource, const char* fragmentSource);
    void DestroyWaterResources();

    struct TextureInfo {
        uint16_t width;
        uint16_t height;
        uint16_t filtering;
    } textures[1024];

    GLuint mCurrentTextureIds[SHADER_MAX_TEXTURES];
    uint8_t mCurrentTile;

    std::map<std::pair<uint64_t, uint32_t>, ShaderProgram> mShaderProgramPool;
    ShaderProgram* mCurrentShaderProgram;

    GLuint mOpenglVbo = 0;
#if defined(__APPLE__) || defined(USE_OPENGLES)
    GLuint mOpenglVao;
#endif

    uint32_t mFrameCount = 0;

    std::vector<FramebufferOGL> mFrameBuffers;
    size_t mCurrentFrameBuffer = 0;
    float mCurrentNoiseScale = 0.0f;
    FilteringMode mCurrentFilterMode = FILTER_THREE_POINT;

    GLint mMaxMsaaLevel = 1;
    GLuint mPixelDepthRb = 0;
    GLuint mPixelDepthFb = 0;
    size_t mPixelDepthRbSize = 0;

    static constexpr int mWaterSimulationSize = 512;
    GLuint mWaterSimulationProgram = 0;
    GLuint mWaterDropProgram = 0;
    GLuint mWaterRenderProgram = 0;
    GLuint mWaterSimulationTextures[2] = { 0, 0 };
    GLuint mWaterNoiseTexture = 0;
    GLuint mWaterMaskTextures[8] = {};
    uint32_t mWaterMaskVersions[8] = {};
    GLuint mActiveWaterMaskTexture = 0;
    GLuint mWaterSimulationFbo = 0;
    GLuint mWaterSceneTexture = 0;
    GLuint mWaterSceneFbo = 0;
    GLuint mWaterQuadVbo = 0;
    int mWaterTextureIndex = 0;
    uint32_t mWaterSceneWidth = 0;
    uint32_t mWaterSceneHeight = 0;
    uint32_t mWaterCapturedFrame = UINT32_MAX;
    double mWaterLastTime = 0.0;
    double mWaterAccumulator = 0.0;
    bool mWaterResourcesTried = false;
    bool mWaterResourcesReady = false;
};

} // namespace Fast
#endif
