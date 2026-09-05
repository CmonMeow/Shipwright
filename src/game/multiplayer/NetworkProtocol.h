#pragma once

#include "NetworkVersion.h"

#include "multiplayer/netTransport.hpp"
#include "sodium.h"
#include <algorithm>
#include <cstddef>
#include <cctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

inline constexpr const char* DEFAULT_NETWORK_ADDRESS = "127.0.0.1";
inline constexpr unsigned short DEFAULT_NETWORK_PORT = 777;
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
inline constexpr size_t NET_MAX_STRATEGIC_SITES = 4096;
inline constexpr size_t NET_MAX_SUPPLY_ROUTES = 8192;
inline constexpr size_t NET_MAX_INFLUENCE_ADJACENCIES = 8192;
inline constexpr unsigned __int64 NET_CLIENT_INTENT_MS = 16;
inline constexpr unsigned __int64 NET_PLAYER_SYNC_MS = 50;
inline constexpr unsigned char NETWORK_TEAM_NEUTRAL = 0;
inline constexpr unsigned char NETWORK_TEAM_RED = 1;
inline constexpr unsigned char NETWORK_TEAM_BLUE = 2;
inline constexpr unsigned char NETWORK_TEAM_GREEN = 3;
inline constexpr unsigned char NETWORK_STRUCTURE_ACTION_BUILD = 1;
inline constexpr unsigned char NETWORK_STRUCTURE_ACTION_REPAIR = 2;

struct NetworkIdentity
{
    __int32 protocolVersion = 0;
    bool voiceClient = false;
    bool authenticated = false;
    string id;
    string name;
    string publicKey;
    string signature;
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
    void putInt16(short value) { put(&value, sizeof(value)); }
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
    bool getInt16(short& value) { return get(&value, sizeof(value)); }
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
    switch (type)
    {
    case NAMTConnect:
    case NAMTChat:
    case NAMTPlayerAssign:
    case NAMTPlayerSnapshot:
    case NAMTPlayerLifecycle:
    case NAMTVoice:
    case NAMTKeyHello:
    case NAMTKeyAccept:
    case NAMTProjectileState:
    case NAMTPlayerIntent:
    case NAMTChatKey:
    case NAMTPrivateChat:
    case NAMTCombatResult:
    case NAMTPlayerRespawn:
    case NAMTFishingState:
    case NAMTObjectiveState:
    case NAMTStructureState:
    case NAMTStructureAction:
    case NAMTCorpseState:
    case NAMTArrowFireIntent:
    case NAMTSceneEntryIntent:
    case NAMTSceneEntryState:
    case NAMTFishIntent:
    case NAMTFishState:
    case NAMTLureControlIntent:
    case NAMTLureState:
    case NAMTProjectileLifecycle:
    case NAMTWeaponSelectionIntent:
    case NAMTProjectileIntentResult:
    case NAMTStrategicTopology:
    case NAMTEncrypted:
        return true;
    default:
        return false;
    }
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
    string publicKey;
    string name;
    string signature;
    unsigned char voiceClient = 0;
    if (!raw.getInt32(version) ||
        !raw.getString(publicKey, crypto_sign_PUBLICKEYBYTES) ||
        !raw.getString(name, 48) ||
        !raw.getUInt8(voiceClient) ||
        !raw.getString(signature, crypto_sign_BYTES))
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
    identity.name = SanitiseIdentityText(name, 48);
    identity.publicKey = std::move(publicKey);
    identity.signature = std::move(signature);
    if (identity.name.empty())
    {
        identity.name = "Anon";
    }
    return identity.publicKey.size() == crypto_sign_PUBLICKEYBYTES &&
           identity.signature.size() == crypto_sign_BYTES;
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
    string publicKey;
    string name;
    string signature;
    unsigned char voiceClient = 0;
    if (!raw.getInt32(identity.protocolVersion) ||
        !raw.getString(publicKey, crypto_sign_PUBLICKEYBYTES) ||
        !raw.getString(name, 48) ||
        !raw.getUInt8(voiceClient) ||
        !raw.getString(signature, crypto_sign_BYTES))
    {
        return false;
    }
    if (!raw.fullyRead())
    {
        return false;
    }

    identity.voiceClient = voiceClient != 0;
    identity.name = SanitiseIdentityText(name, 48);
    identity.publicKey = std::move(publicKey);
    identity.signature = std::move(signature);
    if (identity.name.empty())
    {
        identity.name = "Anon";
    }
    return identity.publicKey.size() == crypto_sign_PUBLICKEYBYTES &&
           identity.signature.size() == crypto_sign_BYTES;
}

#pragma pack(push, networkPlayerPackets, 1)
enum
{
    NETWORK_FISHING_LINE_POINT_COUNT = 200
};

struct NetworkPlayerAssignPacket
{
    __int32 playerId;
};

struct NetworkFishingPresentationPacket
{
    __int32 playerId;
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    __int32 sceneId;
    unsigned __int32 lifeEpoch;
    __int32 sequence;
    unsigned char fishingState;
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
    unsigned char fishingLineSpooled;
    unsigned char fishingSinkingLureSegmentIndex;
    unsigned char fishingSinkingLureUnderwater;
    short fishingFishRot[3];
    short fishingFishLimbRot[8];
};

// Client-to-server cosmetic telemetry. Ownership and player entity identity
// are intentionally absent; the authenticated server session supplies them.
struct NetworkFishingPresentationIntentPacket
{
    __int32 sequence;
    unsigned __int32 lifeEpoch;
    unsigned char fishingState;
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
    unsigned char fishingLineSpooled;
    unsigned char fishingSinkingLureSegmentIndex;
    unsigned char fishingSinkingLureUnderwater;
    short fishingFishRot[3];
    short fishingFishLimbRot[8];
};

enum
{
    NETWORK_FISH_INTENT_HOOK = 1,
    NETWORK_FISH_INTENT_RELEASE = 2
};

enum
{
    NETWORK_LURE_DEPLOYED = 1 << 0,
    NETWORK_LURE_REEL_HELD = 1 << 1,
    NETWORK_LURE_CONTROL_MASK = NETWORK_LURE_DEPLOYED | NETWORK_LURE_REEL_HELD
};

