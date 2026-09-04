#include <runtime/log/Log.hpp>
#ifdef ENABLE_OPENGL

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <map>
#include <unordered_map>
#include <chrono>
#include <cmath>
#include <atomic>
#include <array>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#include "rendering/backends/gfx_opengl.h"
#include <prism/processor.h>
#include <fstream>
#include "engine/resource/ResourceManager.h"
#include "engine/resource/factory/ShaderFactory.h"
#include "rendering/interpreter.h"
#include "rendering/water_renderer.h"
#include "engine/config/ConsoleVariable.h"

static std::atomic<float> sPendingWaterDropU{ 0.5f };
static std::atomic<float> sPendingWaterDropV{ 0.5f };
static std::atomic<float> sPendingWaterDropRadius{ 0.02f };
static std::atomic<float> sPendingWaterDropStrength{ 0.0f };
static constexpr int kWaterMaskLevels = 8;
static constexpr int kWaterMaskSize = 64;
static std::array<std::array<unsigned char, kWaterMaskSize * kWaterMaskSize>, kWaterMaskLevels> sWaterMasks{};
static std::array<std::atomic<uint32_t>, kWaterMaskLevels> sWaterMaskVersions{};
static std::mutex sWaterMaskMutex;

extern "C" void gfx_queue_water_drop(float u, float v, float radius, float strength) {
    if (strength <= sPendingWaterDropStrength.load(std::memory_order_relaxed)) {
        return;
    }
    sPendingWaterDropU.store(u, std::memory_order_relaxed);
    sPendingWaterDropV.store(v, std::memory_order_relaxed);
    sPendingWaterDropRadius.store(radius, std::memory_order_relaxed);
    sPendingWaterDropStrength.store(strength, std::memory_order_release);
}

extern "C" void gfx_set_water_mask(int level, const unsigned char* data, int size) {
    if (level < 0 || level >= kWaterMaskLevels || data == nullptr || size != kWaterMaskSize) {
        return;
    }
    std::lock_guard<std::mutex> lock(sWaterMaskMutex);
    std::memcpy(sWaterMasks[level].data(), data, sWaterMasks[level].size());
    sWaterMaskVersions[level].fetch_add(1, std::memory_order_release);
}

namespace Engine::Rendering {
GfxRenderingAPIOGL::GfxRenderingAPIOGL(Engine::ResourceManager& resources, Engine::ConsoleVariable& variables)
    : mResources(resources), mVariables(variables) {
}

int GfxRenderingAPIOGL::GetMaxTextureSize() {
    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    return max_texture_size;
}

const char* GfxRenderingAPIOGL::GetName() {
    return "OpenGL";
}

GfxRenderingAPIOGL::~GfxRenderingAPIOGL() = default;

bool GfxRenderingAPIOGL::EnsureWaterMaskTexture(int level) {
    if (level < 0 || level >= kWaterMaskLevels) {
        return false;
    }
    const uint32_t version = sWaterMaskVersions[level].load(std::memory_order_acquire);
    if (version == 0) {
        return false;
    }
    if (mWaterMaskTextures[level] == 0) {
        glGenTextures(1, &mWaterMaskTextures[level]);
    }
    if (mWaterMaskVersions[level] != version) {
        std::lock_guard<std::mutex> lock(sWaterMaskMutex);
        glBindTexture(GL_TEXTURE_2D, mWaterMaskTextures[level]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kWaterMaskSize, kWaterMaskSize, 0, GL_RED, GL_UNSIGNED_BYTE,
                     sWaterMasks[level].data());
        mWaterMaskVersions[level] = version;
    }
    mActiveWaterMaskTexture = mWaterMaskTextures[level];
    return true;
}

GLuint GfxRenderingAPIOGL::CompileWaterProgram(const char* vertexSource, const char* fragmentSource) {
    const auto compile = [](GLenum type, const char* source) -> GLuint {
        const GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled == GL_TRUE) {
            return shader;
        }
        GLchar log[2048]{};
        GLsizei length = 0;
        glGetShaderInfoLog(shader, sizeof(log), &length, log);
        WriteLog("Water shader compile failed: {}", log);
        glDeleteShader(shader);
        return 0;
    };

    const GLuint vertex = compile(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aUv");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }
    GLchar log[2048]{};
    GLsizei length = 0;
    glGetProgramInfoLog(program, sizeof(log), &length, log);
    WriteLog("Water shader link failed: {}", log);
    glDeleteProgram(program);
    return 0;
}

bool GfxRenderingAPIOGL::EnsureWaterResources() {
    if (mWaterResourcesTried) {
        return mWaterResourcesReady;
    }
    mWaterResourcesTried = true;

    static const char* simulationVertex = R"GLSL(#version 130
in vec2 aPosition;
out vec2 vUv;
void main() {
    vUv = aPosition * 0.5 + 0.5;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)GLSL";
    static const char* simulationFragment = R"GLSL(#version 130
uniform sampler2D sNormal;
uniform sampler2D sNoise;
uniform sampler2D sMask;
uniform float uTime;
in vec2 vUv;
void main() {
    vec2 texel = vec2(1.0 / 512.0);
    vec2 state = texture2D(sNormal, vUv).xy;
    float hL = texture2D(sNormal, vUv - vec2(texel.x, 0.0)).x;
    float hR = texture2D(sNormal, vUv + vec2(texel.x, 0.0)).x;
    float hD = texture2D(sNormal, vUv - vec2(0.0, texel.y)).x;
    float hU = texture2D(sNormal, vUv + vec2(0.0, texel.y)).x;
    float velocity = state.y + ((hL + hR + hD + hU) * 0.25 - state.x) * 0.72;
    velocity *= 0.994;
    vec2 noiseUv = fract(vUv * 4.0 + vec2(uTime * 0.011, -uTime * 0.008));
    float noise = texture2D(sNoise, noiseUv).r * 2.0 - 1.0;
    float height = state.x + velocity + noise * 0.00025;
    vec2 masked = vec2(height, velocity) * texture2D(sMask, vUv).x;
    gl_FragColor = vec4(masked, 0.0, 0.0);
}
)GLSL";
    static const char* dropFragment = R"GLSL(#version 130
uniform sampler2D sNormal;
uniform vec4 uTouch;
in vec2 vUv;
const float PI = 3.141592653589793;
void main() {
    vec2 state = texture2D(sNormal, vUv).xy;
    float amount = max(0.0, 1.0 - length(uTouch.xy - vUv) / uTouch.z);
    amount = 0.5 - cos(amount * PI) * 0.5;
    state.x += amount * uTouch.w;
    gl_FragColor = vec4(state, 0.0, 0.0);
}
)GLSL";
    // This is the game water surface shader, kept as a dedicated
    // program instead of feeding it through OOT's generated color combiner.
    // OOT still supplies the projected water-box triangles and depth buffer.
    static const char* waterVertex = R"GLSL(#version 130
in vec4 aPosition;
in vec2 aUv;
out vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = aPosition;
}
)GLSL";
    static const char* waterFragment = R"GLSL(#version 130
#define UNDERWATER_COLOR vec3(0.6, 0.9, 0.9)
uniform sampler2D sNormal;
uniform sampler2D sScene;
uniform sampler2D sMask;
uniform vec2 uScreenSize;
in vec2 vUv;

