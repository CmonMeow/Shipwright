#pragma once

#include "LocalNetworkIdentity.h"

#include <string>
#include <vector>

namespace Game::Multiplayer {

class ModerationRegistry final {
  public:
    explicit ModerationRegistry(std::string banFile = BAN_LIST_FILENAME,
                                std::string gameMasterFile = GM_LIST_FILENAME);

    void Load();
    bool IsBanned(const std::string& identity) const;
    bool IsGameMaster(const std::string& identity) const;
    bool Ban(const std::string& identity);
    bool GrantGameMaster(const std::string& identity);
    bool MigrateGameMasterIdentity(const std::string& legacyIdentity,
                                   const std::string& authenticatedIdentity);
    bool RevokeGameMaster(const std::string& identity, std::string* removedIdentity = nullptr);
    bool Unban(const std::string& identity, std::string* removedIdentity = nullptr);

    const std::vector<std::string>& Bans() const { return mBannedIdentities; }
    const std::vector<std::string>& GameMasters() const { return mGameMasterIdentities; }

  private:
    static bool Contains(const std::vector<std::string>& identities, const std::string& identity);
    static bool Remove(std::vector<std::string>& identities, const std::string& identity,
                       std::string* removedIdentity);
    void SaveBans() const;
    void SaveGameMasters() const;

    std::string mBanFile;
    std::string mGameMasterFile;
    std::vector<std::string> mBannedIdentities;
    std::vector<std::string> mGameMasterIdentities;
};

} // namespace Game::Multiplayer