struct NetworkFishIntentPacket
{
    unsigned __int32 sequence;
    unsigned __int32 lifeEpoch;
    unsigned char action;
};

static_assert(sizeof(NetworkFishIntentPacket) == 9);

struct NetworkFishStatePacket
{
    __int32 ownerPlayerId;
    unsigned __int32 ownerLifeEpoch;
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    unsigned __int32 sequence;
    __int32 sceneId;
    unsigned __int32 spawnKey;
    float x;
    float y;
    float z;
    unsigned char species;
    float length;
    unsigned char active;
};

struct NetworkLureControlIntentPacket
{
    unsigned __int32 sequence;
    unsigned __int32 lifeEpoch;
    unsigned char controlFlags;
};

static_assert(sizeof(NetworkLureControlIntentPacket) == 9);

struct NetworkLureStatePacket
{
    __int32 ownerPlayerId;
    unsigned __int32 ownerLifeEpoch;
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    unsigned __int32 sequence;
    __int32 sceneId;
    float x;
    float y;
    float z;
    unsigned char phase;
    unsigned char lureType;
    unsigned char active;
};

struct NetworkProjectileStatePacket
{
    __int32 playerId;
    __int32 projectileId;
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
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
    int32_t bodyPlayerId = -1;
    uint32_t bodyLifeEpoch = 0;
    uint8_t bodyRegion = 0;
    float bodyOffsetX = 0.0f;
    float bodyOffsetY = 0.0f;
    float bodyOffsetZ = 0.0f;
    float bodyDirectionX = 0.0f;
    float bodyDirectionY = 0.0f;
    float bodyDirectionZ = 0.0f;
};

struct NetworkProjectileLifecyclePacket
{
    __int32 playerId;
    __int32 projectileId;
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    __int32 sceneId;
    unsigned char projectileKind;
    unsigned char active;
};

struct NetworkArrowFireIntentPacket
{
    unsigned __int32 sequence;
    unsigned __int32 lifeEpoch;
    unsigned __int32 clientTick;
    short heading;
    short aimPitch;
};

static_assert(sizeof(NetworkArrowFireIntentPacket) == 16);

enum
{
    NETWORK_PROJECTILE_INTENT_ARROW_FIRE = 1
};

struct NetworkProjectileIntentResultPacket
{
    unsigned __int32 sequence;
    unsigned __int32 lifeEpoch;
    __int32 projectileId;
    unsigned char intentKind;
    unsigned char accepted;
};

static_assert(sizeof(NetworkProjectileIntentResultPacket) == 14);

enum
{
    NETWORK_PROJECTILE_ARROW = 0,
    NETWORK_ARROW_FLYING = 0,
    NETWORK_ARROW_STUCK = 1,
    NETWORK_ARROW_BLOCKED = 2
};

struct NetworkPlayerCommandPacket
{
    unsigned __int32 sequence;
    unsigned __int32 actionSequence;
    unsigned __int32 lifeEpoch;
    unsigned __int32 clientTick;
    signed char moveX;
    signed char moveY;
    short heading;
    short aimPitch;
    unsigned short heldActions;
    unsigned short pressedActions;
    unsigned char meleeAttackVariant;
    unsigned char hasMeleeAttackVariant;
    float x;
    float y;
    float z;
    unsigned char locomotionMode;
    unsigned char hasPose;
};

static_assert(sizeof(NetworkPlayerCommandPacket) == 42);

struct NetworkWeaponSelectionIntentPacket
{
    unsigned __int32 sequence;
    unsigned __int32 lifeEpoch;
    unsigned char selectedWeapon;
};

static_assert(sizeof(NetworkWeaponSelectionIntentPacket) == 9);

struct NetworkSceneEntryIntentPacket
{
    unsigned __int32 sequence;
    unsigned __int32 lifeEpoch;
};

static_assert(sizeof(NetworkSceneEntryIntentPacket) == 8);

struct NetworkSceneEntryStatePacket
{
    __int32 playerId;
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    unsigned __int32 requestSequence;
    unsigned __int32 lifeEpoch;
    __int32 sceneId;
    float x;
    float y;
    float z;
    short heading;
    unsigned char accepted;
};

struct NetworkPlayerSnapshotPacket
{
    __int32 playerId;
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    __int32 sceneId;
    unsigned __int32 serverTick;
    unsigned __int32 lastProcessedCommand;
    unsigned __int32 lifeEpoch;
    float x;
    float y;
    float z;
    float velocityX;
    float velocityY;
    float velocityZ;
    short heading;
    short aimPitch;
    unsigned short heldActions;
    unsigned char selectedWeapon;
    unsigned char actionState;
    unsigned char meleeAttackVariant;
    unsigned __int32 meleeAttackId;
    unsigned __int32 actionStartTick;
    unsigned char health;
    unsigned char team;
    unsigned char locomotionMode;
    float locomotionPhaseRadians;
};

struct NetworkObjectiveStatePacket
{
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    unsigned __int32 sequence;
    __int32 objectiveKey;
    __int32 sceneId;
    float x;
    float y;
    float z;
    float captureRadius;
    float captureProgress;
    unsigned char active;
    unsigned char ownerTeam;
    unsigned char capturingTeam;
    unsigned char contested;
};

struct NetworkStructureStatePacket
{
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    unsigned __int32 sequence;
    __int32 structureKey;
    __int32 objectiveKey;
    __int32 sceneId;
    float x;
    float y;
    float z;
    unsigned __int32 health;
    unsigned __int32 maximumHealth;
    unsigned __int32 buildProgress;
    unsigned __int32 requiredBuild;
    unsigned char active;
    unsigned char team;
    unsigned char phase;
};

struct NetworkStructureActionPacket
{
    unsigned __int32 sequence;
    unsigned __int32 lifeEpoch;
    __int32 structureKey;
    unsigned char action;
};

static_assert(sizeof(NetworkStructureActionPacket) == 13);

