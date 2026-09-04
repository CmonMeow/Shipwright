#include "LocalNetworkIdentity.h"

#include "NetworkProtocol.h"

#include <Windows.h>
#include <sodium.h>

#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>

namespace Game::Multiplayer {

namespace {

constexpr char kIdentityDirectory[] = "GamePlatform";
constexpr char kIdentityFilename[] = "player_identity.key";
constexpr std::array<unsigned char, 8> kIdentityMagic{
    'G', 'P', 'I', 'D', 'K', 'E', 'Y', 1
};

struct LocalSigningIdentity {
    std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> publicKey{};
    std::array<unsigned char, crypto_sign_SECRETKEYBYTES> secretKey{};
    bool ready = false;

    ~LocalSigningIdentity() {
        sodium_memzero(secretKey.data(), secretKey.size());
    }
};

LocalSigningIdentity gSigningIdentity;
std::mutex gSigningIdentityMutex;

std::string IdentityFilePath() {
    char localAppData[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableA(
        "LOCALAPPDATA", localAppData, static_cast<DWORD>(sizeof(localAppData)));
    if (length == 0 || length >= sizeof(localAppData)) return {};
    std::string directory(localAppData, length);
    directory += "\\";
    directory += kIdentityDirectory;
    if (!CreateDirectoryA(directory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return {};
    }
    return directory + "\\" + kIdentityFilename;
}

bool ReadIdentityFile(const std::string& path, LocalSigningIdentity& identity) {
    const HANDLE file = CreateFileA(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    std::array<unsigned char,
               kIdentityMagic.size() + crypto_sign_PUBLICKEYBYTES +
                   crypto_sign_SECRETKEYBYTES> bytes{};
    DWORD read = 0;
    const bool success = ReadFile(file, bytes.data(),
                                  static_cast<DWORD>(bytes.size()), &read,
                                  nullptr) != FALSE;
    CloseHandle(file);
    if (!success || read != bytes.size() ||
        !std::equal(kIdentityMagic.begin(), kIdentityMagic.end(),
                    bytes.begin())) {
        return false;
    }

    std::copy_n(bytes.begin() + kIdentityMagic.size(),
                identity.publicKey.size(), identity.publicKey.begin());
    std::copy_n(bytes.begin() + kIdentityMagic.size() +
                    identity.publicKey.size(),
                identity.secretKey.size(), identity.secretKey.begin());
    std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> derived{};
    if (crypto_sign_ed25519_sk_to_pk(derived.data(),
                                     identity.secretKey.data()) != 0 ||
        sodium_memcmp(derived.data(), identity.publicKey.data(),
                      derived.size()) != 0) {
        sodium_memzero(identity.secretKey.data(), identity.secretKey.size());
        return false;
    }
    identity.ready = true;
    return true;
}

bool CreateIdentityFile(const std::string& path,
                        LocalSigningIdentity& identity) {
    if (crypto_sign_keypair(identity.publicKey.data(),
                            identity.secretKey.data()) != 0) {
        return false;
    }
    std::array<unsigned char,
               kIdentityMagic.size() + crypto_sign_PUBLICKEYBYTES +
                   crypto_sign_SECRETKEYBYTES> bytes{};
    std::copy(kIdentityMagic.begin(), kIdentityMagic.end(), bytes.begin());
    std::copy(identity.publicKey.begin(), identity.publicKey.end(),
              bytes.begin() + kIdentityMagic.size());
    std::copy(identity.secretKey.begin(), identity.secretKey.end(),
              bytes.begin() + kIdentityMagic.size() +
                  identity.publicKey.size());

    const HANDLE file = CreateFileA(
        path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        sodium_memzero(identity.secretKey.data(), identity.secretKey.size());
        return false;
    }
    DWORD written = 0;
    const bool success =
        WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                  &written, nullptr) != FALSE &&
        written == bytes.size() && FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (!success) {
        DeleteFileA(path.c_str());
        sodium_memzero(identity.secretKey.data(), identity.secretKey.size());
        return false;
    }
    identity.ready = true;
    return true;
}

bool EnsureLocalSigningIdentity() {
    std::lock_guard lock(gSigningIdentityMutex);
    if (gSigningIdentity.ready) return true;
    if (sodium_init() < 0) return false;
    const std::string path = IdentityFilePath();
    if (path.empty()) return false;
    if (ReadIdentityFile(path, gSigningIdentity)) return true;
    if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    if (CreateIdentityFile(path, gSigningIdentity)) return true;

    // Another process may have won CREATE_NEW while several local clients
    // started together. Wait only for that bounded file publication.
    for (int attempt = 0; attempt < 20; ++attempt) {
        Sleep(5);
        if (ReadIdentityFile(path, gSigningIdentity)) return true;
    }
    return false;
}

} // namespace

std::string LocalMachineSerialId() {
    DWORD serial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr,
                          nullptr, 0);
    std::ostringstream value;
    value << serial;
    return value.str();
}

std::string IdentityIdFromPublicKey(const std::string& publicKey) {
    if (publicKey.size() != crypto_sign_PUBLICKEYBYTES) return {};
    std::array<char, crypto_sign_PUBLICKEYBYTES * 2 + 1> hex{};
    sodium_bin2hex(hex.data(), hex.size(),
                   reinterpret_cast<const unsigned char*>(publicKey.data()),
                   publicKey.size());
    return std::string(hex.data(), hex.size() - 1);
}

bool SignIdentityBinding(const std::string& secretKey,
                         const std::string& binding,
                         std::string& signature) {
    signature.clear();
    if (secretKey.size() != crypto_sign_SECRETKEYBYTES || binding.empty()) {
        return false;
    }
    signature.resize(crypto_sign_BYTES);
    unsigned long long signatureBytes = 0;
    if (crypto_sign_detached(
            reinterpret_cast<unsigned char*>(signature.data()),
            &signatureBytes,
            reinterpret_cast<const unsigned char*>(binding.data()),
            binding.size(),
            reinterpret_cast<const unsigned char*>(secretKey.data())) != 0 ||
        signatureBytes != crypto_sign_BYTES) {
        signature.clear();
        return false;
    }
    return true;
}

bool VerifyIdentityBinding(const std::string& publicKey,
                           const std::string& binding,
                           const std::string& signature) {
    return publicKey.size() == crypto_sign_PUBLICKEYBYTES &&
           signature.size() == crypto_sign_BYTES && !binding.empty() &&
           crypto_sign_verify_detached(
               reinterpret_cast<const unsigned char*>(signature.data()),
               reinterpret_cast<const unsigned char*>(binding.data()),
               binding.size(),
               reinterpret_cast<const unsigned char*>(publicKey.data())) == 0;
}

bool LocalIdentityPublicKey(std::string& publicKey) {
    publicKey.clear();
    if (!EnsureLocalSigningIdentity()) return false;
    std::lock_guard lock(gSigningIdentityMutex);
    publicKey.assign(
        reinterpret_cast<const char*>(gSigningIdentity.publicKey.data()),
        gSigningIdentity.publicKey.size());
    return true;
}

bool SignLocalIdentityBinding(const std::string& binding,
                              std::string& signature) {
    signature.clear();
    if (binding.empty() || !EnsureLocalSigningIdentity()) return false;
    std::lock_guard lock(gSigningIdentityMutex);
    signature.resize(crypto_sign_BYTES);
    unsigned long long signatureBytes = 0;
    if (crypto_sign_detached(
            reinterpret_cast<unsigned char*>(signature.data()),
            &signatureBytes,
            reinterpret_cast<const unsigned char*>(binding.data()),
            binding.size(), gSigningIdentity.secretKey.data()) != 0 ||
        signatureBytes != crypto_sign_BYTES) {
        signature.clear();
        return false;
    }
    return true;
}

std::string LocalIdentityId() {
    std::string publicKey;
    return LocalIdentityPublicKey(publicKey)
               ? IdentityIdFromPublicKey(publicKey)
               : std::string();
}

std::string LocalUserName() {
    char buffer[256]{};
    DWORD size = sizeof(buffer);
    if (GetUserNameA(buffer, &size) && size > 1) {
        return SanitiseIdentityText(buffer, 48);
    }
    return "Anon";
}

std::string LocalExecutableName() {
    char path[MAX_PATH]{};
    if (!GetModuleFileNameA(nullptr, path, sizeof(path))) {
        return "GameClient.exe";
    }
    const char* slash = std::strrchr(path, '\\');
    const char* forwardSlash = std::strrchr(path, '/');
    if (forwardSlash && (!slash || forwardSlash > slash)) {
        slash = forwardSlash;
    }
    return slash ? std::string(slash + 1) : std::string(path);
}

void LoadIdentityList(const char* filename, std::vector<std::string>& list) {
    list.clear();
    if (!filename || !*filename) return;
    std::ifstream file(filename);
    if (!file.good()) return;

    std::string token;
    char character = 0;
    while (file.get(character)) {
        if (std::isspace(static_cast<unsigned char>(character)) ||
            character == ',' || character == ';') {
            AddUniqueString(list, SanitiseIdentityText(token, 64));
            token.clear();
        } else if (token.size() < 255) {
            token.push_back(character);
        }
    }
    AddUniqueString(list, SanitiseIdentityText(token, 64));
}

void SaveIdentityList(const char* filename,
                      const std::vector<std::string>& list) {
    if (!filename || !*filename) return;
    std::ofstream file(filename, std::ios::out | std::ios::trunc);
    if (!file.good()) return;
    for (const std::string& identity : list) {
        if (!identity.empty()) file << identity << "\r\n";
    }
}

void LoadBanList(std::vector<std::string>& list) {
    LoadIdentityList(BAN_LIST_FILENAME, list);
}

void SaveBanList(const std::vector<std::string>& list) {
    SaveIdentityList(BAN_LIST_FILENAME, list);
}

void LoadGameMasterList(std::vector<std::string>& list) {
    LoadIdentityList(GM_LIST_FILENAME, list);
}

void SaveGameMasterList(const std::vector<std::string>& list) {
    SaveIdentityList(GM_LIST_FILENAME, list);
}

bool EncodeLocalIdentityRaw(NetworkMessageRaw& raw,
                            const std::string& sessionBinding) {
    std::string publicKey;
    std::string signature;
    if (!LocalIdentityPublicKey(publicKey) ||
        !SignLocalIdentityBinding(sessionBinding, signature)) {
        return false;
    }
    raw.putInt32(APP_PROTOCOL_VERSION);
    raw.putString(publicKey, crypto_sign_PUBLICKEYBYTES);
    raw.putString(LocalUserName(), 48);
    raw.putUInt8(_stricmp(LocalExecutableName().c_str(), "VoiceClient.exe") == 0
                     ? 1
                     : 0);
    raw.putString(signature, crypto_sign_BYTES);
    return true;
}

} // namespace Game::Multiplayer