float heightAt(vec2 uv) {
    return texture2D(sNormal, uv).x;
}

float calcFresnel(float NdotV, float f0) {
    return f0 + (1.0 - f0) * pow(1.0 - NdotV, 5.0);
}

vec3 calcNormal(vec2 tc, float base) {
    vec2 texel = vec2(1.0 / 512.0);
    float dx = heightAt(vec2(tc.x + texel.x, tc.y)) - base;
    float dz = heightAt(vec2(tc.x, tc.y + texel.y)) - base;
    return normalize(vec3(dx, 64.0 / (1024.0 * 8.0), dz));
}

void main() {
    float waterMask = smoothstep(0.05, 0.95, texture2D(sMask, vUv).x);
    if (waterMask < 0.01) {
        discard;
    }
    vec2 screenUv = gl_FragCoord.xy / max(uScreenSize, vec2(1.0));
    float value = heightAt(vUv);
    vec3 normal = calcNormal(vUv, value);
    vec3 viewVec = normalize(vec3((screenUv - vec2(0.5)) * 0.7, 1.0));
    vec3 lightVec = normalize(vec3(0.35, 0.8, 0.25));
    vec3 rv = reflect(-viewVec, normal);
    float spec = pow(max(0.0, dot(rv, lightVec)), 64.0) * 0.5;
    vec2 dudv = normal.xz;
    vec4 refr = texture2D(sScene,
        clamp(screenUv + dudv * 0.065, vec2(0.001), vec2(0.999)));
    vec4 refl = texture2D(sScene,
        clamp(vec2(screenUv.x, 1.0 - screenUv.y) + dudv * 0.020, vec2(0.001), vec2(0.999)));
    float fresnel = calcFresnel(abs(dot(normal, viewVec)), 0.12);
    vec4 color = mix(refr, refl, fresnel);
    float slope = clamp(length(dudv) * 850.0, 0.0, 1.0);
    color.xyz = mix(color.xyz, UNDERWATER_COLOR, 0.18);
    color.xyz += slope * vec3(0.06, 0.13, 0.14);
    color.xyz += spec * 1.2;
    color.xyz = mix(color.xyz, color.xyz * UNDERWATER_COLOR, 0.28);
    float edgeFade = smoothstep(0.0, 0.10, min(min(vUv.x, vUv.y), min(1.0 - vUv.x, 1.0 - vUv.y)));
    gl_FragColor = vec4(color.xyz, 0.52 * edgeFade * waterMask);
}
)GLSL";
    mWaterSimulationProgram = CompileWaterProgram(simulationVertex, simulationFragment);
    mWaterDropProgram = CompileWaterProgram(simulationVertex, dropFragment);
    mWaterRenderProgram = CompileWaterProgram(waterVertex, waterFragment);
    if (!mWaterSimulationProgram || !mWaterDropProgram || !mWaterRenderProgram) {
        return false;
    }

    const float quad[] = { -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
                           -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f };
    glGenBuffers(1, &mWaterQuadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mWaterQuadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, mOpenglVbo);

    std::vector<float> initial(static_cast<size_t>(mWaterSimulationSize) * mWaterSimulationSize * 4, 0.0f);
    for (int y = 0; y < mWaterSimulationSize; ++y) {
        for (int x = 0; x < mWaterSimulationSize; ++x) {
            const float fx = static_cast<float>(x) / mWaterSimulationSize;
            const float fy = static_cast<float>(y) / mWaterSimulationSize;
            initial[(static_cast<size_t>(y) * mWaterSimulationSize + x) * 4] =
                (std::sin(fx * 18.8495559f) + std::sin((fx + fy) * 12.5663706f)) * 0.00018f;
        }
    }
    glGenTextures(2, mWaterSimulationTextures);
    for (GLuint texture : mWaterSimulationTextures) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, mWaterSimulationSize, mWaterSimulationSize, 0, GL_RGBA, GL_FLOAT,
                     initial.data());
    }

    std::vector<uint8_t> noise(32 * 32 * 4);
    uint32_t seed = 0x01234567u;
    for (uint8_t& value : noise) {
        seed = seed * 1664525u + 1013904223u;
        value = static_cast<uint8_t>((seed >> 16) & 0xFF);
    }
    glGenTextures(1, &mWaterNoiseTexture);
    glBindTexture(GL_TEXTURE_2D, mWaterNoiseTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, noise.data());

    glGenFramebuffers(1, &mWaterSimulationFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, mWaterSimulationFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mWaterSimulationTextures[1], 0);
    mWaterResourcesReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[mCurrentFrameBuffer].fbo);
    glBindTexture(GL_TEXTURE_2D, 0);
    mWaterAccumulator = 1.0 / 40.0;
    if (!mWaterResourcesReady) {
        WriteLog("Water simulation framebuffer is incomplete");
    } else {
        WriteLog("Game water simulation initialized at {}x{}", mWaterSimulationSize,
                      mWaterSimulationSize);
    }
    return mWaterResourcesReady;
}

void GfxRenderingAPIOGL::StepWaterSimulation(float time) {
    if (!EnsureWaterResources()) {
        return;
    }
    if (mWaterLastTime == 0.0) {
        mWaterLastTime = time;
    }
    double elapsed = static_cast<double>(time) - mWaterLastTime;
    if (elapsed < 0.0 || elapsed > 0.25) {
        elapsed = 0.0;
    }
    mWaterLastTime = time;
    mWaterAccumulator += elapsed;
    constexpr double step = 1.0 / 40.0;
    if (mWaterAccumulator < step) {
        return;
    }

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_VIEWPORT_BIT | GL_SCISSOR_BIT);
    glViewport(0, 0, mWaterSimulationSize, mWaterSimulationSize);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindBuffer(GL_ARRAY_BUFFER, mWaterQuadVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glUseProgram(mWaterSimulationProgram);
    glUniform1i(glGetUniformLocation(mWaterSimulationProgram, "sNormal"), 6);
    glUniform1i(glGetUniformLocation(mWaterSimulationProgram, "sNoise"), 8);
    glUniform1i(glGetUniformLocation(mWaterSimulationProgram, "sMask"), 0);
    glUniform1f(glGetUniformLocation(mWaterSimulationProgram, "uTime"), time);

    int iterations = 0;
    while (mWaterAccumulator >= step && iterations++ < 4) {
        const float dropStrength = sPendingWaterDropStrength.exchange(0.0f, std::memory_order_acq_rel);
        if (dropStrength > 0.0f) {
            const int readIndex = mWaterTextureIndex;
            const int writeIndex = 1 - readIndex;
            glBindFramebuffer(GL_FRAMEBUFFER, mWaterSimulationFbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   mWaterSimulationTextures[writeIndex], 0);
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, mWaterSimulationTextures[readIndex]);
            glUseProgram(mWaterDropProgram);
            glUniform1i(glGetUniformLocation(mWaterDropProgram, "sNormal"), 6);
            glUniform4f(glGetUniformLocation(mWaterDropProgram, "uTouch"),
                        sPendingWaterDropU.load(std::memory_order_relaxed),
                        sPendingWaterDropV.load(std::memory_order_relaxed),
                        sPendingWaterDropRadius.load(std::memory_order_relaxed), -dropStrength);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            mWaterTextureIndex = writeIndex;
        }
        const int readIndex = mWaterTextureIndex;
        const int writeIndex = 1 - readIndex;
        glBindFramebuffer(GL_FRAMEBUFFER, mWaterSimulationFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mWaterSimulationTextures[writeIndex], 0);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, mWaterSimulationTextures[readIndex]);
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, mWaterNoiseTexture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mActiveWaterMaskTexture);
        glUseProgram(mWaterSimulationProgram);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        mWaterTextureIndex = writeIndex;
        mWaterAccumulator -= step;
    }

    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, mOpenglVbo);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[mCurrentFrameBuffer].fbo);
    glActiveTexture(GL_TEXTURE0);
    glPopAttrib();
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

