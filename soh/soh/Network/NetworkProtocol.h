#pragma once

#include "Network/netTransport.hpp"
#include "sodium.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

using std::string;
using std::vector;

inline constexpr const char* DEFAULT_NETWORK_ADDRESS = "127.0.0.1";
inline constexpr unsigned short DEFAULT_NETWORK_PORT = 777;
inline constexpr __int32 APP_PROTOCOL_VERSION = 39;
inline constexpr const char* BAN_LIST_FILENAME = "resource\\banlist.txt";
inline constexpr const char* GM_LIST_FILENAME = "resource\\gmlist.txt";
inline constexpr size_t CHAT_MAX_MESSAGE_CHARS = 140;
inline constexpr size_t CHAT_MAX_LINE_CHARS = 192;
inline constexpr size_t CHAT_WRAP_CHARS = 54;
inline constexpr size_t CHAT_VISIBLE_ROWS = 9;
inline constexpr size_t CHAT_MAX_HISTORY_LINES = 500;
inline constexpr size_t NET_MAX_TEXT_BYTES = 4096;
inline constexpr size_t NET_MAX_ENCRYPTED_BYTES = 65536;
inline constexpr size_t NET_MAX_PLAYER_BATCH_OBJECTS = 16;
inline constexpr size_t NET_MAX_WORLD_BYTES = 16 * 1024 * 1024;
inline constexpr size_t NET_WORLD_CHUNK_BYTES = 1024;
inline constexpr size_t NET_MAX_WORLD_LEVELS = 256;
inline constexpr unsigned __int64 NET_CLIENT_INTENT_MS = 16;
inline constexpr unsigned __int64 NET_PLAYER_SYNC_MS = 50;
inline constexpr unsigned __int64 NET_RESPAWN_MS = 5000;

struct NetworkIdentity
{
    __int32 protocolVersion;
    bool voiceClient;
    string id;
    string name;
};

enum ChatLineKind
{
    CLKNormal,
    CLKSystem,
    CLKPrivate
};

struct NetworkChatLine
{
    string text;
    ChatLineKind kind;
};

inline string SanitiseText(const string& text, size_t maxChars)
{
    string result;
    result.reserve(text.size() < maxChars ? text.size() : maxChars);
    for (size_t i = 0; i < text.size() && result.size() < maxChars; ++i)
    {
        unsigned char c = (unsigned char)text[i];
        if (c >= 32 && c != 127)
        {
            result.push_back((char)c);
        }
    }
    return result;
}

inline string SanitiseChatText(const string& text)
{
    return SanitiseText(text, CHAT_MAX_MESSAGE_CHARS);
}

inline string SanitiseChatLine(const string& text)
{
    return SanitiseText(text, CHAT_MAX_LINE_CHARS);
}

inline string TrimWhitespace(const string& text)
{
    size_t first = 0;
    while (first < text.size() && isspace((unsigned char)text[first]))
    {
        ++first;
    }
    size_t last = text.size();
    while (last > first && isspace((unsigned char)text[last - 1]))
    {
        --last;
    }
    return text.substr(first, last - first);
}

inline bool AddUniqueString(vector<string>& list, const string& value)
{
    if (value.empty() || std::find(list.begin(), list.end(), value) != list.end())
    {
        return false;
    }
    list.push_back(value);
    return true;
}

inline string SanitiseIdentityText(const string& text, size_t maxChars)
{
    string result = SanitiseText(text, maxChars);
    for (size_t i = 0; i < result.size(); ++i)
    {
        if (result[i] == '|' || result[i] == '\r' || result[i] == '\n')
        {
            result[i] = '_';
        }
    }
    return TrimWhitespace(result);
}

inline string LocalIdentityId()
{
    DWORD serial = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0);
    std::ostringstream value;
    value << serial;
    return value.str();
}

inline string LocalUserName()
{
    char buf[256];
    DWORD bufSize = sizeof(buf);
    if (GetUserNameA(buf, &bufSize) && bufSize > 1)
    {
        return SanitiseIdentityText(buf, 48);
    }
    return "Anon";
}

inline void LoadIdentityList(const char* filename, vector<string>& list)
{
    list.clear();
    std::ifstream f(filename);
    if (!f.good())
    {
        return;
    }

    string token;
    char c;
    while (f.get(c))
    {
        if (isspace((unsigned char)c) || c == ',' || c == ';')
        {
            AddUniqueString(list, SanitiseIdentityText(token, 64));
            token.clear();
        }
        else if (token.size() < 255)
        {
            token.push_back(c);
        }
    }

    AddUniqueString(list, SanitiseIdentityText(token, 64));
}

inline void SaveIdentityList(const char* filename, const vector<string>& list)
{
    std::ofstream f(filename, std::ios::out | std::ios::trunc);
    if (!f.good())
    {
        return;
    }

    for (size_t i = 0; i < list.size(); ++i)
    {
        if (!list[i].empty())
        {
            f << list[i] << "\r\n";
        }
    }
}

inline void LoadBanList(vector<string>& list)
{
    LoadIdentityList(BAN_LIST_FILENAME, list);
}

inline void SaveBanList(const vector<string>& list)
{
    SaveIdentityList(BAN_LIST_FILENAME, list);
}

inline void LoadGameMasterList(vector<string>& list)
{
    LoadIdentityList(GM_LIST_FILENAME, list);
}

inline void SaveGameMasterList(const vector<string>& list)
{
    SaveIdentityList(GM_LIST_FILENAME, list);
}

class NetworkMessageRaw
{
    vector<char> _buffer;
    const char* _externalBuffer;
    __int32 _externalBufferSize;
    __int32 _pos;

public:
    NetworkMessageRaw()
        : _externalBuffer(NULL), _externalBufferSize(0), _pos(0)
    {
        _buffer.reserve(512);
    }

