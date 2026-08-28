#ifdef _WIN32

#include "engine/window/Overlay.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "fast/backends/OpenGLExtensions.h"

#include "engine/window/OverlayFontData.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Engine::Overlay {
namespace {

enum class CommandType { Rectangle, Text };

struct Command {
    CommandType type = CommandType::Rectangle;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float red = 1.0f;
    float green = 1.0f;
    float blue = 1.0f;
    float alpha = 1.0f;
    bool outline = false;
    bool gameCoordinates = false;
    bool centered = false;
    std::string text;
};

std::mutex gCommandMutex;
std::vector<Command> gCommands;
std::atomic_bool gNetworkActive = false;
std::atomic_int32_t gNetworkLatencyMilliseconds = 0;
std::atomic_int32_t gNetworkInboundBytesPerSecond = 0;
std::atomic_int32_t gNetworkOutboundBytesPerSecond = 0;
std::atomic<void (*)()> gMoveLoopCallback = nullptr;

void DrawText(const Command& command, uint32_t width, uint32_t height) {
    float x = command.gameCoordinates ? command.minX * static_cast<float>(width) / 320.0f : command.minX;
    const float y = command.gameCoordinates
                        ? static_cast<float>(height) - command.minY * static_cast<float>(height) / 240.0f - 16.0f
                        : command.minY;
    if (command.centered) {
        float textWidth = 0.0f;
        for (const unsigned char character : command.text) {
            textWidth += character == '\t' ? 18.0f : character == ' ' ? 6.0f : 12.0f;
        }
        x -= textWidth * 0.5f;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glColor4f(0.12f, 0.12f, 0.12f, command.alpha);
    glRasterPos2f(std::max(0.0f, x + 2.0f), std::max(2.0f, y - 2.0f));
    for (const unsigned char character : command.text) {
        const uint8_t* glyph = character >= 33 && character <= 127
                                   ? OverlayFont::Serif[character - 32]
                                   : OverlayFont::Serif[0];
        const float advance = character == '\t' ? 18.0f : character == ' ' ? 6.0f : 12.0f;
        glBitmap(16, 16, 0.0f, 0.0f, advance, 0.0f, glyph);
    }

    glColor4f(command.red, command.green, command.blue, command.alpha);
    glRasterPos2f(std::max(0.0f, x), std::max(2.0f, y));
    for (const unsigned char character : command.text) {
        const uint8_t* glyph = character >= 33 && character <= 127
                                   ? OverlayFont::Serif[character - 32]
                                   : OverlayFont::Serif[0];
        const float advance = character == '\t' ? 18.0f : character == ' ' ? 6.0f : 12.0f;
        glBitmap(16, 16, 0.0f, 0.0f, advance, 0.0f, glyph);
    }
}

void DrawRectangle(const Command& command) {
    glColor4f(command.red, command.green, command.blue, command.alpha);
    glBegin(command.outline ? GL_LINE_LOOP : GL_QUADS);
    glVertex2f(command.minX, command.minY);
    glVertex2f(command.maxX, command.minY);
    glVertex2f(command.maxX, command.maxY);
    glVertex2f(command.minX, command.maxY);
    glEnd();
}

} // namespace

void BeginFrame() {
    std::lock_guard lock(gCommandMutex);
    gCommands.clear();
}

void QueueRect(float minX, float minY, float maxX, float maxY, float red, float green, float blue, float alpha,
               bool outline) {
    std::lock_guard lock(gCommandMutex);
    gCommands.push_back(
        { CommandType::Rectangle, minX, minY, maxX, maxY, red, green, blue, alpha, outline, false, false, {} });
}

void QueueText(const char* text, float x, float y, float red, float green, float blue, float alpha) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    std::lock_guard lock(gCommandMutex);
    gCommands.push_back({ CommandType::Text, x, y, 0.0f, 0.0f, red, green, blue, alpha, false, false, false, text });
}

void QueueGameText(const char* text, float x, float y, float red, float green, float blue, float alpha) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    std::lock_guard lock(gCommandMutex);
    gCommands.push_back({ CommandType::Text, x, y, 0.0f, 0.0f, red, green, blue, alpha, false, true, false, text });
}

void QueueCenteredGameText(const char* text, float x, float y, float red, float green, float blue, float alpha) {
    if (text == nullptr || text[0] == '\0') {
        return;
    }
    std::lock_guard lock(gCommandMutex);
    gCommands.push_back({ CommandType::Text, x, y, 0.0f, 0.0f, red, green, blue, alpha, false, true, true, text });
}

void Render(uint32_t width, uint32_t height) {
    std::vector<Command> commands;
    {
        std::lock_guard lock(gCommandMutex);
        // Fast3D may present multiple interpolated frames for one game update.
        // Keep the latest UI command list until BeginFrame replaces it; consuming
        // it here made chat appear on only one of those presentations and strobe.
        commands = gCommands;
    }
    if (commands.empty() || width == 0 || height == 0) {
        return;
    }

    GLint previousProgram = 0;
    GLint previousDrawFramebuffer = 0;
    GLint previousReadFramebuffer = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);

    // The game renderer leaves its GLSL program and, in some framebuffer
    // paths, an off-screen target bound. Fixed-function glBitmap/glBegin does
    // not produce visible output through that shader. Draw the Game UI
    // directly into the window back buffer with the compatibility pipeline.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(width), 0.0, static_cast<double>(height), -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    for (const Command& command : commands) {
        if (command.type == CommandType::Text) {
            DrawText(command, width, height);
        } else {
            DrawRectangle(command);
        }
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
    glUseProgram(static_cast<GLuint>(previousProgram));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
}

void Clear() {
    std::lock_guard lock(gCommandMutex);
    gCommands.clear();
}

void SetNetworkTelemetry(bool active, int32_t latencyMilliseconds, int32_t inboundBytesPerSecond,
                         int32_t outboundBytesPerSecond) {
    gNetworkActive.store(active, std::memory_order_relaxed);
    gNetworkLatencyMilliseconds.store(std::max(0, latencyMilliseconds), std::memory_order_relaxed);
    gNetworkInboundBytesPerSecond.store(std::max(0, inboundBytesPerSecond), std::memory_order_relaxed);
    gNetworkOutboundBytesPerSecond.store(std::max(0, outboundBytesPerSecond), std::memory_order_relaxed);
}

NetworkTelemetry GetNetworkTelemetry() {
    return { gNetworkActive.load(std::memory_order_relaxed),
             gNetworkLatencyMilliseconds.load(std::memory_order_relaxed),
             gNetworkInboundBytesPerSecond.load(std::memory_order_relaxed),
             gNetworkOutboundBytesPerSecond.load(std::memory_order_relaxed) };
}

void SetMoveLoopCallback(void (*callback)()) {
    gMoveLoopCallback.store(callback, std::memory_order_release);
}

void PumpMoveLoop() {
    if (const auto callback = gMoveLoopCallback.load(std::memory_order_acquire)) {
        callback();
    }
}

} // namespace Engine::Overlay

extern "C" void Overlay_QueueGameText(const char* text, float x, float y, float red, float green,
                                                  float blue, float alpha) {
    Engine::Overlay::QueueGameText(text, x, y, red, green, blue, alpha);
}

extern "C" const uint8_t* OverlayFont_GetSerifGlyph(unsigned char character) {
    return character >= 33 && character <= 127 ? Engine::OverlayFont::Serif[character - 32]
                                                : Engine::OverlayFont::Serif[0];
}

#endif