void GfxRenderingAPIOGL::CaptureWaterScene() {
    if (!mWaterResourcesReady || mWaterCapturedFrame == mFrameCount || mCurrentFrameBuffer >= mFrameBuffers.size()) {
        return;
    }
    const FramebufferOGL& source = mFrameBuffers[mCurrentFrameBuffer];
    if (source.width == 0 || source.height == 0) {
        return;
    }
    if (!mWaterSceneTexture) {
        glGenTextures(1, &mWaterSceneTexture);
        glGenFramebuffers(1, &mWaterSceneFbo);
    }
    if (mWaterSceneWidth != source.width || mWaterSceneHeight != source.height) {
        mWaterSceneWidth = source.width;
        mWaterSceneHeight = source.height;
        glBindTexture(GL_TEXTURE_2D, mWaterSceneTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, source.width, source.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mWaterSceneFbo);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mWaterSceneTexture, 0);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mWaterSceneFbo);
    glBlitFramebuffer(0, 0, source.width, source.height, 0, 0, source.width, source.height, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, source.fbo);
    mWaterCapturedFrame = mFrameCount;
}

void GfxRenderingAPIOGL::PrepareWaterShader(ShaderProgram* prg) {
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const float time = static_cast<float>(std::fmod(seconds, 3600.0));
    EnsureWaterMaskTexture(prg->waterLevel);
    StepWaterSimulation(time);
    CaptureWaterScene();
    glUseProgram(prg->openglProgramId);
    if (!mWaterResourcesReady || !mWaterSceneTexture) {
        return;
    }
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, mWaterSimulationTextures[mWaterTextureIndex]);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, mWaterSceneTexture);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(prg->waterNormalLocation, 6);
    glUniform1i(prg->waterSceneLocation, 7);
    glUniform2f(prg->waterScreenSizeLocation, static_cast<float>(mWaterSceneWidth),
                static_cast<float>(mWaterSceneHeight));
    glUniform1f(prg->waterTimeLocation, time);
}

void GfxRenderingAPIOGL::DestroyWaterResources() {
    if (mWaterSimulationProgram) glDeleteProgram(mWaterSimulationProgram);
    if (mWaterDropProgram) glDeleteProgram(mWaterDropProgram);
    if (mWaterRenderProgram) glDeleteProgram(mWaterRenderProgram);
    if (mWaterQuadVbo) glDeleteBuffers(1, &mWaterQuadVbo);
    if (mWaterSimulationTextures[0] || mWaterSimulationTextures[1]) glDeleteTextures(2, mWaterSimulationTextures);
    if (mWaterNoiseTexture) glDeleteTextures(1, &mWaterNoiseTexture);
    glDeleteTextures(kWaterMaskLevels, mWaterMaskTextures);
    if (mWaterSceneTexture) glDeleteTextures(1, &mWaterSceneTexture);
    if (mWaterSimulationFbo) glDeleteFramebuffers(1, &mWaterSimulationFbo);
    if (mWaterSceneFbo) glDeleteFramebuffers(1, &mWaterSceneFbo);
    mWaterSimulationProgram = 0;
    mWaterDropProgram = 0;
    mWaterRenderProgram = 0;
    mWaterQuadVbo = 0;
    mWaterSimulationTextures[0] = mWaterSimulationTextures[1] = 0;
    mWaterNoiseTexture = mWaterSceneTexture = 0;
    std::fill(std::begin(mWaterMaskTextures), std::end(mWaterMaskTextures), 0);
    std::fill(std::begin(mWaterMaskVersions), std::end(mWaterMaskVersions), 0);
    mActiveWaterMaskTexture = 0;
    mWaterSimulationFbo = mWaterSceneFbo = 0;
    mWaterResourcesReady = false;
}

GfxClipParameters GfxRenderingAPIOGL::GetClipParameters() {
    return { false, mFrameBuffers[mCurrentFrameBuffer].invertY };
}

static void VertexArraySetAttribs(ShaderProgram* prg) {
    size_t numFloats = prg->numFloats;
    size_t pos = 0;

    for (int i = 0; i < prg->numAttribs; i++) {
        glEnableVertexAttribArray(prg->attribLocations[i]);
        glVertexAttribPointer(prg->attribLocations[i], prg->attribSizes[i], GL_FLOAT, GL_FALSE,
                              numFloats * sizeof(float), (void*)(pos * sizeof(float)));
        pos += prg->attribSizes[i];
    }
}

void GfxRenderingAPIOGL::SetUniforms(ShaderProgram* prg) const {
    glUniform1i(prg->frameCountLocation, mFrameCount);
    glUniform1f(prg->noiseScaleLocation, mCurrentNoiseScale);
}

void GfxRenderingAPIOGL::SetPerDrawUniforms() {
    if (mCurrentShaderProgram->usedTextures[0] || mCurrentShaderProgram->usedTextures[1]) {
        GLint filtering[2] = { textures[mCurrentTextureIds[0]].filtering, textures[mCurrentTextureIds[1]].filtering };
        glUniform1iv(mCurrentShaderProgram->texture_filtering_location, 2, filtering);

        GLint width[2] = { textures[mCurrentTextureIds[0]].width, textures[mCurrentTextureIds[1]].width };
        glUniform1iv(mCurrentShaderProgram->texture_width_location, 2, width);

        GLint height[2] = { textures[mCurrentTextureIds[0]].height, textures[mCurrentTextureIds[1]].height };
        glUniform1iv(mCurrentShaderProgram->texture_height_location, 2, height);
    }
}

void GfxRenderingAPIOGL::UnloadShader(ShaderProgram* old_prg) {
    if (old_prg != nullptr) {
        for (unsigned int i = 0; i < old_prg->numAttribs; i++) {
            glDisableVertexAttribArray(old_prg->attribLocations[i]);
        }
    }
}

void GfxRenderingAPIOGL::LoadShader(ShaderProgram* new_prg) {
    mCurrentShaderProgram = new_prg;
    if (new_prg->isWater) {
        PrepareWaterShader(new_prg);
    } else {
        glUseProgram(new_prg->openglProgramId);
    }
    VertexArraySetAttribs(new_prg);
    SetUniforms(new_prg);
}

#define RAND_NOISE "((random(vec3(floor(gl_FragCoord.xy * noise_scale), float(frame_count))) + 1.0) / 2.0)"