    NetworkMessageRaw(const char* buffer, __int32 size)
        : _externalBuffer(buffer), _externalBufferSize(size), _pos(0)
    {
    }

    const char* data() const
    {
        return _externalBuffer ? _externalBuffer : (_buffer.empty() ? NULL : &_buffer[0]);
    }

    __int32 size() const
    {
        return _externalBuffer ? _externalBufferSize : (__int32)_buffer.size();
    }

    void put(const void* value, __int32 valueSize)
    {
        if (!value || valueSize <= 0)
        {
            return;
        }
        __int32 minSize = _pos + valueSize;
        if ((__int32)_buffer.size() < minSize)
        {
            _buffer.resize(minSize);
        }
        memcpy(&_buffer[_pos], value, valueSize);
        _pos += valueSize;
    }

    bool get(void* value, __int32 valueSize)
    {
        const char* source = data();
        if (!value || valueSize < 0 || !source || remaining() < valueSize)
        {
            return false;
        }
        memcpy(value, source + _pos, valueSize);
        _pos += valueSize;
        return true;
    }

    __int32 remaining() const
    {
        __int32 total = size();
        if (_pos < 0 || _pos > total)
        {
            return 0;
        }
        return total - _pos;
    }

    bool fullyRead() const
    {
        return remaining() == 0;
    }

    void putUInt8(unsigned char value) { put(&value, sizeof(value)); }
    void putUInt16(unsigned short value) { put(&value, sizeof(value)); }
    void putUInt32(unsigned __int32 value) { put(&value, sizeof(value)); }
    void putInt32(__int32 value) { put(&value, sizeof(value)); }
    void putFloat(float value) { put(&value, sizeof(value)); }
    void putBytes(const vector<unsigned char>& value, size_t maxLength = 65535)
    {
        if (maxLength > 65535)
        {
            maxLength = 65535;
        }
        size_t length = value.size();
        if (length > maxLength)
        {
            length = maxLength;
        }
        putUInt16((unsigned short)length);
        if (length > 0)
        {
            put(&value[0], (__int32)length);
        }
    }
    void putString(const string& value, size_t maxLength = 255)
    {
        if (maxLength > 255)
        {
            maxLength = 255;
        }
        size_t length = value.size();
        if (length > maxLength)
        {
            length = maxLength;
        }
        if (length > 255)
        {
            length = 255;
        }
        putUInt8((unsigned char)length);
        if (length > 0)
        {
            put(value.data(), (__int32)length);
        }
    }
    bool getUInt8(unsigned char& value) { return get(&value, sizeof(value)); }
    bool getUInt16(unsigned short& value) { return get(&value, sizeof(value)); }
    bool getUInt32(unsigned __int32& value) { return get(&value, sizeof(value)); }
    bool getInt32(__int32& value) { return get(&value, sizeof(value)); }
    bool getFloat(float& value) { return get(&value, sizeof(value)); }
    bool getBytes(vector<unsigned char>& value, size_t maxLength = 65535)
    {
        value.clear();
        if (maxLength > 65535)
        {
            maxLength = 65535;
        }
        unsigned short length = 0;
        if (!getUInt16(length) || length > maxLength || remaining() < (__int32)length)
        {
            return false;
        }
        if (length == 0)
        {
            return true;
        }
        value.resize(length);
        return get(&value[0], length);
    }
    bool getString(string& value, size_t maxLength = 255)
    {
        value.clear();
        if (maxLength > 255)
        {
            maxLength = 255;
        }
        unsigned char length = 0;
        if (!getUInt8(length) || length > maxLength || remaining() < (__int32)length)
        {
            return false;
        }
        if (length == 0)
        {
            return true;
        }
        value.resize(length);
        return get(&value[0], length);
    }
};

inline string LocalExecutableName()
{
    char path[MAX_PATH];
    if (!GetModuleFileNameA(NULL, path, sizeof(path)))
    {
        return "GameClient.exe";
    }
    const char* slash = strrchr(path, '\\');
    const char* slash2 = strrchr(path, '/');
    if (slash2 && (!slash || slash2 > slash))
    {
        slash = slash2;
    }
    return slash ? string(slash + 1) : string(path);
}

inline void EncodeLocalIdentityRaw(NetworkMessageRaw& raw)
{
    raw.putInt32(APP_PROTOCOL_VERSION);
    raw.putString(LocalIdentityId(), 64);
    raw.putString(LocalUserName(), 48);
    raw.putUInt8(_stricmp(LocalExecutableName().c_str(), "PathEngineVoice.exe") == 0 ? 1 : 0);
}

inline string BuildAppRawMessage(NetAppMessageType type, const NetworkMessageRaw& raw)
{
    string payload;
    payload.resize(sizeof(NetAppMessageHeader) + raw.size());
    NetAppMessageHeader header;
    header.type = type;
    memcpy(&payload[0], &header, sizeof(header));
    if (raw.size() > 0)
    {
        memcpy(&payload[sizeof(header)], raw.data(), raw.size());
    }
    return payload;
}

inline bool ValidAppMessageType(NetAppMessageType type)
{
    return type >= NAMTConnect && type <= NAMTEncrypted;
}

inline const char* ConnectResultName(ConnectResult result)
{
    switch (result)
    {
    case CROK: return "Sucess";
    case CRPassword: return "Invalid Password";
    case CRVersion: return "Incompatible Version";
    case CRError: return "Error";
    case CRName: return "Bad Name";
    case CRSessionFull: return "Session Full";
    case CRNone: return "Timeout";
    default: return "Unknown";
    }
}

