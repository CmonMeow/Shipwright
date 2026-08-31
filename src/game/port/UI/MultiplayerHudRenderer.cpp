#include "MultiplayerHudRenderer.h"

#include <engine/window/Overlay.h>
#include <runtime/bridge/windowbridge.h>

#include <algorithm>

namespace Game::UI {

void MultiplayerHudRenderer::Draw(const MultiplayerHudView& view) {
    DrawChat(view);
    DrawLife(view);
}

void MultiplayerHudRenderer::Reset() {
    mDraftViewStart = 0;
}

void MultiplayerHudRenderer::DrawChat(const MultiplayerHudView& view) {
    if (!view.inputActive && !view.passiveChatEnabled &&
        view.gameplayNotice.empty()) {
        mDraftViewStart = 0;
        return;
    }

    const float windowWidth = static_cast<float>(WindowGetWidth());
    const float width =
        std::max(120.0f, std::min(680.0f, windowWidth - 24.0f));
    constexpr float x = 12.0f;
    constexpr float y = 14.0f;
    constexpr float lineHeight = 18.0f;
    constexpr float inputHeight = 24.0f;
    const float height = static_cast<float>(view.visibleHistoryRows) *
                             lineHeight +
                         inputHeight + 18.0f;

    Engine::Overlay::QueueRect(
        x, y, x + width, y + height, 0.02f, 0.025f, 0.025f, 0.70f);
    Engine::Overlay::QueueRect(
        x, y, x + width, y + height, 0.32f, 0.38f, 0.36f, 0.95f, true);

    const size_t newest = view.history.size() > view.historyScrollOffset
                              ? view.history.size() - view.historyScrollOffset
                              : 0;
    const size_t first = newest > view.visibleHistoryRows
                             ? newest - view.visibleHistoryRows
                             : 0;
    const size_t visibleCharacters = width > 24.0f
                                         ? static_cast<size_t>(
                                               (width - 20.0f) / 12.0f)
                                         : 1;
    const float textTop = y + height - 22.0f;
    for (size_t index = first; index < newest; ++index) {
        float red = 0.96f;
        float green = 0.96f;
        float blue = 0.96f;
        if (view.history[index].kind == MultiplayerHudLineKind::System) {
            red = 0.58f;
            green = 0.78f;
            blue = 0.84f;
        } else if (view.history[index].kind ==
                   MultiplayerHudLineKind::Private) {
            red = 0.82f;
            green = 0.70f;
            blue = 1.0f;
        }
        const std::string line =
            view.history[index].text.substr(0, visibleCharacters);
        Engine::Overlay::QueueText(
            line.c_str(), x + 8.0f,
            textTop - static_cast<float>(index - first) * lineHeight, red,
            green, blue);
    }

    const size_t cursor = std::min(view.cursor, view.draft.size());
    if (!view.inputActive) {
        mDraftViewStart = 0;
    } else if (cursor < mDraftViewStart) {
        mDraftViewStart = cursor;
    }
    const size_t inputCharacters = visibleCharacters > 3
                                       ? visibleCharacters - 3
                                       : visibleCharacters;
    if (cursor > mDraftViewStart + inputCharacters) {
        mDraftViewStart = cursor - inputCharacters;
    }
    mDraftViewStart = std::min(mDraftViewStart, view.draft.size());
    const std::string visibleDraft(
        view.draft.substr(mDraftViewStart, inputCharacters));
    const size_t localCursor =
        std::min(cursor - mDraftViewStart, visibleDraft.size());
    const std::string inputLine =
        view.inputActive
            ? "> " + visibleDraft.substr(0, localCursor) + "|" +
                  visibleDraft.substr(localCursor)
            : "> Enter: chat   /: command";
    Engine::Overlay::QueueRect(
        x + 6.0f, y + 6.0f, x + width - 6.0f, y + inputHeight + 4.0f,
        0.07f, 0.085f, 0.08f, 0.92f);
    Engine::Overlay::QueueText(
        inputLine.c_str(), x + 10.0f, y + 12.0f,
        view.inputActive ? 0.95f : 0.60f,
        view.inputActive ? 0.96f : 0.68f,
        view.inputActive ? 0.92f : 0.66f);
    if (!view.statusNotice.empty()) {
        const std::string notice(view.statusNotice);
        Engine::Overlay::QueueText(
            notice.c_str(), x + 8.0f, y + height + 4.0f, 0.95f, 0.82f,
            0.48f);
    }
    if (!view.gameplayNotice.empty()) {
        const float notificationX =
            std::max(12.0f, (windowWidth - 420.0f) * 0.5f);
        Engine::Overlay::QueueRect(
            notificationX, 42.0f, notificationX + 420.0f, 78.0f, 0.02f,
            0.025f, 0.025f, 0.82f);
        const std::string notice(view.gameplayNotice);
        Engine::Overlay::QueueText(
            notice.c_str(), notificationX + 12.0f, 53.0f, 0.95f, 0.90f,
            0.72f);
    }
}

void MultiplayerHudRenderer::DrawLife(const MultiplayerHudView& view) {
    if (!view.lifeVisible || view.healthCapacity <= 0) return;

    const float windowHeight = static_cast<float>(WindowGetHeight());
    constexpr float labelX = 18.0f;
    constexpr float barX = 72.0f;
    constexpr float barWidth = 192.0f;
    constexpr float barHeight = 18.0f;
    const float barY = std::max(12.0f, windowHeight - 38.0f);
    const int32_t health =
        std::clamp(view.health, 0, view.healthCapacity);
    const float healthFraction =
        static_cast<float>(health) / static_cast<float>(view.healthCapacity);

    Engine::Overlay::QueueText(
        "LIFE", labelX, barY + 1.0f, 0.96f, 0.96f, 0.92f);
    Engine::Overlay::QueueRect(
        barX, barY, barX + barWidth, barY + barHeight, 0.035f, 0.025f,
        0.025f, 0.88f);
    if (health > 0) {
        Engine::Overlay::QueueRect(
            barX + 2.0f, barY + 2.0f,
            barX + 2.0f + (barWidth - 4.0f) * healthFraction,
            barY + barHeight - 2.0f, 0.78f, 0.06f, 0.10f, 0.96f);
    }

    const int32_t segmentSize = std::max(1, view.healthPerSegment);
    const int32_t segmentCount =
        std::max(1, view.healthCapacity / segmentSize);
    for (int32_t segment = 1; segment < segmentCount; ++segment) {
        const float dividerX =
            barX + barWidth * static_cast<float>(segment) /
                       static_cast<float>(segmentCount);
        Engine::Overlay::QueueRect(
            dividerX - 0.5f, barY + 1.0f, dividerX + 0.5f,
            barY + barHeight - 1.0f, 0.18f, 0.04f, 0.05f, 0.92f);
    }
    Engine::Overlay::QueueRect(
        barX, barY, barX + barWidth, barY + barHeight, 0.88f, 0.88f,
        0.82f, 0.96f, true);
}

} // namespace Game::UI