static const char* shader_item_to_str(uint32_t item, bool with_alpha, bool only_alpha, bool inputs_have_alpha,
                                      bool first_cycle, bool hint_single_element) {
    if (!only_alpha) {
        switch (item) {
            case SHADER_0:
                return with_alpha ? "vec4(0.0, 0.0, 0.0, 0.0)" : "vec3(0.0, 0.0, 0.0)";
            case SHADER_1:
                return with_alpha ? "vec4(1.0, 1.0, 1.0, 1.0)" : "vec3(1.0, 1.0, 1.0)";
            case SHADER_INPUT_1:
                return with_alpha || !inputs_have_alpha ? "vInput1" : "vInput1.rgb";
            case SHADER_INPUT_2:
                return with_alpha || !inputs_have_alpha ? "vInput2" : "vInput2.rgb";
            case SHADER_INPUT_3:
                return with_alpha || !inputs_have_alpha ? "vInput3" : "vInput3.rgb";
            case SHADER_INPUT_4:
                return with_alpha || !inputs_have_alpha ? "vInput4" : "vInput4.rgb";
            case SHADER_TEXEL0:
                return first_cycle ? (with_alpha ? "texVal0" : "texVal0.rgb")
                                   : (with_alpha ? "texVal1" : "texVal1.rgb");
            case SHADER_TEXEL0A:
                return first_cycle
                           ? (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"))
                           : (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"));
            case SHADER_TEXEL1A:
                return first_cycle
                           ? (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"))
                           : (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"));
            case SHADER_TEXEL1:
                return first_cycle ? (with_alpha ? "texVal1" : "texVal1.rgb")
                                   : (with_alpha ? "texVal0" : "texVal0.rgb");
            case SHADER_COMBINED:
                return with_alpha ? "texel" : "texel.rgb";
            case SHADER_NOISE:
                return with_alpha ? "vec4(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")"
                                  : "vec3(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")";
        }
    } else {
        switch (item) {
            case SHADER_0:
                return "0.0";
            case SHADER_1:
                return "1.0";
            case SHADER_INPUT_1:
                return "vInput1.a";
            case SHADER_INPUT_2:
                return "vInput2.a";
            case SHADER_INPUT_3:
                return "vInput3.a";
            case SHADER_INPUT_4:
                return "vInput4.a";
            case SHADER_TEXEL0:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL0A:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL1A:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_TEXEL1:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_COMBINED:
                return "texel.a";
            case SHADER_NOISE:
                return RAND_NOISE;
        }
    }
    return "";
}

bool get_bool(prism::ContextTypes* value) {
    if (std::holds_alternative<int>(*value)) {
        return std::get<int>(*value) == 1;
    }
    return false;
}

prism::ContextTypes* append_formula(prism::ContextTypes* _, prism::ContextTypes* a_arg, prism::ContextTypes* a_single,
                                    prism::ContextTypes* a_mult, prism::ContextTypes* a_mix,
                                    prism::ContextTypes* a_with_alpha, prism::ContextTypes* a_only_alpha,
                                    prism::ContextTypes* a_alpha, prism::ContextTypes* a_first_cycle) {
    auto c = std::get<prism::MTDArray<int>>(*a_arg);
    bool do_single = get_bool(a_single);
    bool do_multiply = get_bool(a_mult);
    bool do_mix = get_bool(a_mix);
    bool with_alpha = get_bool(a_with_alpha);
    bool only_alpha = get_bool(a_only_alpha);
    bool opt_alpha = get_bool(a_alpha);
    bool first_cycle = get_bool(a_first_cycle);
    std::string out = "";
    if (do_single) {
        out += shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    } else if (do_multiply) {
        out += shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " * ";
        out += shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
    } else if (do_mix) {
        out += "mix(";
        out += shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += ")";
    } else {
        out += "(";
        out += shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " - ";
        out += shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ") * ";
        out += shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += " + ";
        out += shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    }
    return new prism::ContextTypes{ out };
}

std::optional<std::string> opengl_include_fs(Engine::ResourceManager& resources, const std::string& path) {
    auto init = std::make_shared<Engine::ResourceInitData>();
    init->Type = (uint32_t)Engine::ResourceType::Shader;
    init->ByteOrder = Engine::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = std::static_pointer_cast<Engine::Shader>(resources.LoadResource(path, init));
    if (res == nullptr) {
        return std::nullopt;
    }
    auto inc = static_cast<std::string*>(res->GetRawPointer());
    return *inc;
}

std::string GfxRenderingAPIOGL::BuildFsShader(const CCFeatures& cc_features) {
    const std::string* customShader = gfx_get_custom_shader_source(cc_features.shader_id);
    const bool waterShader = customShader && customShader->rfind("GAME_WATER_", 0) == 0;
    prism::Processor processor;
    prism::ContextItems mContext = {
        { "o_c", M_ARRAY(cc_features.c, int, 2, 2, 4) },
        { "o_alpha", cc_features.opt_alpha },
        { "o_fog", cc_features.opt_fog },
        { "o_texture_edge", cc_features.opt_texture_edge },
        { "o_noise", cc_features.opt_noise },
        { "o_2cyc", cc_features.opt_2cyc },
        { "o_alpha_threshold", cc_features.opt_alpha_threshold },
        { "o_invisible", cc_features.opt_invisible },
        { "o_grayscale", cc_features.opt_grayscale },
        { "o_water", waterShader },
        { "o_textures", M_ARRAY(cc_features.usedTextures, bool, 2) },
        { "o_masks", M_ARRAY(cc_features.used_masks, bool, 2) },
        { "o_blend", M_ARRAY(cc_features.used_blend, bool, 2) },
        { "o_clamp", M_ARRAY(cc_features.clamp, bool, 2, 2) },
        { "o_inputs", cc_features.numInputs },
        { "o_do_mix", M_ARRAY(cc_features.do_mix, bool, 2, 2) },
        { "o_do_single", M_ARRAY(cc_features.do_single, bool, 2, 2) },
        { "o_do_multiply", M_ARRAY(cc_features.do_multiply, bool, 2, 2) },
        { "o_color_alpha_same", M_ARRAY(cc_features.color_alpha_same, bool, 2) },
        { "FILTER_THREE_POINT", FILTER_THREE_POINT },
        { "FILTER_LINEAR", FILTER_LINEAR },
        { "FILTER_NONE", FILTER_NONE },
        { "srgb_mode", mSrgbMode },
        { "SHADER_0", SHADER_0 },
        { "SHADER_INPUT_1", SHADER_INPUT_1 },
        { "SHADER_INPUT_2", SHADER_INPUT_2 },
        { "SHADER_INPUT_3", SHADER_INPUT_3 },
        { "SHADER_INPUT_4", SHADER_INPUT_4 },
        { "SHADER_INPUT_5", SHADER_INPUT_5 },
        { "SHADER_INPUT_6", SHADER_INPUT_6 },
        { "SHADER_INPUT_7", SHADER_INPUT_7 },
        { "SHADER_TEXEL0", SHADER_TEXEL0 },
        { "SHADER_TEXEL0A", SHADER_TEXEL0A },
        { "SHADER_TEXEL1", SHADER_TEXEL1 },
        { "SHADER_TEXEL1A", SHADER_TEXEL1A },
        { "SHADER_1", SHADER_1 },
        { "SHADER_COMBINED", SHADER_COMBINED },
        { "SHADER_NOISE", SHADER_NOISE },
        { "o_three_point_filtering", mCurrentFilterMode == FILTER_THREE_POINT },
        { "append_formula", (InvokeFunc)append_formula },
#ifdef __APPLE__
        { "GLSL_VERSION", "#version 410 core" },
        { "attr", "in" },
        { "opengles", false },
        { "core_opengl", true },
        { "texture", "texture" },
        { "vOutColor", "vOutColor" },
#elif defined(USE_OPENGLES)
        { "GLSL_VERSION", "#version 300 es\nprecision mediump float;" },
        { "attr", "in" },
        { "opengles", true },
        { "core_opengl", false },
        { "texture", "texture" },
        { "vOutColor", "vOutColor" },
#else
        { "GLSL_VERSION", "#version 130" },
        { "attr", "varying" },
        { "opengles", false },
        { "core_opengl", false },
        { "texture", "texture2D" },
        { "vOutColor", "gl_FragColor" },
#endif
    };
    processor.populate(mContext);
    auto init = std::make_shared<Engine::ResourceInitData>();
    init->Type = (uint32_t)Engine::ResourceType::Shader;
    init->ByteOrder = Engine::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = std::static_pointer_cast<Engine::Shader>(
        mResources.LoadResource("shaders/opengl/default.shader.fs", init));

    if (res == nullptr) {
        WriteLog("Failed to load default fragment shader, missing f3d.o2r?");
        abort();
    }

    auto shader = static_cast<std::string*>(res->GetRawPointer());
    processor.load(*shader);
    processor.bind_include_loader(
        [this](const std::string& path) { return opengl_include_fs(mResources, path); });
    auto result = processor.process();
    // WriteLog("=========== FRAGMENT SHADER ============");
    // WriteLog(result);
    // WriteLog("========================================");
    return result;
}

static size_t numFloats = 0;

static prism::ContextTypes* UpdateFloats(prism::ContextTypes* _, prism::ContextTypes* num) {
    numFloats += std::get<int>(*num);
    return nullptr;
}

static std::string BuildVsShader(const CCFeatures& cc_features, Engine::ResourceManager& resources) {
    numFloats = 4;
    const std::string* customShader = gfx_get_custom_shader_source(cc_features.shader_id);
    const bool waterShader = customShader && customShader->rfind("GAME_WATER_", 0) == 0;
    prism::Processor processor;
    prism::ContextItems mContext = { { "o_textures", M_ARRAY(cc_features.usedTextures, bool, 2) },
                                     { "o_water", waterShader },
                                     { "o_clamp", M_ARRAY(cc_features.clamp, bool, 2, 2) },
                                     { "o_fog", cc_features.opt_fog },
                                     { "o_grayscale", cc_features.opt_grayscale },
                                     { "o_alpha", cc_features.opt_alpha },
                                     { "o_inputs", cc_features.numInputs },
                                     { "update_floats", (InvokeFunc)UpdateFloats },
#ifdef __APPLE__
                                     { "GLSL_VERSION", "#version 410 core" },
                                     { "attr", "in" },
                                     { "out", "out" },
                                     { "opengles", false }
#elif defined(USE_OPENGLES)
                                     { "GLSL_VERSION", "#version 300 es" },
                                     { "attr", "in" },
                                     { "out", "out" },
                                     { "opengles", true }
#else
                                     { "GLSL_VERSION", "#version 110" },
                                     { "attr", "attribute" },
                                     { "out", "varying" },
                                     { "opengles", false }
#endif
    };
    processor.populate(mContext);

    auto init = std::make_shared<Engine::ResourceInitData>();
    init->Type = (uint32_t)Engine::ResourceType::Shader;
    init->ByteOrder = Engine::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = std::static_pointer_cast<Engine::Shader>(
        resources.LoadResource("shaders/opengl/default.shader.vs", init));

    if (res == nullptr) {
        WriteLog("Failed to load default vertex shader, missing f3d.o2r?");
        abort();
    }

    auto shader = static_cast<std::string*>(res->GetRawPointer());
    processor.load(*shader);
    processor.bind_include_loader(
        [&resources](const std::string& path) { return opengl_include_fs(resources, path); });
    auto result = processor.process();
    // WriteLog("=========== VERTEX SHADER ============");
    // WriteLog(result);
    // WriteLog("========================================");
    return result;
}

ShaderProgram* GfxRenderingAPIOGL::CreateAndLoadNewShader(uint64_t shader_id0, uint32_t shader_id1) {
    CCFeatures cc_features;
    gfx_cc_get_features(shader_id0, shader_id1, &cc_features);
    const auto fs_buf = BuildFsShader(cc_features);
    const auto vs_buf = BuildVsShader(cc_features, mResources);
    const GLchar* sources[2] = { vs_buf.data(), fs_buf.data() };
    const GLint lengths[2] = { (GLint)vs_buf.size(), (GLint)fs_buf.size() };
    GLint success;

    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &sources[0], &lengths[0]);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint max_length = 0;
        glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &max_length);
        char error_log[1024];
        // fprintf(stderr, "Vertex shader compilation failed\n");
        glGetShaderInfoLog(vertex_shader, max_length, &max_length, &error_log[0]);
        // fprintf(stderr, "%s\n", &error_log[0]);
        abort();
    }

    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &sources[1], &lengths[1]);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint max_length = 0;
        glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &max_length);
        char error_log[1024];
        fprintf(stderr, "Fragment shader compilation failed\n");
        glGetShaderInfoLog(fragment_shader, max_length, &max_length, &error_log[0]);
        fprintf(stderr, "%s\n", &error_log[0]);
        abort();
    }

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    size_t cnt = 0;

    struct ShaderProgram* prg = &mShaderProgramPool[std::make_pair(shader_id0, shader_id1)];
    prg->attribLocations[cnt] = glGetAttribLocation(shader_program, "aVtxPos");
    prg->attribSizes[cnt] = 4;
    ++cnt;

    for (int i = 0; i < 2; i++) {
        if (cc_features.usedTextures[i]) {
            char name[32];
            sprintf(name, "aTexCoord%d", i);
            prg->attribLocations[cnt] = glGetAttribLocation(shader_program, name);
            prg->attribSizes[cnt] = 2;
            ++cnt;

            for (int j = 0; j < 2; j++) {
                if (cc_features.clamp[i][j]) {
                    sprintf(name, "aTexClamp%s%d", j == 0 ? "S" : "T", i);
                    prg->attribLocations[cnt] = glGetAttribLocation(shader_program, name);
                    prg->attribSizes[cnt] = 1;
                    ++cnt;
                }
            }
        }
    }

    if (cc_features.opt_fog) {
        prg->attribLocations[cnt] = glGetAttribLocation(shader_program, "aFog");
        prg->attribSizes[cnt] = 4;
        ++cnt;
    }

    if (cc_features.opt_grayscale) {
        prg->attribLocations[cnt] = glGetAttribLocation(shader_program, "aGrayscaleColor");
        prg->attribSizes[cnt] = 4;
        ++cnt;
    }

    for (int i = 0; i < cc_features.numInputs; i++) {
        char name[16];
        sprintf(name, "aInput%d", i + 1);
        prg->attribLocations[cnt] = glGetAttribLocation(shader_program, name);
        prg->attribSizes[cnt] = cc_features.opt_alpha ? 4 : 3;
        ++cnt;
    }

    prg->openglProgramId = shader_program;
    prg->numInputs = cc_features.numInputs;
    prg->usedTextures[0] = cc_features.usedTextures[0];
    prg->usedTextures[1] = cc_features.usedTextures[1];
    prg->usedTextures[2] = cc_features.used_masks[0];
    prg->usedTextures[3] = cc_features.used_masks[1];
    prg->usedTextures[4] = cc_features.used_blend[0];
    prg->usedTextures[5] = cc_features.used_blend[1];
    prg->numFloats = numFloats;
    prg->numAttribs = cnt;

    prg->frameCountLocation = glGetUniformLocation(shader_program, "frame_count");
    prg->noiseScaleLocation = glGetUniformLocation(shader_program, "noise_scale");
    prg->texture_width_location = glGetUniformLocation(shader_program, "texture_width");
    prg->texture_height_location = glGetUniformLocation(shader_program, "texture_height");
    prg->texture_filtering_location = glGetUniformLocation(shader_program, "texture_filtering");
    const std::string* customShader = gfx_get_custom_shader_source(cc_features.shader_id);
    prg->isWater = customShader && customShader->rfind("GAME_WATER_", 0) == 0;
    prg->waterLevel = prg->isWater && !customShader->empty() ? customShader->back() - '0' : -1;
    prg->waterNormalLocation = glGetUniformLocation(shader_program, "uWaterNormal");
    prg->waterSceneLocation = glGetUniformLocation(shader_program, "uWaterScene");
    prg->waterScreenSizeLocation = glGetUniformLocation(shader_program, "uWaterScreenSize");
    prg->waterTimeLocation = glGetUniformLocation(shader_program, "uWaterTime");

    LoadShader(prg);

    if (cc_features.usedTextures[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTex0");
        glUniform1i(sampler_location, 0);
    }
    if (cc_features.usedTextures[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTex1");
        glUniform1i(sampler_location, 1);
    }
    if (cc_features.used_masks[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexMask0");
        glUniform1i(sampler_location, 2);
    }
    if (cc_features.used_masks[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexMask1");
        glUniform1i(sampler_location, 3);
    }
    if (cc_features.used_blend[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexBlend0");
        glUniform1i(sampler_location, 4);
    }
    if (cc_features.used_blend[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexBlend1");
        glUniform1i(sampler_location, 5);
    }

    return prg;
}

struct ShaderProgram* GfxRenderingAPIOGL::LookupShader(uint64_t shader_id0, uint32_t shader_id1) {
    auto it = mShaderProgramPool.find(std::make_pair(shader_id0, shader_id1));
    return it == mShaderProgramPool.end() ? nullptr : &it->second;
}

void GfxRenderingAPIOGL::ShaderGetInfo(struct ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) {
    *numInputs = prg->numInputs;
    usedTextures[0] = prg->usedTextures[0];
    usedTextures[1] = prg->usedTextures[1];
}

GLuint GfxRenderingAPIOGL::NewTexture() {
    GLuint ret;
    glGenTextures(1, &ret);
    return ret;
}

void GfxRenderingAPIOGL::DeleteTexture(uint32_t texID) {
    glDeleteTextures(1, &texID);
}

void GfxRenderingAPIOGL::SelectTexture(int tile, GLuint texture_id) {
    glActiveTexture(GL_TEXTURE0 + tile);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    mCurrentTextureIds[tile] = texture_id;
    mCurrentTile = tile;
}

void GfxRenderingAPIOGL::UploadTexture(const uint8_t* rgba32_buf, uint32_t width, uint32_t height) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba32_buf);
    textures[mCurrentTextureIds[mCurrentTile]].width = width;
    textures[mCurrentTextureIds[mCurrentTile]].height = height;
}

#ifdef USE_OPENGLES
#define GL_MIRROR_CLAMP_TO_EDGE 0x8743
#endif

static uint32_t gfx_cm_to_opengl(uint32_t val) {
    switch (val) {
        case G_TX_NOMIRROR | G_TX_CLAMP:
            return GL_CLAMP_TO_EDGE;
        case G_TX_MIRROR | G_TX_WRAP:
            return GL_MIRRORED_REPEAT;
        case G_TX_MIRROR | G_TX_CLAMP:
            return GL_MIRROR_CLAMP_TO_EDGE;
        case G_TX_NOMIRROR | G_TX_WRAP:
            return GL_REPEAT;
    }
    return 0;
}

void GfxRenderingAPIOGL::SetSamplerParameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
    glActiveTexture(GL_TEXTURE0 + tile);
    const GLint filter = linear_filter && mCurrentFilterMode == FILTER_LINEAR ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    textures[mCurrentTextureIds[tile]].filtering = !linear_filter ? FILTER_LINEAR : FILTER_THREE_POINT;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gfx_cm_to_opengl(cms));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gfx_cm_to_opengl(cmt));
}

