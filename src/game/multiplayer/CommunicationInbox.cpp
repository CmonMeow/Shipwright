#include "CommunicationInbox.h"
#include "platform/SequenceNumber.h"

#include <utility>

namespace Game::Multiplayer {

namespace {

bool AdmitSequence(std::unordered_map<int32_t, uint32_t>& latest,
                   int32_t playerId, uint32_t sequence) {
    if (playerId < 0 || sequence == 0) return false;
    const auto previous = latest.find(playerId);
    if (previous != latest.end() &&
        !Game::Sequence::IsNewer(sequence, previous->second)) {
        return false;
    }
    latest[playerId] = sequence;
    return true;
}

} // namespace

void CommunicationInbox::Reset() {
    mChat.clear();
    ResetVoiceSession();
}

void CommunicationInbox::ClearVoice() {
    mVoice.clear();
}

void CommunicationInbox::ResetVoiceSession() {
    mVoice.clear();
    mActiveVoicePlayers.clear();
    mLatestReceivedVoiceSequences.clear();
    mLatestVoiceIntentSequences.clear();
}

bool CommunicationInbox::ActivateVoicePlayer(int32_t playerId) {
    if (playerId < 0) return false;
    const bool inserted = mActiveVoicePlayers.insert(playerId).second;
    if (inserted) {
        mLatestReceivedVoiceSequences.erase(playerId);
        mLatestVoiceIntentSequences.erase(playerId);
    }
    return inserted;
}

void CommunicationInbox::ForgetVoicePlayer(int32_t playerId) {
    mActiveVoicePlayers.erase(playerId);
    mLatestReceivedVoiceSequences.erase(playerId);
    mLatestVoiceIntentSequences.erase(playerId);
    for (auto packet = mVoice.begin(); packet != mVoice.end();) {
        if (packet->playerId == playerId) {
            packet = mVoice.erase(packet);
        } else {
            ++packet;
        }
    }
}

void CommunicationInbox::QueueChat(const std::string& text, ChatLineKind kind) {
    const std::string clean = SanitiseChatLine(text);
    if (clean.empty()) return;
    mChat.push_back({ clean, kind });
    while (mChat.size() > CHAT_MAX_HISTORY_LINES) mChat.pop_front();
}

bool CommunicationInbox::PollChat(NetworkChatLine& line) {
    if (mChat.empty()) return false;
    line = std::move(mChat.front());
    mChat.pop_front();
    return true;
}

bool CommunicationInbox::QueueVoice(NetworkVoicePacket packet) {
    if (!IsSaneVoice(packet) ||
        mActiveVoicePlayers.count(packet.playerId) == 0 ||
        !AdmitSequence(mLatestReceivedVoiceSequences, packet.playerId,
                       packet.sequence)) {
        return false;
    }
    mVoice.push_back(std::move(packet));
    while (mVoice.size() > kMaxVoicePackets) mVoice.pop_front();
    return true;
}

bool CommunicationInbox::AdmitVoiceIntent(int32_t playerId,
                                           uint32_t sequence) {
    return mActiveVoicePlayers.count(playerId) != 0 &&
           AdmitSequence(mLatestVoiceIntentSequences, playerId, sequence);
}

bool CommunicationInbox::PollVoice(NetworkVoicePacket& packet) {
    if (mVoice.empty()) return false;
    packet = std::move(mVoice.front());
    mVoice.pop_front();
    return true;
}

bool CommunicationInbox::IsSaneVoice(const NetworkVoicePacket& packet) {
    return packet.playerId >= 0 && packet.sequence != 0 &&
           packet.codec == VOICE_CODEC_OPUS &&
           packet.sampleRate == VOICE_SAMPLE_RATE && packet.frameSamples == VOICE_SAMPLES_PER_PACKET &&
           !packet.data.empty() && packet.data.size() <= VOICE_MAX_OPUS_BYTES;
}

bool CommunicationInbox::IsSaneVoice(const NetworkVoiceIntentPacket& packet) {
    return packet.sequence != 0 && packet.codec == VOICE_CODEC_OPUS &&
           packet.sampleRate == VOICE_SAMPLE_RATE &&
           packet.frameSamples == VOICE_SAMPLES_PER_PACKET && !packet.data.empty() &&
           packet.data.size() <= VOICE_MAX_OPUS_BYTES;
}

} // namespace Game::Multiplayer
