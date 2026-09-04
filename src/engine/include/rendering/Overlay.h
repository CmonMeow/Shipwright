#pragma once

#include <cstdint>

namespace Engine::Rendering::Overlay {

struct NetworkTelemetry {
    bool active = false;
    int32_t latencyMilliseconds = 0;
    int32_t inboundBytesPerSecond = 0;
    int32_t outboundBytesPerSecond = 0;
};

void BeginFrame();
void QueueRect(float minX, float minY, float maxX, float maxY, float red, float green, float blue, float alpha,
               bool outline = false);
void QueueText(const char* text, float x, float y, float red = 0.86f, float green = 0.90f, float blue = 0.88f,
               float alpha = 1.0f);
void QueueGameText(const char* text, float x, float y, float red, float green, float blue, float alpha);
void QueueCenteredGameText(const char* text, float x, float y, float red, float green, float blue, float alpha);
void Render(uint32_t width, uint32_t height);
void Clear();
void SetNetworkTelemetry(bool active, int32_t latencyMilliseconds, int32_t inboundBytesPerSecond,
                         int32_t outboundBytesPerSecond);
NetworkTelemetry GetNetworkTelemetry();
} // namespace Engine::Rendering::Overlay