inline bool ParseAppRawString(const char* buffer, __int32 bufferSize, NetAppMessageType expectedType, string& text, size_t maxLength)
{
    text.clear();
    if (!buffer || bufferSize < (__int32)sizeof(NetAppMessageHeader))
    {
        return false;
    }

    const NetAppMessageHeader* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (header->type != expectedType || !ValidAppMessageType(header->type))
    {
        return false;
    }

    const __int32 payloadSize = bufferSize - (__int32)sizeof(NetAppMessageHeader);
    if (payloadSize < 0 || (size_t)payloadSize > NET_MAX_TEXT_BYTES)
    {
        return false;
    }

    NetworkMessageRaw raw(buffer + sizeof(NetAppMessageHeader), payloadSize);
    return raw.getString(text, maxLength) && raw.fullyRead();
}

inline bool ParseAppRawBytes(const char* buffer, __int32 bufferSize, NetAppMessageType expectedType, string& bytes, size_t expectedBytes)
{
    bytes.clear();
    if (!buffer || bufferSize < (__int32)sizeof(NetAppMessageHeader))
    {
        return false;
    }

    const NetAppMessageHeader* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (header->type != expectedType || !ValidAppMessageType(header->type))
    {
        return false;
    }

    const __int32 payloadSize = bufferSize - (__int32)sizeof(NetAppMessageHeader);
    if (payloadSize < 0 || (size_t)payloadSize != expectedBytes || expectedBytes > NET_MAX_ENCRYPTED_BYTES)
    {
        return false;
    }

    bytes.assign(buffer + sizeof(NetAppMessageHeader), buffer + bufferSize);
    return true;
}

inline bool ParseAppRawControl(const char* buffer, __int32 bufferSize, NetAppMessageType expectedType)
{
    if (!buffer || bufferSize != (__int32)sizeof(NetAppMessageHeader))
    {
        return false;
    }

    const NetAppMessageHeader* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    return header->type == expectedType && ValidAppMessageType(header->type);
}

inline bool ParseIdentityRaw(const char* buffer, __int32 bufferSize, NetworkIdentity& identity)
{
    identity = NetworkIdentity();
    if (!buffer || bufferSize < (__int32)sizeof(NetAppMessageHeader))
    {
        return false;
    }

    const NetAppMessageHeader* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (header->type != NAMTConnect || !ValidAppMessageType(header->type))
    {
        return false;
    }

    const __int32 payloadSize = bufferSize - (__int32)sizeof(NetAppMessageHeader);
    if (payloadSize < 0 || (size_t)payloadSize > NET_MAX_TEXT_BYTES)
    {
        return false;
    }

    NetworkMessageRaw raw(buffer + sizeof(NetAppMessageHeader), payloadSize);
    __int32 version = 0;
    string id;
    string name;
    unsigned char voiceClient = 0;
    if (!raw.getInt32(version) ||
        !raw.getString(id, 64) ||
        !raw.getString(name, 48))
    {
        return false;
    }
    if (raw.remaining() > 0 && !raw.getUInt8(voiceClient))
    {
        return false;
    }
    if (!raw.fullyRead())
    {
        return false;
    }
    if (version != APP_PROTOCOL_VERSION)
    {
        return false;
    }

    identity.protocolVersion = version;
    identity.voiceClient = voiceClient != 0;
    identity.id = SanitiseIdentityText(id, 64);
    identity.name = SanitiseIdentityText(name, 48);
    if (identity.name.empty())
    {
        identity.name = "Anon";
    }
    return !identity.id.empty();
}

inline bool ParseIdentityRawAnyVersion(const char* buffer, __int32 bufferSize, NetworkIdentity& identity)
{
    identity = NetworkIdentity();
    if (!buffer || bufferSize < (__int32)sizeof(NetAppMessageHeader))
    {
        return false;
    }

    const NetAppMessageHeader* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (header->type != NAMTConnect || !ValidAppMessageType(header->type))
    {
        return false;
    }

    const __int32 payloadSize = bufferSize - (__int32)sizeof(NetAppMessageHeader);
    if (payloadSize < 0 || (size_t)payloadSize > NET_MAX_TEXT_BYTES)
    {
        return false;
    }

    NetworkMessageRaw raw(buffer + sizeof(NetAppMessageHeader), payloadSize);
    string id;
    string name;
    unsigned char voiceClient = 0;
    if (!raw.getInt32(identity.protocolVersion) ||
        !raw.getString(id, 64) ||
        !raw.getString(name, 48))
    {
        return false;
    }
    if (raw.remaining() > 0 && !raw.getUInt8(voiceClient))
    {
        return false;
    }
    if (!raw.fullyRead())
    {
        return false;
    }

    identity.voiceClient = voiceClient != 0;
    identity.id = SanitiseIdentityText(id, 64);
    identity.name = SanitiseIdentityText(name, 48);
    if (identity.name.empty())
    {
        identity.name = "Anon";
    }
    return !identity.id.empty();
}

#pragma pack(push, networkPlayerPackets, 1)
enum
{
    NETWORK_PLAYER_LIMB_COUNT = 22,
    NETWORK_FISHING_LINE_POINT_COUNT = 200,
    NETWORK_PLAYER_ITEM_FISHING_POLE = 2,
    NETWORK_PLAYER_VISIBLE = 1,
    NETWORK_PLAYER_GROUNDED = 2,
    NETWORK_PLAYER_SWIMMING = 4,
    NETWORK_PLAYER_READY_TO_FIRE = 8,
    NETWORK_PLAYER_DEAD = 16
};

struct NetworkPlayerAssignPacket
{
    __int32 playerId;
};

