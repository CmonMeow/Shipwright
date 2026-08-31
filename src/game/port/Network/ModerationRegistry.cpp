#include "ModerationRegistry.h"
#include "NetworkProtocol.h"

#include <algorithm>
#include <utility>

namespace SoH::Network {

ModerationRegistry::ModerationRegistry(std::string banFile, std::string gameMasterFile)
    : mBanFile(std::move(banFile)), mGameMasterFile(std::move(gameMasterFile)) {
}

void ModerationRegistry::Load() {
    mBannedIdentities.clear();
    mGameMasterIdentities.clear();
    if (!mBanFile.empty()) LoadIdentityList(mBanFile.c_str(), mBannedIdentities);
    if (!mGameMasterFile.empty()) LoadIdentityList(mGameMasterFile.c_str(), mGameMasterIdentities);
}

bool ModerationRegistry::Contains(const std::vector<std::string>& identities,
                                  const std::string& identity) {
    const std::string clean = TrimWhitespace(identity);
    return !clean.empty() && std::any_of(identities.begin(), identities.end(), [&clean](const auto& value) {
        return _stricmp(value.c_str(), clean.c_str()) == 0;
    });
}

bool ModerationRegistry::Remove(std::vector<std::string>& identities, const std::string& identity,
                                std::string* removedIdentity) {
    const std::string clean = TrimWhitespace(identity);
    const auto found = std::find_if(identities.begin(), identities.end(), [&clean](const auto& value) {
        return _stricmp(value.c_str(), clean.c_str()) == 0;
    });
    if (clean.empty() || found == identities.end()) return false;
    if (removedIdentity) *removedIdentity = *found;
    identities.erase(found);
    return true;
}

bool ModerationRegistry::IsBanned(const std::string& identity) const {
    return Contains(mBannedIdentities, identity);
}

bool ModerationRegistry::IsGameMaster(const std::string& identity) const {
    return Contains(mGameMasterIdentities, identity);
}

bool ModerationRegistry::Ban(const std::string& identity) {
    const std::string clean = SanitiseIdentityText(identity, 64);
    if (clean.empty() || Contains(mBannedIdentities, clean)) return false;
    mBannedIdentities.push_back(clean);
    SaveBans();
    return true;
}

bool ModerationRegistry::GrantGameMaster(const std::string& identity) {
    const std::string clean = SanitiseIdentityText(identity, 64);
    if (clean.empty() || Contains(mGameMasterIdentities, clean)) return false;
    mGameMasterIdentities.push_back(clean);
    SaveGameMasters();
    return true;
}

bool ModerationRegistry::MigrateGameMasterIdentity(
    const std::string& legacyIdentity,
    const std::string& authenticatedIdentity) {
    const std::string legacy = SanitiseIdentityText(legacyIdentity, 64);
    const std::string authenticated =
        SanitiseIdentityText(authenticatedIdentity, 64);
    if (legacy.empty() || authenticated.empty() ||
        !Contains(mGameMasterIdentities, legacy)) {
        return false;
    }
    Remove(mGameMasterIdentities, legacy, nullptr);
    AddUniqueString(mGameMasterIdentities, authenticated);
    SaveGameMasters();
    return true;
}

bool ModerationRegistry::RevokeGameMaster(const std::string& identity,
                                          std::string* removedIdentity) {
    if (!Remove(mGameMasterIdentities, identity, removedIdentity)) return false;
    SaveGameMasters();
    return true;
}

bool ModerationRegistry::Unban(const std::string& identity, std::string* removedIdentity) {
    if (!Remove(mBannedIdentities, identity, removedIdentity)) return false;
    SaveBans();
    return true;
}

void ModerationRegistry::SaveBans() const {
    if (!mBanFile.empty()) SaveIdentityList(mBanFile.c_str(), mBannedIdentities);
}

void ModerationRegistry::SaveGameMasters() const {
    if (!mGameMasterFile.empty()) SaveIdentityList(mGameMasterFile.c_str(), mGameMasterIdentities);
}

} // namespace SoH::Network