void GfxRenderingAPIOGL::SetDepthTestAndMask(bool depth_test, bool z_upd) {
    mCurrentDepthTest = depth_test;
    mCurrentDepthMask = z_upd;
}

void GfxRenderingAPIOGL::SetZmodeDecal(bool zmode_decal) {
    mCurrentZmodeDecal = zmode_decal;
}

void GfxRenderingAPIOGL::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void GfxRenderingAPIOGL::SetScissor(int x, int y, int width, int height) {
    glScissor(x, y, width, height);
}

void GfxRenderingAPIOGL::SetUseAlpha(bool use_alpha) {
    if (use_alpha) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
}

void GfxRenderingAPIOGL::DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    const bool drawingWater = mCurrentShaderProgram != nullptr && mCurrentShaderProgram->isWater;
    if (drawingWater) {
        // Match game's dedicated water pass. Fast3D normally performs
        // culling on the CPU, but water must also survive any leaked GL state.
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    if (mCurrentDepthTest != mLastDepthTest || mCurrentDepthMask != mLastDepthMask) {
        mLastDepthTest = mCurrentDepthTest;
        mLastDepthMask = mCurrentDepthMask;

        if (mCurrentDepthTest || mLastDepthMask) {
            glEnable(GL_DEPTH_TEST);
            glDepthMask(mLastDepthMask ? GL_TRUE : GL_FALSE);
            glDepthFunc(mCurrentDepthTest ? (mCurrentZmodeDecal ? GL_LEQUAL : GL_LESS) : GL_ALWAYS);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }

    if (mCurrentZmodeDecal != mLastZmodeDecal) {
        mLastZmodeDecal = mCurrentZmodeDecal;
        if (mCurrentZmodeDecal) {
            // SSDB = SlopeScaledDepthBias 120 leads to -2 at 240p which is the same as N64 mode which has very little
            // fighting
            const int n64modeFactor = 120;
            const int noVanishFactor = 100;
            GLfloat SSDB = -2;
            switch (mVariables.GetInteger(CVAR_Z_FIGHTING_MODE, 0)) {
                // scaled z-fighting (N64 mode like)
                case 1:
                    if (mFrameBuffers.size() >
                        mCurrentFrameBuffer) { // safety check for vector size can probably be removed
                        SSDB = -1.0f * (GLfloat)mFrameBuffers[mCurrentFrameBuffer].height / n64modeFactor;
                    }
                    break;
                // no vanishing paths
                case 2:
                    if (mFrameBuffers.size() >
                        mCurrentFrameBuffer) { // safety check for vector size can probably be removed
                        SSDB = -1.0f * (GLfloat)mFrameBuffers[mCurrentFrameBuffer].height / noVanishFactor;
                    }
                    break;
                // disabled
                case 0:
                default:
                    SSDB = -2;
            }
            glPolygonOffset(SSDB, -2);
            glEnable(GL_POLYGON_OFFSET_FILL);
        } else {
            glPolygonOffset(0, 0);
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    SetPerDrawUniforms();

    // printf("flushing %d tris\n", buf_vbo_num_tris);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * buf_vbo_len, buf_vbo, GL_STREAM_DRAW);
    if (drawingWater && mWaterResourcesReady && mWaterRenderProgram != 0 && mWaterSceneTexture != 0) {
        // Fast3D has already transformed the OOT water-box vertices to clip
        // coordinates. Render those exact triangles with game's
        // dedicated surface program; collision and swimming remain entirely
        // driven by OOT's original WaterBox data.
        glUseProgram(mWaterRenderProgram);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, mWaterSimulationTextures[mWaterTextureIndex]);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, mWaterSceneTexture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mActiveWaterMaskTexture);
        glUniform1i(glGetUniformLocation(mWaterRenderProgram, "sNormal"), 6);
        glUniform1i(glGetUniformLocation(mWaterRenderProgram, "sScene"), 7);
        glUniform1i(glGetUniformLocation(mWaterRenderProgram, "sMask"), 0);
        glUniform2f(glGetUniformLocation(mWaterRenderProgram, "uScreenSize"),
                    static_cast<float>(mWaterSceneWidth), static_cast<float>(mWaterSceneHeight));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE,
                              static_cast<GLsizei>(mCurrentShaderProgram->numFloats * sizeof(float)), nullptr);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                              static_cast<GLsizei>(mCurrentShaderProgram->numFloats * sizeof(float)),
                              reinterpret_cast<void*>(4 * sizeof(float)));
        // The removed room surface was coplanar with the WaterBox plane.
        // LEQUAL also keeps the replacement stable against adjoining pool
        // geometry without allowing it through nearer opaque walls.
        glDepthFunc(GL_LEQUAL);
        glDrawArrays(GL_TRIANGLES, 0, 3 * buf_vbo_num_tris);
        glDepthFunc(mCurrentDepthTest ? (mCurrentZmodeDecal ? GL_LEQUAL : GL_LESS) : GL_ALWAYS);

        // Restore the generated program and its attribute layout for the next
        // Fast3D batch.
        glUseProgram(mCurrentShaderProgram->openglProgramId);
        VertexArraySetAttribs(mCurrentShaderProgram);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, 3 * buf_vbo_num_tris);
    }
}