struct NetworkPlayerStatePacket
{
    __int32 playerId;
    __int32 sceneId;
    __int32 roomId;
    __int32 sequence;
    float x;
    float y;
    float z;
    short rotationX;
    short rotationY;
    short rotationZ;
    short aimPitch;
    short aimYaw;
    float speed;
    unsigned __int32 stateFlags;
    unsigned char modelGroup;
    unsigned char itemAction;
    unsigned char fishingState;
    signed char meleeWeaponState;
    short upperLimbRot[3];
    short headLimbRot[3];
    float meleeBase[3];
    float meleeTip[3];
    float bowStringScale;
    float fishingRodTipOffset[3];
    float fishingLureOffset[3];
    float fishingLureDrawOffset[3];
    float fishingRodBendY;
    float fishingRodBendX;
    float fishingRodTwist;
    float fishingRodCastX;
    float fishingLureRot[3];
    float fishingLureSpin;
    float fishingLureZOffset;
    float fishingLureHookOffsets[2][3];
    float fishingLureHookRot[2][2];
    float fishingLineScale;
    float fishingLineGravity;
    unsigned char fishingLureType;
    unsigned char fishingLineSpooled;
    unsigned char fishingLineHooked;
    unsigned char fishingSinkingLureSegmentIndex;
    unsigned char fishingSinkingLureUnderwater;
    unsigned char fishingFishActive;
    unsigned char fishingFishIsLoach;
    float fishingFishOffset[3];
    short fishingFishRot[3];
    short fishingFishLimbRot[8];
    float fishingFishLength;
    short jointTable[22][3];
};

struct NetworkDynamicObjectStatePacket
{
    __int32 sceneId;
    __int32 roomId;
    __int32 actorId;
    __int32 actorParams;
    __int32 homeX;
    __int32 homeY;
    __int32 homeZ;
    unsigned char destroyed;
};

struct NetworkActorEventPacket
{
    __int32 sourcePlayerId;
    __int32 eventId;
    __int32 sceneId;
    __int32 roomId;
    __int32 actorId;
    __int32 actorParams;
    __int32 homeX;
    __int32 homeY;
    __int32 homeZ;
    float x;
    float y;
    float z;
    unsigned char eventType;
};

enum
{
    NETWORK_ACTOR_EVENT_GRASS_CUT = 1,
    NETWORK_ACTOR_EVENT_BOULDER_BREAK = 2,
    NETWORK_ACTOR_EVENT_OWL_DEPART = 3,
    NETWORK_ACTOR_EVENT_FISH_HOOK = 4,
    NETWORK_ACTOR_EVENT_FISH_RELEASE = 5,
    NETWORK_ACTOR_EVENT_GRASS_THROWN_BREAK = 6
};

struct NetworkProjectileStatePacket
{
    __int32 playerId;
    __int32 projectileId;
    __int32 sceneId;
    unsigned __int32 sequence;
    unsigned char active;
    unsigned char projectileKind;
    unsigned char phase;
    unsigned char projectileType;
    float x;
    float y;
    float z;
    short rotationX;
    short rotationY;
    short rotationZ;
    float velocityX;
    float velocityY;
    float velocityZ;
};

struct NetworkProjectileImpactPacket
{
    __int32 ownerPlayerId;
    __int32 projectileId;
    __int32 sceneId;
    float x;
    float y;
    float z;
};

enum
{
    NETWORK_PROJECTILE_ARROW = 0,
    NETWORK_PROJECTILE_BOMB = 1,
    NETWORK_ARROW_FLYING = 0,
    NETWORK_ARROW_STUCK = 1,
    NETWORK_BOMB_HELD = 0,
    NETWORK_BOMB_RELEASED = 1,
    NETWORK_BOMB_EXPLODING = 2
};

struct NetworkPlayerIntentPacket
{
    __int32 playerId;
    __int32 levelIndex;
    unsigned char flags;
    float localX;
    float localY;
    float heading;
    float deltaTime;
    __int32 targetX;
    __int32 targetY;
    __int32 targetCell;
};

enum
{
    NPIFDirect = 1,
    NPIFPathTarget = 2
};

struct NetworkLevelAdvancePacket
{
    __int32 levelIndex;
    __int32 result;
};

struct NetworkPlayerRemovePacket
{
    __int32 playerId;
    __int32 levelIndex;
};

struct NetworkPlayerDamagePacket
{
    __int32 sourcePlayerId;
    __int32 targetPlayerId;
    short damage;
    short impactYaw;
};

struct NetworkPlayerRespawnPacket
{
    __int32 playerId;
};

enum
{
    VOICE_SAMPLE_RATE = 16000,
    VOICE_SAMPLES_PER_PACKET = 320,
    VOICE_MAX_OPUS_BYTES = 1275,
    VOICE_CODEC_OPUS = 1,
    VOICE_CODEC_ADPCM = 2
};

#pragma pack(pop, networkPlayerPackets)

