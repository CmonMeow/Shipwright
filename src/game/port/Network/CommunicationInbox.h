#pragma once

#include "NetworkProtocol.h"

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace SoH::Network {

class CommunicationInbox final {
  public:
    void Reset();
    void ClearVoice();
    void ResetVoiceSession();
    bool ActivateVoicePlayer(int32_t playerId);
    void ForgetVoicePlayer(int32_t playerId);

    void QueueChat(const std::string& text, ChatLineKind kind = CLKNormal);
    bool PollChat(NetworkChatLine& line);

    bool QueueVoice(NetworkVoicePacket packet);
    bool AdmitVoiceIntent(int32_t playerId, uint32_t sequence);
    bool PollVoice(NetworkVoicePacket& packet);

    static bool IsSaneVoice(const NetworkVoicePacket& packet);
    static bool IsSaneVoice(const NetworkVoiceIntentPacket& packet);

    size_t ChatCount() const { return mChat.size(); }
    size_t VoiceCount() const { return mVoice.size(); }

  private:
    static constexpr size_t kMaxVoicePackets = 64;

    std::deque<NetworkChatLine> mChat;
    std::deque<NetworkVoicePacket> mVoice;
    std::unordered_set<int32_t> mActiveVoicePlayers;
    std::unordered_map<int32_t, uint32_t> mLatestReceivedVoiceSequences;
    std::unordered_map<int32_t, uint32_t> mLatestVoiceIntentSequences;
};

} // namespace SoH::Network