void GfxRenderingAPIOGL::Init() {
    if (!InitializeOpenGLExtensions()) {
        throw std::runtime_error("Required OpenGL functions are unavailable");
    }

    glGenBuffers(1, &mOpenglVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mOpenglVbo);

#if defined(__APPLE__) || defined(USE_OPENGLES)
    glGenVertexArrays(1, &mOpenglVao);
    glBindVertexArray(mOpenglVao);
#endif

#ifndef USE_OPENGLES // not supported on gles
    glEnable(GL_DEPTH_CLAMP);
#endif
    glDepthFunc(GL_LEQUAL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mFrameBuffers.resize(1); // for the default screen buffer

    glGenRenderbuffers(1, &mPixelDepthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, mPixelDepthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &mPixelDepthFb);
    glBindFramebuffer(GL_FRAMEBUFFER, mPixelDepthFb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mPixelDepthRb);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    mPixelDepthRbSize = 1;

    glGetIntegerv(GL_MAX_SAMPLES, &mMaxMsaaLevel);
}

void GfxRenderingAPIOGL::Shutdown() {
    DestroyWaterResources();
    for (auto& programEntry : mShaderProgramPool) {
        auto& program = programEntry.second;
        if (program.openglProgramId) {
            glDeleteProgram(program.openglProgramId);
            program.openglProgramId = 0;
        }
    }
    mShaderProgramPool.clear();
    for (FramebufferOGL& framebuffer : mFrameBuffers) {
        if (framebuffer.fbo) glDeleteFramebuffers(1, &framebuffer.fbo);
        if (framebuffer.clrbuf) glDeleteTextures(1, &framebuffer.clrbuf);
        if (framebuffer.clrbufMsaa) glDeleteRenderbuffers(1, &framebuffer.clrbufMsaa);
        if (framebuffer.rbo) glDeleteRenderbuffers(1, &framebuffer.rbo);
        framebuffer = {};
    }
    mFrameBuffers.clear();
    if (mPixelDepthFb) glDeleteFramebuffers(1, &mPixelDepthFb);
    if (mPixelDepthRb) glDeleteRenderbuffers(1, &mPixelDepthRb);
    if (mOpenglVbo) glDeleteBuffers(1, &mOpenglVbo);
    mPixelDepthFb = mPixelDepthRb = mOpenglVbo = 0;
}

void GfxRenderingAPIOGL::OnResize() {
}

void GfxRenderingAPIOGL::StartFrame() {
    mFrameCount++;
}

void GfxRenderingAPIOGL::EndFrame() {
    glFlush();
}

void GfxRenderingAPIOGL::FinishRender() {
}

int GfxRenderingAPIOGL::CreateFramebuffer() {
    GLuint clrbuf;
    glGenTextures(1, &clrbuf);
    glBindTexture(GL_TEXTURE_2D, clrbuf);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint clrbufMsaa;
    glGenRenderbuffers(1, &clrbufMsaa);

    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);

    size_t i = mFrameBuffers.size();
    mFrameBuffers.resize(i + 1);

    mFrameBuffers[i].fbo = fbo;
    mFrameBuffers[i].clrbuf = clrbuf;
    mFrameBuffers[i].clrbufMsaa = clrbufMsaa;
    mFrameBuffers[i].rbo = rbo;

    return i;
}