struct NetworkVoicePacket
{
    __int32 playerId;
    unsigned __int32 sequence;
    unsigned char codec;
    unsigned short sampleRate;
    unsigned short frameSamples;
    vector<unsigned char> data;
};

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerAssignPacket& packet)
{
    raw.putInt32(packet.playerId);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerAssignPacket& packet)
{
    return raw.getInt32(packet.playerId) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerStatePacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putInt32(packet.sceneId);
    raw.putInt32(packet.roomId);
    raw.putInt32(packet.sequence);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putUInt16(static_cast<unsigned short>(packet.rotationX));
    raw.putUInt16(static_cast<unsigned short>(packet.rotationY));
    raw.putUInt16(static_cast<unsigned short>(packet.rotationZ));
    raw.putUInt16(static_cast<unsigned short>(packet.aimPitch));
    raw.putUInt16(static_cast<unsigned short>(packet.aimYaw));
    raw.putFloat(packet.speed);
    raw.putUInt32(packet.stateFlags);
    raw.putUInt8(packet.modelGroup);
    raw.putUInt8(packet.itemAction);
    raw.putUInt8(packet.fishingState);
    raw.putUInt8(static_cast<unsigned char>(packet.meleeWeaponState));
    for (unsigned char axis = 0; axis < 3; ++axis)
    {
        raw.putUInt16(static_cast<unsigned short>(packet.upperLimbRot[axis]));
        raw.putUInt16(static_cast<unsigned short>(packet.headLimbRot[axis]));
    }
    for (unsigned char axis = 0; axis < 3; ++axis)
    {
        raw.putFloat(packet.meleeBase[axis]);
        raw.putFloat(packet.meleeTip[axis]);
    }
    raw.putFloat(packet.bowStringScale);
    if (packet.itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE)
    {
        for (unsigned char axis = 0; axis < 3; ++axis)
        {
            raw.putFloat(packet.fishingRodTipOffset[axis]);
            raw.putFloat(packet.fishingLureOffset[axis]);
            raw.putFloat(packet.fishingLureDrawOffset[axis]);
        }
        raw.putFloat(packet.fishingRodBendY);
        raw.putFloat(packet.fishingRodBendX);
        raw.putFloat(packet.fishingRodTwist);
        raw.putFloat(packet.fishingRodCastX);
        for (unsigned char axis = 0; axis < 3; ++axis)
            raw.putFloat(packet.fishingLureRot[axis]);
        raw.putFloat(packet.fishingLureSpin);
        raw.putFloat(packet.fishingLureZOffset);
        for (unsigned char hook = 0; hook < 2; ++hook)
        {
            for (unsigned char axis = 0; axis < 3; ++axis)
                raw.putFloat(packet.fishingLureHookOffsets[hook][axis]);
            for (unsigned char axis = 0; axis < 2; ++axis)
                raw.putFloat(packet.fishingLureHookRot[hook][axis]);
        }
        raw.putFloat(packet.fishingLineScale);
        raw.putFloat(packet.fishingLineGravity);
        raw.putUInt8(packet.fishingLureType);
        raw.putUInt8(packet.fishingLineSpooled);
        raw.putUInt8(packet.fishingLineHooked);
        raw.putUInt8(packet.fishingSinkingLureSegmentIndex);
        raw.putUInt8(packet.fishingSinkingLureUnderwater);
        raw.putUInt8(packet.fishingFishActive);
        raw.putUInt8(packet.fishingFishIsLoach);
        for (unsigned char axis = 0; axis < 3; ++axis)
            raw.putFloat(packet.fishingFishOffset[axis]);
        for (unsigned char axis = 0; axis < 3; ++axis)
            raw.putUInt16(static_cast<unsigned short>(packet.fishingFishRot[axis]));
        for (unsigned char limbRot = 0; limbRot < 8; ++limbRot)
            raw.putUInt16(static_cast<unsigned short>(packet.fishingFishLimbRot[limbRot]));
        raw.putFloat(packet.fishingFishLength);
    }
    for (unsigned char limb = 0; limb < NETWORK_PLAYER_LIMB_COUNT; ++limb)
    {
        raw.putUInt16(static_cast<unsigned short>(packet.jointTable[limb][0]));
        raw.putUInt16(static_cast<unsigned short>(packet.jointTable[limb][1]));
        raw.putUInt16(static_cast<unsigned short>(packet.jointTable[limb][2]));
    }
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerStatePacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    unsigned short rotationX = 0;
    unsigned short rotationY = 0;
    unsigned short rotationZ = 0;
    unsigned short aimPitch = 0;
    unsigned short aimYaw = 0;
    if (!raw.getInt32(packet.playerId) ||
        !raw.getInt32(packet.sceneId) ||
        !raw.getInt32(packet.roomId) ||
        !raw.getInt32(packet.sequence) ||
        !raw.getFloat(packet.x) ||
        !raw.getFloat(packet.y) ||
        !raw.getFloat(packet.z) ||
        !raw.getUInt16(rotationX) ||
        !raw.getUInt16(rotationY) ||
        !raw.getUInt16(rotationZ) ||
        !raw.getUInt16(aimPitch) ||
        !raw.getUInt16(aimYaw) ||
        !raw.getFloat(packet.speed) ||
        !raw.getUInt32(packet.stateFlags) ||
        !raw.getUInt8(packet.modelGroup) ||
        !raw.getUInt8(packet.itemAction) ||
        !raw.getUInt8(packet.fishingState))
    {
        return false;
    }
    unsigned char meleeWeaponState = 0;
    if (!raw.getUInt8(meleeWeaponState))
        return false;
    packet.meleeWeaponState = static_cast<signed char>(meleeWeaponState);
    packet.rotationX = static_cast<short>(rotationX);
    packet.rotationY = static_cast<short>(rotationY);
    packet.rotationZ = static_cast<short>(rotationZ);
    packet.aimPitch = static_cast<short>(aimPitch);
    packet.aimYaw = static_cast<short>(aimYaw);
    for (unsigned char axis = 0; axis < 3; ++axis)
    {
        unsigned short upper = 0;
        unsigned short head = 0;
        if (!raw.getUInt16(upper) || !raw.getUInt16(head))
            return false;
        packet.upperLimbRot[axis] = static_cast<short>(upper);
        packet.headLimbRot[axis] = static_cast<short>(head);
    }
    for (unsigned char axis = 0; axis < 3; ++axis)
    {
        if (!raw.getFloat(packet.meleeBase[axis]) || !raw.getFloat(packet.meleeTip[axis]))
            return false;
    }
    if (!raw.getFloat(packet.bowStringScale))
        return false;
    if (packet.itemAction == NETWORK_PLAYER_ITEM_FISHING_POLE)
    {
        for (unsigned char axis = 0; axis < 3; ++axis)
        {
            if (!raw.getFloat(packet.fishingRodTipOffset[axis]) ||
                !raw.getFloat(packet.fishingLureOffset[axis]) ||
                !raw.getFloat(packet.fishingLureDrawOffset[axis]))
            {
                return false;
            }
        }
        if (!raw.getFloat(packet.fishingRodBendY) || !raw.getFloat(packet.fishingRodBendX) ||
            !raw.getFloat(packet.fishingRodTwist) || !raw.getFloat(packet.fishingRodCastX))
            return false;
        for (unsigned char axis = 0; axis < 3; ++axis)
        {
            if (!raw.getFloat(packet.fishingLureRot[axis]))
                return false;
        }
        if (!raw.getFloat(packet.fishingLureSpin) || !raw.getFloat(packet.fishingLureZOffset))
            return false;
        for (unsigned char hook = 0; hook < 2; ++hook)
        {
            for (unsigned char axis = 0; axis < 3; ++axis)
                if (!raw.getFloat(packet.fishingLureHookOffsets[hook][axis]))
                    return false;
            for (unsigned char axis = 0; axis < 2; ++axis)
                if (!raw.getFloat(packet.fishingLureHookRot[hook][axis]))
                    return false;
        }
        if (!raw.getFloat(packet.fishingLineScale) || !raw.getFloat(packet.fishingLineGravity) ||
            !raw.getUInt8(packet.fishingLureType) ||
            !raw.getUInt8(packet.fishingLineSpooled) || !raw.getUInt8(packet.fishingLineHooked) ||
            !raw.getUInt8(packet.fishingSinkingLureSegmentIndex) ||
            !raw.getUInt8(packet.fishingSinkingLureUnderwater) ||
            !raw.getUInt8(packet.fishingFishActive) || !raw.getUInt8(packet.fishingFishIsLoach))
            return false;
        for (unsigned char axis = 0; axis < 3; ++axis)
            if (!raw.getFloat(packet.fishingFishOffset[axis]))
                return false;
        for (unsigned char axis = 0; axis < 3; ++axis)
        {
            unsigned short fishRot = 0;
            if (!raw.getUInt16(fishRot))
                return false;
            packet.fishingFishRot[axis] = static_cast<short>(fishRot);
        }
        for (unsigned char limbRot = 0; limbRot < 8; ++limbRot)
        {
            unsigned short fishLimbRot = 0;
            if (!raw.getUInt16(fishLimbRot))
                return false;
            packet.fishingFishLimbRot[limbRot] = static_cast<short>(fishLimbRot);
        }
        if (!raw.getFloat(packet.fishingFishLength))
            return false;
    }
    for (unsigned char limb = 0; limb < NETWORK_PLAYER_LIMB_COUNT; ++limb)
    {
        unsigned short x = 0;
        unsigned short y = 0;
        unsigned short z = 0;
        if (!raw.getUInt16(x) || !raw.getUInt16(y) || !raw.getUInt16(z))
        {
            return false;
        }
        packet.jointTable[limb][0] = static_cast<short>(x);
        packet.jointTable[limb][1] = static_cast<short>(y);
        packet.jointTable[limb][2] = static_cast<short>(z);
    }
    return raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkDynamicObjectStatePacket& packet)
{
    raw.putInt32(packet.sceneId);
    raw.putInt32(packet.roomId);
    raw.putInt32(packet.actorId);
    raw.putInt32(packet.actorParams);
    raw.putInt32(packet.homeX);
    raw.putInt32(packet.homeY);
    raw.putInt32(packet.homeZ);
    raw.putUInt8(packet.destroyed);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkDynamicObjectStatePacket& packet)
{
    return raw.getInt32(packet.sceneId) && raw.getInt32(packet.roomId) && raw.getInt32(packet.actorId) &&
           raw.getInt32(packet.actorParams) && raw.getInt32(packet.homeX) && raw.getInt32(packet.homeY) &&
           raw.getInt32(packet.homeZ) && raw.getUInt8(packet.destroyed) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkActorEventPacket& packet)
{
    raw.putInt32(packet.sourcePlayerId);
    raw.putInt32(packet.eventId);
    raw.putInt32(packet.sceneId);
    raw.putInt32(packet.roomId);
    raw.putInt32(packet.actorId);
    raw.putInt32(packet.actorParams);
    raw.putInt32(packet.homeX);
    raw.putInt32(packet.homeY);
    raw.putInt32(packet.homeZ);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putUInt8(packet.eventType);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkActorEventPacket& packet)
{
    return raw.getInt32(packet.sourcePlayerId) && raw.getInt32(packet.eventId) && raw.getInt32(packet.sceneId) &&
           raw.getInt32(packet.roomId) && raw.getInt32(packet.actorId) && raw.getInt32(packet.actorParams) &&
           raw.getInt32(packet.homeX) && raw.getInt32(packet.homeY) && raw.getInt32(packet.homeZ) &&
           raw.getFloat(packet.x) && raw.getFloat(packet.y) && raw.getFloat(packet.z) &&
           raw.getUInt8(packet.eventType) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkProjectileStatePacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putInt32(packet.projectileId);
    raw.putInt32(packet.sceneId);
    raw.putUInt32(packet.sequence);
    raw.putUInt8(packet.active);
    raw.putUInt8(packet.projectileKind);
    raw.putUInt8(packet.phase);
    raw.putUInt8(packet.projectileType);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putUInt16(static_cast<unsigned short>(packet.rotationX));
    raw.putUInt16(static_cast<unsigned short>(packet.rotationY));
    raw.putUInt16(static_cast<unsigned short>(packet.rotationZ));
    raw.putFloat(packet.velocityX);
    raw.putFloat(packet.velocityY);
    raw.putFloat(packet.velocityZ);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkProjectileStatePacket& packet)
{
    unsigned short rotationX = 0;
    unsigned short rotationY = 0;
    unsigned short rotationZ = 0;
    if (!raw.getInt32(packet.playerId) || !raw.getInt32(packet.projectileId) || !raw.getInt32(packet.sceneId) ||
        !raw.getUInt32(packet.sequence) ||
        !raw.getUInt8(packet.active) || !raw.getUInt8(packet.projectileKind) || !raw.getUInt8(packet.phase) ||
        !raw.getUInt8(packet.projectileType) || !raw.getFloat(packet.x) ||
        !raw.getFloat(packet.y) || !raw.getFloat(packet.z) || !raw.getUInt16(rotationX) ||
        !raw.getUInt16(rotationY) || !raw.getUInt16(rotationZ) || !raw.getFloat(packet.velocityX) ||
        !raw.getFloat(packet.velocityY) || !raw.getFloat(packet.velocityZ) || !raw.fullyRead())
    {
        return false;
    }
    packet.rotationX = static_cast<short>(rotationX);
    packet.rotationY = static_cast<short>(rotationY);
    packet.rotationZ = static_cast<short>(rotationZ);
    return true;
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkProjectileImpactPacket& packet)
{
    raw.putInt32(packet.ownerPlayerId);
    raw.putInt32(packet.projectileId);
    raw.putInt32(packet.sceneId);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkProjectileImpactPacket& packet)
{
    return raw.getInt32(packet.ownerPlayerId) && raw.getInt32(packet.projectileId) &&
           raw.getInt32(packet.sceneId) && raw.getFloat(packet.x) && raw.getFloat(packet.y) &&
           raw.getFloat(packet.z) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerDamagePacket& packet)
{
    raw.putInt32(packet.sourcePlayerId);
    raw.putInt32(packet.targetPlayerId);
    raw.putUInt16(static_cast<unsigned short>(packet.damage));
    raw.putUInt16(static_cast<unsigned short>(packet.impactYaw));
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerDamagePacket& packet)
{
    unsigned short damage = 0;
    unsigned short impactYaw = 0;
    if (!raw.getInt32(packet.sourcePlayerId) || !raw.getInt32(packet.targetPlayerId) || !raw.getUInt16(damage) ||
        !raw.getUInt16(impactYaw) ||
        !raw.fullyRead())
        return false;
    packet.damage = static_cast<short>(damage);
    packet.impactYaw = static_cast<short>(impactYaw);
    return true;
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerRespawnPacket& packet)
{
    raw.putInt32(packet.playerId);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerRespawnPacket& packet)
{
    return raw.getInt32(packet.playerId) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerIntentPacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putInt32(packet.levelIndex);
    raw.putUInt8(packet.flags);
    raw.putFloat(packet.heading);
    raw.putFloat(packet.deltaTime);
    if ((packet.flags & NPIFDirect) != 0)
    {
        raw.putFloat(packet.localX);
        raw.putFloat(packet.localY);
    }
    if ((packet.flags & NPIFPathTarget) != 0)
    {
        raw.putInt32(packet.targetX);
        raw.putInt32(packet.targetY);
        raw.putInt32(packet.targetCell);
    }
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerIntentPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    if (!raw.getInt32(packet.playerId) ||
        !raw.getInt32(packet.levelIndex) ||
        !raw.getUInt8(packet.flags) ||
        !raw.getFloat(packet.heading) ||
        !raw.getFloat(packet.deltaTime))
    {
        return false;
    }
    if ((packet.flags & NPIFDirect) != 0)
    {
        if (!raw.getFloat(packet.localX) || !raw.getFloat(packet.localY))
        {
            return false;
        }
    }
    if ((packet.flags & NPIFPathTarget) != 0)
    {
        if (!raw.getInt32(packet.targetX) ||
            !raw.getInt32(packet.targetY) ||
            !raw.getInt32(packet.targetCell))
        {
            return false;
        }
    }
    return raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkLevelAdvancePacket& packet)
{
    raw.putInt32(packet.levelIndex);
    raw.putInt32(packet.result);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkLevelAdvancePacket& packet)
{
    return raw.getInt32(packet.levelIndex) &&
           raw.getInt32(packet.result) &&
           raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerRemovePacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putInt32(packet.levelIndex);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerRemovePacket& packet)
{
    return raw.getInt32(packet.playerId) &&
           raw.getInt32(packet.levelIndex) &&
           raw.fullyRead();
}

