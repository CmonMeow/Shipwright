#include "runtime/actors/ActorDB.h"

#include <algorithm>
#include <assert.h>

ActorDB* ActorDB::Instance;

#define DEFINE_ACTOR(name, _1, _2) extern "C" ActorInit name##_InitVars;
#define DEFINE_ACTOR_INTERNAL(name, _1, _2) extern "C" ActorInit name##_InitVars;
#include "tables/actor_table.h"
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL

struct AddPair {
    const char* name;
    const char* description;
    ActorInit& init;
};

#define DEFINE_ACTOR_INTERNAL(name, _1, allocType) { #name, #name, name##_InitVars },
#define DEFINE_ACTOR(name, _1, allocType) { #name, #name, name##_InitVars },
static constexpr AddPair initialActorTable[] = {
#include "tables/actor_table.h"
};
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR

static_assert(ACTOR_PLAYER == 0);
static_assert(ACTOR_EN_ARROW == 1);
static_assert(ACTOR_EN_FISH == 2);
static_assert(ACTOR_FISHING == 3);
static_assert(sizeof(initialActorTable) / sizeof(initialActorTable[0]) == ACTOR_ID_MAX);

ActorDB::ActorDB() {
    db.reserve(ACTOR_ID_MAX);
    for (const AddPair& pair : initialActorTable) {
        AddEntry(pair.name, pair.description, pair.init);
    }
}

ActorDB::Entry& ActorDB::AddEntry(const std::string& name, const std::string& desc,
                                  size_t index) {
    assert(!nameTable.contains(name));
    if (db.size() < index + 1) db.resize(index + 1);
    Entry& newEntry = db.at(index);
    newEntry.entry.id = static_cast<int32_t>(index);
    assert(!newEntry.entry.valid);
    nextFreeId = std::max(nextFreeId, index + 1);
    nameTable[name] = newEntry.entry.id;
    newEntry.SetName(name);
    newEntry.SetDesc(desc);
    newEntry.entry.valid = true;
    return newEntry;
}

ActorDB::Entry& ActorDB::AddEntry(const std::string& name, const std::string& desc,
                                  const ActorInit& init) {
    Entry& entry = AddEntry(name, desc, init.id);
    entry.entry.category = init.category;
    entry.entry.flags = init.flags;
    entry.entry.objectId = init.objectId;
    entry.entry.instanceSize = init.instanceSize;
    entry.entry.init = init.init;
    entry.entry.destroy = init.destroy;
    entry.entry.update = init.update;
    entry.entry.draw = init.draw;
    entry.entry.reset = init.reset;
    return entry;
}

ActorDB::Entry& ActorDB::AddEntry(const ActorDBInit& init) {
    Entry& entry = AddEntry(init.name, init.desc, nextFreeId);
    entry.entry.category = init.category;
    entry.entry.flags = init.flags;
    entry.entry.objectId = init.objectId;
    entry.entry.instanceSize = init.instanceSize;
    entry.entry.init = init.init;
    entry.entry.destroy = init.destroy;
    entry.entry.update = init.update;
    entry.entry.draw = init.draw;
    entry.entry.reset = init.reset;
    return entry;
}

ActorDB::Entry& ActorDB::RetrieveEntry(const int id) {
    static Entry invalid;
    if (id < 0 || static_cast<size_t>(id) >= db.size()) return invalid;
    return db[id];
}

int ActorDB::RetrieveId(const std::string& name) {
    const auto entry = nameTable.find(name);
    return entry == nameTable.end() ? -1 : entry->second;
}

int ActorDB::GetEntryCount() {
    return static_cast<int>(db.size());
}

ActorDB::Entry::Entry() {
    entry.name = nullptr;
    entry.desc = nullptr;
    entry.valid = false;
    entry.id = 0;
    entry.category = 0;
    entry.flags = 0;
    entry.objectId = 0;
    entry.instanceSize = 0;
    entry.init = nullptr;
    entry.destroy = nullptr;
    entry.update = nullptr;
    entry.draw = nullptr;
    entry.reset = nullptr;
    entry.numLoaded = 0;
}

ActorDB::Entry::Entry(const Entry& other) {
    entry = other.entry;
    SetName(other.name);
    SetDesc(other.desc);
}

ActorDB::Entry& ActorDB::Entry::operator=(const Entry& other) {
    entry = other.entry;
    SetName(other.name);
    SetDesc(other.desc);
    return *this;
}

void ActorDB::Entry::SetName(const std::string& newName) {
    name = newName;
    entry.name = name.c_str();
}

void ActorDB::Entry::SetDesc(const std::string& newDesc) {
    desc = newDesc;
    entry.desc = desc.c_str();
}

extern "C" ActorDBEntry* ActorDB_Retrieve(const int id) {
    return &ActorDB::Instance->RetrieveEntry(id).entry;
}

extern "C" int ActorDB_RetrieveId(const char* name) {
    return ActorDB::Instance->RetrieveId(name);
}