struct NetworkCorpseStatePacket
{
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    unsigned __int32 sequence;
    __int32 sourcePlayerId;
    unsigned __int32 sourcePlayerEntityIndex;
    unsigned __int32 sourcePlayerEntityGeneration;
    unsigned __int32 sourceLifeEpoch;
    __int32 sceneId;
    __int32 roomId;
    float x;
    float y;
    float z;
    short rotation[3];
    unsigned char selectedWeapon;
    unsigned char active;
};

enum
{
    NETWORK_ACTION_PRIMARY = 1 << 0,
    NETWORK_ACTION_BLOCK = 1 << 1,
    NETWORK_ACTION_AIM = 1 << 2,
    NETWORK_ACTION_EVADE = 1 << 3
};

enum
{
    NETWORK_PLAYER_ACTION_IDLE = 0,
    NETWORK_PLAYER_ACTION_PRIMARY_WINDUP = 1,
    NETWORK_PLAYER_ACTION_PRIMARY_ACTIVE = 2,
    NETWORK_PLAYER_ACTION_PRIMARY_RECOVERY = 3,
    NETWORK_PLAYER_ACTION_BLOCKING = 4,
    NETWORK_PLAYER_ACTION_AIMING = 5,
    NETWORK_PLAYER_ACTION_EVADING = 6,
    NETWORK_PLAYER_ACTION_JUMP_SLASHING = 7,
    NETWORK_PLAYER_ACTION_SPIN_ATTACKING = 8
};

struct NetworkPlayerLifecyclePacket
{
    __int32 playerId;
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    unsigned __int32 lifeEpoch;
    __int32 sceneId;
    unsigned char active;
};

enum
{
    NETWORK_COMBAT_MELEE = 0,
    NETWORK_COMBAT_ARROW = 1,
    NETWORK_COMBAT_EXPLOSION = 2,
    NETWORK_COMBAT_ENVIRONMENT = 3,
    NETWORK_COMBAT_DAMAGED = 0,
    NETWORK_COMBAT_BLOCKED = 1
};

struct NetworkCombatResultPacket
{
    unsigned __int32 eventId;
    __int32 sourcePlayerId;
    __int32 targetPlayerId;
    unsigned __int32 sourceEntityIndex;
    unsigned __int32 sourceEntityGeneration;
    unsigned __int32 targetEntityIndex;
    unsigned __int32 targetEntityGeneration;
    unsigned __int32 sourceLifeEpoch;
    unsigned __int32 targetLifeEpoch;
    unsigned __int32 meleeAttackId;
    __int32 sceneId;
    unsigned char attackKind;
    unsigned char result;
    unsigned char damage;
    unsigned char hitRegion;
    short impactYaw;
    float impactX;
    float impactY;
    float impactZ;
};

struct NetworkPlayerRespawnPacket
{
    __int32 playerId;
    unsigned __int32 entityIndex;
    unsigned __int32 entityGeneration;
    unsigned __int32 lifeEpoch;
    __int32 sceneId;
    unsigned __int32 serverTick;
    float x;
    float y;
    float z;
    short heading;
    unsigned char selectedWeapon;
};

enum
{
    VOICE_SAMPLE_RATE = 16000,
    VOICE_SAMPLES_PER_PACKET = 320,
    VOICE_MAX_OPUS_BYTES = 1275,
    VOICE_CODEC_OPUS = 1
};

#pragma pack(pop, networkPlayerPackets)

struct NetworkStrategicSiteRecord
{
    __int32 objectiveKey;
    __int32 influenceRegionKey;
    unsigned char kind;
};

struct NetworkSupplyRouteRecord
{
    __int32 routeKey;
    __int32 sourceObjectiveKey;
    __int32 destinationObjectiveKey;
};

struct NetworkInfluenceAdjacencyRecord
{
    __int32 adjacencyKey;
    __int32 lowerRegionKey;
    __int32 upperRegionKey;
};

struct NetworkStrategicTopologyPacket
{
    unsigned __int32 revision;
    vector<NetworkStrategicSiteRecord> sites;
    vector<NetworkSupplyRouteRecord> supplyRoutes;
    vector<NetworkInfluenceAdjacencyRecord> influenceAdjacencies;
};

struct NetworkVoicePacket
{
    __int32 playerId;
    unsigned __int32 sequence;
    unsigned char codec;
    unsigned short sampleRate;
    unsigned short frameSamples;
    vector<unsigned char> data;
};