void GfxRenderingAPIOGL::UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                                     bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                                     bool can_extract_depth) {
    FramebufferOGL& fb = mFrameBuffers[fb_id];

    width = std::max(width, 1U);
    height = std::max(height, 1U);
    msaa_level = std::min(msaa_level, (uint32_t)mMaxMsaaLevel);

    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);

    if (fb_id != 0) {
        if (fb.width != width || fb.height != height || fb.msaa_level != msaa_level) {
            if (msaa_level <= 1) {
                glBindTexture(GL_TEXTURE_2D, fb.clrbuf);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
                glBindTexture(GL_TEXTURE_2D, 0);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.clrbuf, 0);
            } else {
                glBindRenderbuffer(GL_RENDERBUFFER, fb.clrbufMsaa);
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa_level, GL_RGB8, width, height);
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fb.clrbufMsaa);
            }
        }

        if (has_depth_buffer &&
            (fb.width != width || fb.height != height || fb.msaa_level != msaa_level || !fb.has_depth_buffer)) {
            glBindRenderbuffer(GL_RENDERBUFFER, fb.rbo);
            if (msaa_level <= 1) {
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            } else {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa_level, GL_DEPTH24_STENCIL8, width, height);
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        if (!fb.has_depth_buffer && has_depth_buffer) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb.rbo);
        } else if (fb.has_depth_buffer && !has_depth_buffer) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
        }
    }

    fb.width = width;
    fb.height = height;
    fb.has_depth_buffer = has_depth_buffer;
    fb.msaa_level = msaa_level;
    fb.invertY = opengl_invertY;
}