template<class T>
inline string BuildAppPacket(NetAppMessageType type, const T& packet)
{
    NetworkMessageRaw raw;
    EncodeAppPacketRaw(raw, packet);
    return BuildAppRawMessage(type, raw);
}

template<class T>
inline bool ParseAppPacket(const char* buffer, __int32 bufferSize, NetAppMessageType expectedType, T& packet)
{
    if (!buffer || bufferSize < (__int32)sizeof(NetAppMessageHeader))
    {
        return false;
    }
    const NetAppMessageHeader* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (header->type != expectedType || !ValidAppMessageType(header->type))
    {
        return false;
    }
    const __int32 payloadSize = bufferSize - (__int32)sizeof(NetAppMessageHeader);
    if (payloadSize < 0 || (size_t)payloadSize > NET_MAX_ENCRYPTED_BYTES)
    {
        return false;
    }
    NetworkMessageRaw raw(buffer + sizeof(NetAppMessageHeader), payloadSize);
    return DecodeAppPacketRaw(raw, packet);
}

class cCryptoSession
{
    bool _hasKeypair;
    bool _ready;
    unsigned char _publicKey[crypto_kx_PUBLICKEYBYTES];
    unsigned char _secretKey[crypto_kx_SECRETKEYBYTES];
    unsigned char _rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char _tx[crypto_kx_SESSIONKEYBYTES];

public:
    cCryptoSession()
        : _hasKeypair(false),
          _ready(false)
    {
        memset(_publicKey, 0, sizeof(_publicKey));
        memset(_secretKey, 0, sizeof(_secretKey));
        memset(_rx, 0, sizeof(_rx));
        memset(_tx, 0, sizeof(_tx));
    }