struct NetworkVoiceIntentPacket
{
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

template <typename Packet>
inline void EncodeFishingPresentationBody(NetworkMessageRaw& raw, const Packet& packet)
{
    raw.putUInt8(packet.fishingState);
    for (unsigned char axis = 0; axis < 3; ++axis)
    {
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
    raw.putUInt8(packet.fishingLineSpooled);
    raw.putUInt8(packet.fishingSinkingLureSegmentIndex);
    raw.putUInt8(packet.fishingSinkingLureUnderwater);
    for (unsigned char axis = 0; axis < 3; ++axis)
        raw.putUInt16(static_cast<unsigned short>(packet.fishingFishRot[axis]));
    for (unsigned char limb = 0; limb < 8; ++limb)
        raw.putUInt16(static_cast<unsigned short>(packet.fishingFishLimbRot[limb]));
}

inline void EncodeFishingStateRaw(NetworkMessageRaw& raw,
                                  const NetworkFishingPresentationPacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putInt32(packet.sceneId);
    raw.putUInt32(packet.lifeEpoch);
    raw.putInt32(packet.sequence);
    EncodeFishingPresentationBody(raw, packet);
}

inline void EncodeFishingIntentRaw(NetworkMessageRaw& raw,
                                   const NetworkFishingPresentationIntentPacket& packet)
{
    raw.putInt32(packet.sequence);
    raw.putUInt32(packet.lifeEpoch);
    EncodeFishingPresentationBody(raw, packet);
}

template <typename Packet>
inline bool DecodeFishingPresentationBody(NetworkMessageRaw& raw, Packet& packet)
{
    if (!raw.getUInt8(packet.fishingState))
        return false;
    for (unsigned char axis = 0; axis < 3; ++axis)
        if (!raw.getFloat(packet.fishingLureDrawOffset[axis]))
            return false;
    if (!raw.getFloat(packet.fishingRodBendY) || !raw.getFloat(packet.fishingRodBendX) ||
        !raw.getFloat(packet.fishingRodTwist) || !raw.getFloat(packet.fishingRodCastX))
        return false;
    for (unsigned char axis = 0; axis < 3; ++axis)
        if (!raw.getFloat(packet.fishingLureRot[axis]))
            return false;
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
        !raw.getUInt8(packet.fishingLineSpooled) ||
        !raw.getUInt8(packet.fishingSinkingLureSegmentIndex) ||
        !raw.getUInt8(packet.fishingSinkingLureUnderwater))
        return false;
    for (unsigned char axis = 0; axis < 3; ++axis)
    {
        unsigned short value = 0;
        if (!raw.getUInt16(value))
            return false;
        packet.fishingFishRot[axis] = static_cast<short>(value);
    }
    for (unsigned char limb = 0; limb < 8; ++limb)
    {
        unsigned short value = 0;
        if (!raw.getUInt16(value))
            return false;
        packet.fishingFishLimbRot[limb] = static_cast<short>(value);
    }
    return raw.fullyRead();
}

inline bool DecodeFishingStateRaw(NetworkMessageRaw& raw,
                                  NetworkFishingPresentationPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    return raw.getInt32(packet.playerId) && raw.getUInt32(packet.entityIndex) &&
           raw.getUInt32(packet.entityGeneration) && raw.getInt32(packet.sceneId) &&
           raw.getUInt32(packet.lifeEpoch) && raw.getInt32(packet.sequence) &&
           DecodeFishingPresentationBody(raw, packet);
}

inline bool DecodeFishingIntentRaw(NetworkMessageRaw& raw,
                                   NetworkFishingPresentationIntentPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    return raw.getInt32(packet.sequence) && raw.getUInt32(packet.lifeEpoch) &&
           DecodeFishingPresentationBody(raw, packet);
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkFishIntentPacket& packet)
{
    raw.putUInt32(packet.sequence);
    raw.putUInt32(packet.lifeEpoch);
    raw.putUInt8(packet.action);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkFishIntentPacket& packet)
{
    return raw.getUInt32(packet.sequence) && raw.getUInt32(packet.lifeEpoch) &&
           raw.getUInt8(packet.action) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkFishStatePacket& packet)
{
    raw.putInt32(packet.ownerPlayerId);
    raw.putUInt32(packet.ownerLifeEpoch);
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putUInt32(packet.sequence);
    raw.putInt32(packet.sceneId);
    raw.putUInt32(packet.spawnKey);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putUInt8(packet.species);
    raw.putFloat(packet.length);
    raw.putUInt8(packet.active);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkFishStatePacket& packet)
{
    return raw.getInt32(packet.ownerPlayerId) && raw.getUInt32(packet.ownerLifeEpoch) &&
           raw.getUInt32(packet.entityIndex) &&
           raw.getUInt32(packet.entityGeneration) && raw.getUInt32(packet.sequence) &&
           raw.getInt32(packet.sceneId) && raw.getUInt32(packet.spawnKey) &&
           raw.getFloat(packet.x) && raw.getFloat(packet.y) &&
           raw.getFloat(packet.z) && raw.getUInt8(packet.species) && raw.getFloat(packet.length) &&
           raw.getUInt8(packet.active) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkLureControlIntentPacket& packet)
{
    raw.putUInt32(packet.sequence);
    raw.putUInt32(packet.lifeEpoch);
    raw.putUInt8(packet.controlFlags);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkLureControlIntentPacket& packet)
{
    return raw.getUInt32(packet.sequence) && raw.getUInt32(packet.lifeEpoch) &&
           raw.getUInt8(packet.controlFlags) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkLureStatePacket& packet)
{
    raw.putInt32(packet.ownerPlayerId);
    raw.putUInt32(packet.ownerLifeEpoch);
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putUInt32(packet.sequence);
    raw.putInt32(packet.sceneId);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putUInt8(packet.phase);
    raw.putUInt8(packet.lureType);
    raw.putUInt8(packet.active);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkLureStatePacket& packet)
{
    return raw.getInt32(packet.ownerPlayerId) && raw.getUInt32(packet.ownerLifeEpoch) &&
           raw.getUInt32(packet.entityIndex) &&
           raw.getUInt32(packet.entityGeneration) && raw.getUInt32(packet.sequence) &&
           raw.getInt32(packet.sceneId) && raw.getFloat(packet.x) && raw.getFloat(packet.y) &&
           raw.getFloat(packet.z) && raw.getUInt8(packet.phase) && raw.getUInt8(packet.lureType) &&
           raw.getUInt8(packet.active) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkProjectileStatePacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putInt32(packet.projectileId);
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
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
    raw.putInt32(packet.bodyPlayerId);
    raw.putUInt32(packet.bodyLifeEpoch);
    raw.putUInt8(packet.bodyRegion);
    raw.putFloat(packet.bodyOffsetX);
    raw.putFloat(packet.bodyOffsetY);
    raw.putFloat(packet.bodyOffsetZ);
    raw.putFloat(packet.bodyDirectionX);
    raw.putFloat(packet.bodyDirectionY);
    raw.putFloat(packet.bodyDirectionZ);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkProjectileStatePacket& packet)
{
    unsigned short rotationX = 0;
    unsigned short rotationY = 0;
    unsigned short rotationZ = 0;
    if (!raw.getInt32(packet.playerId) || !raw.getInt32(packet.projectileId) ||
        !raw.getUInt32(packet.entityIndex) || !raw.getUInt32(packet.entityGeneration) ||
        !raw.getInt32(packet.sceneId) || !raw.getUInt32(packet.sequence) ||
        !raw.getUInt8(packet.active) || !raw.getUInt8(packet.projectileKind) || !raw.getUInt8(packet.phase) ||
        !raw.getUInt8(packet.projectileType) || !raw.getFloat(packet.x) ||
        !raw.getFloat(packet.y) || !raw.getFloat(packet.z) || !raw.getUInt16(rotationX) ||
        !raw.getUInt16(rotationY) || !raw.getUInt16(rotationZ) || !raw.getFloat(packet.velocityX) ||
        !raw.getFloat(packet.velocityY) || !raw.getFloat(packet.velocityZ) ||
        !raw.getInt32(packet.bodyPlayerId) || !raw.getUInt32(packet.bodyLifeEpoch) ||
        !raw.getUInt8(packet.bodyRegion) || !raw.getFloat(packet.bodyOffsetX) ||
        !raw.getFloat(packet.bodyOffsetY) || !raw.getFloat(packet.bodyOffsetZ) ||
        !raw.getFloat(packet.bodyDirectionX) || !raw.getFloat(packet.bodyDirectionY) ||
        !raw.getFloat(packet.bodyDirectionZ) || !raw.fullyRead())
    {
        return false;
    }
    packet.rotationX = static_cast<short>(rotationX);
    packet.rotationY = static_cast<short>(rotationY);
    packet.rotationZ = static_cast<short>(rotationZ);
    return true;
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkProjectileLifecyclePacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putInt32(packet.projectileId);
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putInt32(packet.sceneId);
    raw.putUInt8(packet.projectileKind);
    raw.putUInt8(packet.active);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkProjectileLifecyclePacket& packet)
{
    return raw.getInt32(packet.playerId) && raw.getInt32(packet.projectileId) &&
           raw.getUInt32(packet.entityIndex) && raw.getUInt32(packet.entityGeneration) &&
           raw.getInt32(packet.sceneId) && raw.getUInt8(packet.projectileKind) &&
           raw.getUInt8(packet.active) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkArrowFireIntentPacket& packet)
{
    raw.putUInt32(packet.sequence);
    raw.putUInt32(packet.lifeEpoch);
    raw.putUInt32(packet.clientTick);
    raw.putInt16(packet.heading);
    raw.putInt16(packet.aimPitch);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkArrowFireIntentPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    return raw.getUInt32(packet.sequence) && raw.getUInt32(packet.lifeEpoch) &&
           raw.getUInt32(packet.clientTick) &&
           raw.getInt16(packet.heading) && raw.getInt16(packet.aimPitch) &&
           raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw,
                               const NetworkProjectileIntentResultPacket& packet)
{
    raw.putUInt32(packet.sequence);
    raw.putUInt32(packet.lifeEpoch);
    raw.putInt32(packet.projectileId);
    raw.putUInt8(packet.intentKind);
    raw.putUInt8(packet.accepted);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw,
                               NetworkProjectileIntentResultPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    return raw.getUInt32(packet.sequence) && raw.getUInt32(packet.lifeEpoch) &&
           raw.getInt32(packet.projectileId) && raw.getUInt8(packet.intentKind) &&
           raw.getUInt8(packet.accepted) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkCombatResultPacket& packet)
{
    raw.putUInt32(packet.eventId);
    raw.putInt32(packet.sourcePlayerId);
    raw.putInt32(packet.targetPlayerId);
    raw.putUInt32(packet.sourceEntityIndex);
    raw.putUInt32(packet.sourceEntityGeneration);
    raw.putUInt32(packet.targetEntityIndex);
    raw.putUInt32(packet.targetEntityGeneration);
    raw.putUInt32(packet.sourceLifeEpoch);
    raw.putUInt32(packet.targetLifeEpoch);
    raw.putUInt32(packet.meleeAttackId);
    raw.putInt32(packet.sceneId);
    raw.putUInt8(packet.attackKind);
    raw.putUInt8(packet.result);
    raw.putUInt8(packet.damage);
    raw.putUInt8(packet.hitRegion);
    raw.putUInt16(static_cast<unsigned short>(packet.impactYaw));
    raw.putFloat(packet.impactX);
    raw.putFloat(packet.impactY);
    raw.putFloat(packet.impactZ);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkCombatResultPacket& packet)
{
    unsigned short impactYaw = 0;
    if (!raw.getUInt32(packet.eventId) || !raw.getInt32(packet.sourcePlayerId) ||
        !raw.getInt32(packet.targetPlayerId) ||
        !raw.getUInt32(packet.sourceEntityIndex) || !raw.getUInt32(packet.sourceEntityGeneration) ||
        !raw.getUInt32(packet.targetEntityIndex) || !raw.getUInt32(packet.targetEntityGeneration) ||
        !raw.getUInt32(packet.sourceLifeEpoch) || !raw.getUInt32(packet.targetLifeEpoch) ||
        !raw.getUInt32(packet.meleeAttackId) || !raw.getInt32(packet.sceneId) ||
        !raw.getUInt8(packet.attackKind) ||
        !raw.getUInt8(packet.result) || !raw.getUInt8(packet.damage) ||
        !raw.getUInt8(packet.hitRegion) ||
        !raw.getUInt16(impactYaw) || !raw.getFloat(packet.impactX) ||
        !raw.getFloat(packet.impactY) || !raw.getFloat(packet.impactZ) ||
        !raw.fullyRead())
        return false;
    packet.impactYaw = static_cast<short>(impactYaw);
    return true;
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerRespawnPacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putUInt32(packet.lifeEpoch);
    raw.putInt32(packet.sceneId);
    raw.putUInt32(packet.serverTick);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putUInt16(static_cast<unsigned short>(packet.heading));
    raw.putUInt8(packet.selectedWeapon);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerRespawnPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    unsigned short heading = 0;
    if (!raw.getInt32(packet.playerId) || !raw.getUInt32(packet.entityIndex) ||
        !raw.getUInt32(packet.entityGeneration) || !raw.getUInt32(packet.lifeEpoch) ||
        !raw.getInt32(packet.sceneId) || !raw.getUInt32(packet.serverTick) ||
        !raw.getFloat(packet.x) || !raw.getFloat(packet.y) ||
        !raw.getFloat(packet.z) || !raw.getUInt16(heading) ||
        !raw.getUInt8(packet.selectedWeapon) || !raw.fullyRead()) {
        return false;
    }
    packet.heading = static_cast<short>(heading);
    return true;
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerCommandPacket& packet)
{
    raw.putUInt32(packet.sequence);
    raw.putUInt32(packet.actionSequence);
    raw.putUInt32(packet.lifeEpoch);
    raw.putUInt32(packet.clientTick);
    raw.putUInt8(static_cast<unsigned char>(packet.moveX));
    raw.putUInt8(static_cast<unsigned char>(packet.moveY));
    raw.putUInt16(static_cast<unsigned short>(packet.heading));
    raw.putUInt16(static_cast<unsigned short>(packet.aimPitch));
    raw.putUInt16(packet.heldActions);
    raw.putUInt16(packet.pressedActions);
    raw.putUInt8(packet.meleeAttackVariant);
    raw.putUInt8(packet.hasMeleeAttackVariant);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putUInt8(packet.locomotionMode);
    raw.putUInt8(packet.hasPose);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerCommandPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    unsigned char moveX = 0;
    unsigned char moveY = 0;
    unsigned short heading = 0;
    unsigned short aimPitch = 0;
    if (!raw.getUInt32(packet.sequence) ||
        !raw.getUInt32(packet.actionSequence) ||
        !raw.getUInt32(packet.lifeEpoch) ||
        !raw.getUInt32(packet.clientTick) ||
        !raw.getUInt8(moveX) ||
        !raw.getUInt8(moveY) ||
        !raw.getUInt16(heading) ||
        !raw.getUInt16(aimPitch) ||
        !raw.getUInt16(packet.heldActions) ||
        !raw.getUInt16(packet.pressedActions) ||
        !raw.getUInt8(packet.meleeAttackVariant) ||
        !raw.getUInt8(packet.hasMeleeAttackVariant) ||
        !raw.getFloat(packet.x) ||
        !raw.getFloat(packet.y) ||
        !raw.getFloat(packet.z) ||
        !raw.getUInt8(packet.locomotionMode) ||
        !raw.getUInt8(packet.hasPose))
    {
        return false;
    }
    if (!raw.fullyRead()) return false;
    packet.moveX = static_cast<signed char>(moveX);
    packet.moveY = static_cast<signed char>(moveY);
    packet.heading = static_cast<short>(heading);
    packet.aimPitch = static_cast<short>(aimPitch);
    return true;
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw,
                               const NetworkWeaponSelectionIntentPacket& packet)
{
    raw.putUInt32(packet.sequence);
    raw.putUInt32(packet.lifeEpoch);
    raw.putUInt8(packet.selectedWeapon);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw,
                               NetworkWeaponSelectionIntentPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    return raw.getUInt32(packet.sequence) &&
           raw.getUInt32(packet.lifeEpoch) && raw.getUInt8(packet.selectedWeapon) &&
           raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkSceneEntryIntentPacket& packet)
{
    raw.putUInt32(packet.sequence);
    raw.putUInt32(packet.lifeEpoch);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkSceneEntryIntentPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    return raw.getUInt32(packet.sequence) &&
           raw.getUInt32(packet.lifeEpoch) &&
           raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkSceneEntryStatePacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putUInt32(packet.requestSequence);
    raw.putUInt32(packet.lifeEpoch);
    raw.putInt32(packet.sceneId);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putUInt16(static_cast<unsigned short>(packet.heading));
    raw.putUInt8(packet.accepted);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkSceneEntryStatePacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    unsigned short heading = 0;
    if (!raw.getInt32(packet.playerId) || !raw.getUInt32(packet.entityIndex) ||
        !raw.getUInt32(packet.entityGeneration) || !raw.getUInt32(packet.requestSequence) ||
        !raw.getUInt32(packet.lifeEpoch) ||
        !raw.getInt32(packet.sceneId) || !raw.getFloat(packet.x) || !raw.getFloat(packet.y) ||
        !raw.getFloat(packet.z) || !raw.getUInt16(heading) || !raw.getUInt8(packet.accepted) ||
        !raw.fullyRead()) return false;
    packet.heading = static_cast<short>(heading);
    return true;
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerSnapshotPacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putInt32(packet.sceneId);
    raw.putUInt32(packet.serverTick);
    raw.putUInt32(packet.lastProcessedCommand);
    raw.putUInt32(packet.lifeEpoch);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putFloat(packet.velocityX);
    raw.putFloat(packet.velocityY);
    raw.putFloat(packet.velocityZ);
    raw.putUInt16(static_cast<unsigned short>(packet.heading));
    raw.putUInt16(static_cast<unsigned short>(packet.aimPitch));
    raw.putUInt16(packet.heldActions);
    raw.putUInt8(packet.selectedWeapon);
    raw.putUInt8(packet.actionState);
    raw.putUInt8(packet.meleeAttackVariant);
    raw.putUInt32(packet.meleeAttackId);
    raw.putUInt32(packet.actionStartTick);
    raw.putUInt8(packet.health);
    raw.putUInt8(packet.team);
    raw.putUInt8(packet.locomotionMode);
    raw.putFloat(packet.locomotionPhaseRadians);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerSnapshotPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    unsigned short heading = 0;
    unsigned short aimPitch = 0;
    if (!raw.getInt32(packet.playerId) ||
        !raw.getUInt32(packet.entityIndex) ||
        !raw.getUInt32(packet.entityGeneration) ||
        !raw.getInt32(packet.sceneId) ||
        !raw.getUInt32(packet.serverTick) ||
        !raw.getUInt32(packet.lastProcessedCommand) ||
        !raw.getUInt32(packet.lifeEpoch) ||
        !raw.getFloat(packet.x) ||
        !raw.getFloat(packet.y) ||
        !raw.getFloat(packet.z) ||
        !raw.getFloat(packet.velocityX) ||
        !raw.getFloat(packet.velocityY) ||
        !raw.getFloat(packet.velocityZ) ||
        !raw.getUInt16(heading) ||
        !raw.getUInt16(aimPitch) ||
        !raw.getUInt16(packet.heldActions) ||
        !raw.getUInt8(packet.selectedWeapon) ||
        !raw.getUInt8(packet.actionState) ||
        !raw.getUInt8(packet.meleeAttackVariant) ||
        !raw.getUInt32(packet.meleeAttackId) ||
        !raw.getUInt32(packet.actionStartTick) ||
        !raw.getUInt8(packet.health) ||
        !raw.getUInt8(packet.team) ||
        !raw.getUInt8(packet.locomotionMode) ||
        !raw.getFloat(packet.locomotionPhaseRadians))
    {
        return false;
    }
    if (!raw.fullyRead()) return false;
    packet.heading = static_cast<short>(heading);
    packet.aimPitch = static_cast<short>(aimPitch);
    return true;
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkObjectiveStatePacket& packet)
{
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putUInt32(packet.sequence);
    raw.putInt32(packet.objectiveKey);
    raw.putInt32(packet.sceneId);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putFloat(packet.captureRadius);
    raw.putFloat(packet.captureProgress);
    raw.putUInt8(packet.active);
    raw.putUInt8(packet.ownerTeam);
    raw.putUInt8(packet.capturingTeam);
    raw.putUInt8(packet.contested);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkObjectiveStatePacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    return raw.getUInt32(packet.entityIndex) &&
           raw.getUInt32(packet.entityGeneration) &&
           raw.getUInt32(packet.sequence) &&
           raw.getInt32(packet.objectiveKey) &&
           raw.getInt32(packet.sceneId) &&
           raw.getFloat(packet.x) &&
           raw.getFloat(packet.y) &&
           raw.getFloat(packet.z) &&
           raw.getFloat(packet.captureRadius) &&
           raw.getFloat(packet.captureProgress) &&
           raw.getUInt8(packet.active) &&
           raw.getUInt8(packet.ownerTeam) &&
           raw.getUInt8(packet.capturingTeam) &&
           raw.getUInt8(packet.contested) &&
           raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw,
                               const NetworkStrategicTopologyPacket& packet)
{
    raw.putUInt32(packet.revision);
    raw.putUInt32(static_cast<unsigned __int32>(packet.sites.size()));
    raw.putUInt32(static_cast<unsigned __int32>(packet.supplyRoutes.size()));
    raw.putUInt32(static_cast<unsigned __int32>(packet.influenceAdjacencies.size()));
    for (const auto& site : packet.sites)
    {
        raw.putInt32(site.objectiveKey);
        raw.putInt32(site.influenceRegionKey);
        raw.putUInt8(site.kind);
    }
    for (const auto& route : packet.supplyRoutes)
    {
        raw.putInt32(route.routeKey);
        raw.putInt32(route.sourceObjectiveKey);
        raw.putInt32(route.destinationObjectiveKey);
    }
    for (const auto& adjacency : packet.influenceAdjacencies)
    {
        raw.putInt32(adjacency.adjacencyKey);
        raw.putInt32(adjacency.lowerRegionKey);
        raw.putInt32(adjacency.upperRegionKey);
    }
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw,
                               NetworkStrategicTopologyPacket& packet)
{
    packet = {};
    unsigned __int32 siteCount = 0;
    unsigned __int32 routeCount = 0;
    unsigned __int32 adjacencyCount = 0;
    if (!raw.getUInt32(packet.revision) || !raw.getUInt32(siteCount) ||
        !raw.getUInt32(routeCount) || !raw.getUInt32(adjacencyCount) ||
        siteCount > NET_MAX_STRATEGIC_SITES ||
        routeCount > NET_MAX_SUPPLY_ROUTES ||
        adjacencyCount > NET_MAX_INFLUENCE_ADJACENCIES)
    {
        return false;
    }
    packet.sites.resize(siteCount);
    for (auto& site : packet.sites)
    {
        if (!raw.getInt32(site.objectiveKey) ||
            !raw.getInt32(site.influenceRegionKey) || !raw.getUInt8(site.kind))
        {
            return false;
        }
    }
    packet.supplyRoutes.resize(routeCount);
    for (auto& route : packet.supplyRoutes)
    {
        if (!raw.getInt32(route.routeKey) ||
            !raw.getInt32(route.sourceObjectiveKey) ||
            !raw.getInt32(route.destinationObjectiveKey))
        {
            return false;
        }
    }
    packet.influenceAdjacencies.resize(adjacencyCount);
    for (auto& adjacency : packet.influenceAdjacencies)
    {
        if (!raw.getInt32(adjacency.adjacencyKey) ||
            !raw.getInt32(adjacency.lowerRegionKey) ||
            !raw.getInt32(adjacency.upperRegionKey))
        {
            return false;
        }
    }
    return raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkStructureStatePacket& packet)
{
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putUInt32(packet.sequence);
    raw.putInt32(packet.structureKey);
    raw.putInt32(packet.objectiveKey);
    raw.putInt32(packet.sceneId);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    raw.putUInt32(packet.health);
    raw.putUInt32(packet.maximumHealth);
    raw.putUInt32(packet.buildProgress);
    raw.putUInt32(packet.requiredBuild);
    raw.putUInt8(packet.active);
    raw.putUInt8(packet.team);
    raw.putUInt8(packet.phase);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkStructureStatePacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    return raw.getUInt32(packet.entityIndex) &&
           raw.getUInt32(packet.entityGeneration) &&
           raw.getUInt32(packet.sequence) &&
           raw.getInt32(packet.structureKey) &&
           raw.getInt32(packet.objectiveKey) &&
           raw.getInt32(packet.sceneId) &&
           raw.getFloat(packet.x) &&
           raw.getFloat(packet.y) &&
           raw.getFloat(packet.z) &&
           raw.getUInt32(packet.health) &&
           raw.getUInt32(packet.maximumHealth) &&
           raw.getUInt32(packet.buildProgress) &&
           raw.getUInt32(packet.requiredBuild) &&
           raw.getUInt8(packet.active) &&
           raw.getUInt8(packet.team) &&
           raw.getUInt8(packet.phase) &&
           raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkStructureActionPacket& packet)
{
    raw.putUInt32(packet.sequence);
    raw.putUInt32(packet.lifeEpoch);
    raw.putInt32(packet.structureKey);
    raw.putUInt8(packet.action);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkStructureActionPacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    return raw.getUInt32(packet.sequence) && raw.getUInt32(packet.lifeEpoch) &&
           raw.getInt32(packet.structureKey) &&
           raw.getUInt8(packet.action) &&
           raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkCorpseStatePacket& packet)
{
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putUInt32(packet.sequence);
    raw.putInt32(packet.sourcePlayerId);
    raw.putUInt32(packet.sourcePlayerEntityIndex);
    raw.putUInt32(packet.sourcePlayerEntityGeneration);
    raw.putUInt32(packet.sourceLifeEpoch);
    raw.putInt32(packet.sceneId);
    raw.putInt32(packet.roomId);
    raw.putFloat(packet.x);
    raw.putFloat(packet.y);
    raw.putFloat(packet.z);
    for (size_t axis = 0; axis < 3; ++axis) raw.putUInt16(static_cast<unsigned short>(packet.rotation[axis]));
    raw.putUInt8(packet.selectedWeapon);
    raw.putUInt8(packet.active);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkCorpseStatePacket& packet)
{
    memset(&packet, 0, sizeof(packet));
    const auto readShort = [&raw](short& destination) {
        unsigned short value = 0;
        if (!raw.getUInt16(value)) return false;
        destination = static_cast<short>(value);
        return true;
    };
    if (!raw.getUInt32(packet.entityIndex) || !raw.getUInt32(packet.entityGeneration) ||
        !raw.getUInt32(packet.sequence) ||
        !raw.getInt32(packet.sourcePlayerId) ||
        !raw.getUInt32(packet.sourcePlayerEntityIndex) ||
        !raw.getUInt32(packet.sourcePlayerEntityGeneration) ||
        !raw.getUInt32(packet.sourceLifeEpoch) ||
        !raw.getInt32(packet.sceneId) ||
        !raw.getInt32(packet.roomId) || !raw.getFloat(packet.x) || !raw.getFloat(packet.y) ||
        !raw.getFloat(packet.z)) return false;
    for (size_t axis = 0; axis < 3; ++axis) if (!readShort(packet.rotation[axis])) return false;
    if (!raw.getUInt8(packet.selectedWeapon)) return false;
    return raw.getUInt8(packet.active) && raw.fullyRead();
}

inline void EncodeAppPacketRaw(NetworkMessageRaw& raw, const NetworkPlayerLifecyclePacket& packet)
{
    raw.putInt32(packet.playerId);
    raw.putUInt32(packet.entityIndex);
    raw.putUInt32(packet.entityGeneration);
    raw.putUInt32(packet.lifeEpoch);
    raw.putInt32(packet.sceneId);
    raw.putUInt8(packet.active);
}

inline bool DecodeAppPacketRaw(NetworkMessageRaw& raw, NetworkPlayerLifecyclePacket& packet)
{
    return raw.getInt32(packet.playerId) &&
           raw.getUInt32(packet.entityIndex) &&
           raw.getUInt32(packet.entityGeneration) &&
           raw.getUInt32(packet.lifeEpoch) &&
           raw.getInt32(packet.sceneId) &&
           raw.getUInt8(packet.active) &&
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
    bool _serverRole;
    unsigned char _publicKey[crypto_kx_PUBLICKEYBYTES];
    unsigned char _peerPublicKey[crypto_kx_PUBLICKEYBYTES];
    unsigned char _secretKey[crypto_kx_SECRETKEYBYTES];
    unsigned char _rx[crypto_kx_SESSIONKEYBYTES];
    unsigned char _tx[crypto_kx_SESSIONKEYBYTES];

public:
    cCryptoSession()
        : _hasKeypair(false),
          _ready(false),
          _serverRole(false)
    {
        memset(_publicKey, 0, sizeof(_publicKey));
        memset(_peerPublicKey, 0, sizeof(_peerPublicKey));
        memset(_secretKey, 0, sizeof(_secretKey));
        memset(_rx, 0, sizeof(_rx));
        memset(_tx, 0, sizeof(_tx));
    }

    bool ready() const { return _ready; }

    void clear()
    {
        _hasKeypair = false;
        _ready = false;
        _serverRole = false;
        sodium_memzero(_publicKey, sizeof(_publicKey));
        sodium_memzero(_peerPublicKey, sizeof(_peerPublicKey));
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
        _serverRole = false;
        sodium_memzero(_peerPublicKey, sizeof(_peerPublicKey));
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
        memcpy(_peerPublicKey, serverPublic, sizeof(_peerPublicKey));
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
        _serverRole = true;
        const unsigned char* clientPublic = reinterpret_cast<const unsigned char*>(payload.data());
        if (crypto_kx_server_session_keys(_rx, _tx, _publicKey, _secretKey, clientPublic) != 0)
        {
            clear();
            return false;
        }
        memcpy(_peerPublicKey, clientPublic, sizeof(_peerPublicKey));
        _ready = true;
        response.assign(reinterpret_cast<const char*>(_publicKey), sizeof(_publicKey));
        return true;
    }

    string identityBinding() const
    {
        if (!_ready)
        {
            return {};
        }
        static constexpr char domain[] = "GamePlatform identity proof v1";
        string binding(domain, sizeof(domain) - 1);
        const unsigned char* clientPublic = _serverRole ? _peerPublicKey : _publicKey;
        const unsigned char* serverPublic = _serverRole ? _publicKey : _peerPublicKey;
        binding.append(reinterpret_cast<const char*>(clientPublic), crypto_kx_PUBLICKEYBYTES);
        binding.append(reinterpret_cast<const char*>(serverPublic), crypto_kx_PUBLICKEYBYTES);
        return binding;
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

inline string BuildVoiceIntentPayload(const NetworkVoiceIntentPacket& packet)
{
    NetworkMessageRaw raw;
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

inline bool ParseVoiceIntentPacket(const char* buffer, __int32 bufferSize,
                                   NetworkVoiceIntentPacket& packet)
{
    packet = NetworkVoiceIntentPacket();
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
    return raw.getUInt32(packet.sequence) && raw.getUInt8(packet.codec) &&
           raw.getUInt16(packet.sampleRate) && raw.getUInt16(packet.frameSamples) &&
           raw.getBytes(packet.data, VOICE_MAX_OPUS_BYTES) && raw.fullyRead();
}
