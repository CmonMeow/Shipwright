#pragma once

#include <string>
#include <vector>

class NetworkMessageRaw;

namespace Game::Multiplayer {

inline constexpr const char* BAN_LIST_FILENAME = "banlist.txt";
inline constexpr const char* GM_LIST_FILENAME = "gmlist.txt";

std::string LocalIdentityId();
std::string LocalMachineSerialId();
std::string LocalUserName();
std::string LocalExecutableName();

std::string IdentityIdFromPublicKey(const std::string& publicKey);
bool SignIdentityBinding(const std::string& secretKey,
                         const std::string& binding,
                         std::string& signature);
bool VerifyIdentityBinding(const std::string& publicKey,
                           const std::string& binding,
                           const std::string& signature);
bool LocalIdentityPublicKey(std::string& publicKey);
bool SignLocalIdentityBinding(const std::string& binding,
                              std::string& signature);

void LoadIdentityList(const char* filename, std::vector<std::string>& list);
void SaveIdentityList(const char* filename,
                      const std::vector<std::string>& list);
void LoadBanList(std::vector<std::string>& list);
void SaveBanList(const std::vector<std::string>& list);
void LoadGameMasterList(std::vector<std::string>& list);
void SaveGameMasterList(const std::vector<std::string>& list);

bool EncodeLocalIdentityRaw(NetworkMessageRaw& raw,
                            const std::string& sessionBinding);

} // namespace Game::Multiplayer