    bool ready() const { return _ready; }

    void clear()
    {
        _hasKeypair = false;
        _ready = false;
        sodium_memzero(_publicKey, sizeof(_publicKey));
        sodium_memzero(_secretKey, sizeof(_secretKey));
        sodium_memzero(_rx, sizeof(_rx));
        sodium_memzero(_tx, sizeof(_tx));
    }

    bool buildClientHello(string& payload)
    {
        if (crypto_kx_keypair(_publicKey, _secretKey) != 0)
        {
            return false;
        }
        _hasKeypair = true;
        _ready = false;
        payload.assign(reinterpret_cast<const char*>(_publicKey), sizeof(_publicKey));
        return true;
    }

    bool acceptServerKey(const string& payload)
    {
        if (!_hasKeypair || payload.size() != crypto_kx_PUBLICKEYBYTES)
        {
            return false;
        }
        const unsigned char* serverPublic = reinterpret_cast<const unsigned char*>(payload.data());
        if (crypto_kx_client_session_keys(_rx, _tx, _publicKey, _secretKey, serverPublic) != 0)
        {
            clear();
            return false;
        }
        _ready = true;
        return true;
    }

    bool acceptClientHello(const string& payload, string& response)
    {
        if (payload.size() != crypto_kx_PUBLICKEYBYTES)
        {
            return false;
        }
        if (crypto_kx_keypair(_publicKey, _secretKey) != 0)
        {
            return false;
        }
        _hasKeypair = true;
        const unsigned char* clientPublic = reinterpret_cast<const unsigned char*>(payload.data());
        if (crypto_kx_server_session_keys(_rx, _tx, _publicKey, _secretKey, clientPublic) != 0)
        {
            clear();
            return false;
        }
        _ready = true;
        response.assign(reinterpret_cast<const char*>(_publicKey), sizeof(_publicKey));
        return true;
    }