void GfxRenderingAPIOGL::StartDrawToFramebuffer(int fb_id, float noise_scale) {
    FramebufferOGL& fb = mFrameBuffers[fb_id];

    if (noise_scale != 0.0f) {
        mCurrentNoiseScale = 1.0f / noise_scale;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    mCurrentFrameBuffer = fb_id;
}

void GfxRenderingAPIOGL::ClearFramebuffer(bool color, bool depth) {
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear((color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0));
    glDepthMask(mCurrentDepthMask ? GL_TRUE : GL_FALSE);
    glEnable(GL_SCISSOR_TEST);
}

void GfxRenderingAPIOGL::ResolveMSAAColorBuffer(int fb_id_target, int fb_id_source) {
    FramebufferOGL& fb_dst = mFrameBuffers[fb_id_target];
    FramebufferOGL& fb_src = mFrameBuffers[fb_id_source];
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_dst.fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb_src.fbo);

    // Disabled for blit
    glDisable(GL_SCISSOR_TEST);

    glBlitFramebuffer(0, 0, fb_src.width, fb_src.height, 0, 0, fb_dst.width, fb_dst.height, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, mCurrentFrameBuffer);

    glEnable(GL_SCISSOR_TEST);
}

void* GfxRenderingAPIOGL::GetFramebufferTextureId(int fb_id) {
    return (void*)(uintptr_t)mFrameBuffers[fb_id].clrbuf;
}

void GfxRenderingAPIOGL::SelectTextureFb(int fb_id) {
    // glDisable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, mFrameBuffers[fb_id].clrbuf);
}

void GfxRenderingAPIOGL::CopyFramebuffer(int fb_dst_id, int fb_src_id, int srcX0, int srcY0, int srcX1, int srcY1,
                                         int dstX0, int dstY0, int dstX1, int dstY1) {
    if (fb_dst_id >= (int)mFrameBuffers.size() || fb_src_id >= (int)mFrameBuffers.size()) {
        return;
    }

    FramebufferOGL src = mFrameBuffers[fb_src_id];
    const FramebufferOGL& dst = mFrameBuffers[fb_dst_id];

    // Adjust y values for non-inverted source frame buffers because opengl uses bottom left for origin
    if (!src.invertY) {
        int temp = srcY1 - srcY0;
        srcY1 = src.height - srcY0;
        srcY0 = srcY1 - temp;
    }

    // Flip the y values
    if (src.invertY != dst.invertY) {
        std::swap(srcY0, srcY1);
    }

    // Disabled for blit
    glDisable(GL_SCISSOR_TEST);

    // For msaa enabled buffers we can't perform a scaled blit to a simple sample buffer
    // First do an unscaled blit to a msaa resolved buffer
    if (src.height != dst.height && src.width != dst.width && src.msaa_level > 1) {
        // Start with the main buffer (0) as the msaa resolved buffer
        int fb_resolve_id = 0;
        FramebufferOGL fb_resolve = mFrameBuffers[fb_resolve_id];

        // If the size doesn't match our source, then we need to use our separate color msaa resolved buffer (2)
        if (fb_resolve.height != src.height || fb_resolve.width != src.width) {
            fb_resolve_id = 2;
            fb_resolve = mFrameBuffers[fb_resolve_id];
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_resolve.fbo);

        glBlitFramebuffer(0, 0, src.width, src.height, 0, 0, src.width, src.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // Switch source buffer to the resolved sample
        fb_src_id = fb_resolve_id;
        src = fb_resolve;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst.fbo);

    // The 0 buffer is double-buffered, so read from the back buffer before presentation.
    if (fb_src_id == 0) {
        glReadBuffer(GL_BACK);
    } else {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }

    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[mCurrentFrameBuffer].fbo);

    glReadBuffer(GL_BACK);

    glEnable(GL_SCISSOR_TEST);
}

void GfxRenderingAPIOGL::ReadFramebufferToCPU(int fb_id, uint32_t width, uint32_t height, uint16_t* rgba16_buf) {
    if (fb_id >= (int)mFrameBuffers.size()) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[fb_id].fbo);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, (void*)rgba16_buf);
    glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffers[mCurrentFrameBuffer].fbo);
}

std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
GfxRenderingAPIOGL::GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) {
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res;

    FramebufferOGL& fb = mFrameBuffers[fb_id];

    // When looking up one value and the framebuffer is single-sampled, we can read pixels directly
    // Otherwise we need to blit first to a new buffer then read it
    if (coordinates.size() == 1 && fb.msaa_level <= 1) {
        uint32_t depth_stencil_value;
        glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
        int x = coordinates.begin()->first;
        int y = coordinates.begin()->second;
#ifndef USE_OPENGLES // not supported on gles. Runs fine without it, but this may cause issues
        glReadPixels(x, fb.invertY ? fb.height - y : y, 1, 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
                     &depth_stencil_value);
#endif
        res.emplace(*coordinates.begin(), (depth_stencil_value >> 18) << 2);
    } else {
        if (mPixelDepthRbSize < coordinates.size()) {
            // Resizing a renderbuffer seems broken with Intel's driver, so recreate one instead.
            glBindFramebuffer(GL_FRAMEBUFFER, mPixelDepthFb);
            glDeleteRenderbuffers(1, &mPixelDepthRb);
            glGenRenderbuffers(1, &mPixelDepthRb);
            glBindRenderbuffer(GL_RENDERBUFFER, mPixelDepthRb);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, coordinates.size(), 1);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mPixelDepthRb);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);

            mPixelDepthRbSize = coordinates.size();
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mPixelDepthFb);

        glDisable(GL_SCISSOR_TEST); // needed for the blit operation

        {
            size_t i = 0;
            for (const auto& coord : coordinates) {
                int x = coord.first;
                int y = coord.second;
                if (fb.invertY) {
                    y = fb.height - y;
                }
                glBlitFramebuffer(x, y, x + 1, y + 1, i, 0, i + 1, 1, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                                  GL_NEAREST);
                ++i;
            }
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, mPixelDepthFb);
        std::vector<uint32_t> depth_stencil_values(coordinates.size());
#ifndef USE_OPENGLES // not supported on gles. Runs fine without it, but this may cause issues
        glReadPixels(0, 0, coordinates.size(), 1, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, depth_stencil_values.data());
#endif
        {
            size_t i = 0;
            for (const auto& coord : coordinates) {
                res.emplace(coord, (depth_stencil_values[i++] >> 18) << 2);
            }
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mCurrentFrameBuffer);

    return res;
}

void GfxRenderingAPIOGL::SetTextureFilter(FilteringMode mode) {
    gfx_texture_cache_clear();
    mCurrentFilterMode = mode;
}

FilteringMode GfxRenderingAPIOGL::GetTextureFilter() {
    return mCurrentFilterMode;
}

void GfxRenderingAPIOGL::SetSrgbMode() {
    mSrgbMode = true;
}

} // namespace Engine::Rendering
#endif

#pragma clang diagnostic pop