    bool encrypt(const string& plain, string& payload)
    {
        if (!_ready)
        {
            return false;
        }
        unsigned char nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
        randombytes_buf(nonce, sizeof(nonce));

        payload.resize(sizeof(NetAppMessageHeader) + sizeof(nonce) + plain.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES);
        NetAppMessageHeader header;
        header.type = NAMTEncrypted;
        memcpy(&payload[0], &header, sizeof(header));
        memcpy(&payload[sizeof(header)], nonce, sizeof(nonce));

        unsigned long long cipherBytes = 0;
        if (crypto_aead_xchacha20poly1305_ietf_encrypt(
                reinterpret_cast<unsigned char*>(&payload[sizeof(header) + sizeof(nonce)]), &cipherBytes,
                reinterpret_cast<const unsigned char*>(plain.data()), plain.size(),
                NULL, 0, NULL, nonce, _tx) != 0)
        {
            payload.clear();
            return false;
        }
        payload.resize(sizeof(header) + sizeof(nonce) + (size_t)cipherBytes);
        return true;
    }

    bool decrypt(const char* buffer, __int32 bufferSize, string& plain)
    {
        if (!_ready ||
            bufferSize < (__int32)(sizeof(NetAppMessageHeader) + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES + crypto_aead_xchacha20poly1305_ietf_ABYTES) ||
            (size_t)bufferSize > NET_MAX_ENCRYPTED_BYTES)
        {
            return false;
        }
        const NetAppMessageHeader* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
        if (header->type != NAMTEncrypted)
        {
            return false;
        }
        const unsigned char* nonce = reinterpret_cast<const unsigned char*>(buffer + sizeof(NetAppMessageHeader));
        const unsigned char* cipher = reinterpret_cast<const unsigned char*>(buffer + sizeof(NetAppMessageHeader) + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
        const __int32 cipherSize = bufferSize - (__int32)(sizeof(NetAppMessageHeader) + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
        plain.resize(cipherSize);

        unsigned long long plainBytes = 0;
        if (crypto_aead_xchacha20poly1305_ietf_decrypt(
                reinterpret_cast<unsigned char*>(&plain[0]), &plainBytes, NULL,
                cipher, cipherSize, NULL, 0, nonce, _rx) != 0)
        {
            plain.clear();
            return false;
        }
        plain.resize((size_t)plainBytes);
        return true;
    }
};

inline string BuildVoicePayload(const NetworkVoicePacket& packet)
{
    NetworkMessageRaw raw;
    raw.putInt32(packet.playerId);
    raw.putUInt32(packet.sequence);
    raw.putUInt8(packet.codec);
    raw.putUInt16(packet.sampleRate);
    raw.putUInt16(packet.frameSamples);
    raw.putBytes(packet.data, VOICE_MAX_OPUS_BYTES);
    return BuildAppRawMessage(NAMTVoice, raw);
}

inline bool ParseVoicePacket(const char* buffer, __int32 bufferSize, NetworkVoicePacket& packet)
{
    packet = NetworkVoicePacket();
    if (!buffer || bufferSize < (__int32)sizeof(NetAppMessageHeader))
    {
        return false;
    }
    const NetAppMessageHeader* header = reinterpret_cast<const NetAppMessageHeader*>(buffer);
    if (header->type != NAMTVoice)
    {
        return false;
    }

    const __int32 payloadSize = bufferSize - (__int32)sizeof(NetAppMessageHeader);
    if (payloadSize < 0 || (size_t)payloadSize > NET_MAX_ENCRYPTED_BYTES)
    {
        return false;
    }

    NetworkMessageRaw raw(buffer + sizeof(NetAppMessageHeader), payloadSize);
    return raw.getInt32(packet.playerId) &&
           raw.getUInt32(packet.sequence) &&
           raw.getUInt8(packet.codec) &&
           raw.getUInt16(packet.sampleRate) &&
           raw.getUInt16(packet.frameSamples) &&
           raw.getBytes(packet.data, VOICE_MAX_OPUS_BYTES) &&
           raw.fullyRead();
}
