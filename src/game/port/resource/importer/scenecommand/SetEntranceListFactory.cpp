#include "port/resource/importer/scenecommand/SetEntranceListFactory.h"
#include "port/resource/type/scenecommand/SetEntranceList.h"

namespace SOH {
std::shared_ptr<Engine::IResource> SetEntranceListFactory::ReadResource(std::shared_ptr<Engine::ResourceInitData> initData,
                                                                      std::shared_ptr<Engine::BinaryReader> reader) {
    auto setEntranceList = std::make_shared<SetEntranceList>(initData);

    ReadCommandId(setEntranceList, reader);

    setEntranceList->numEntrances = reader->ReadUInt32();
    setEntranceList->entrances.reserve(setEntranceList->numEntrances);
    for (uint32_t i = 0; i < setEntranceList->numEntrances; i++) {
        EntranceEntry entranceEntry;

        entranceEntry.spawn = reader->ReadInt8();
        entranceEntry.room = reader->ReadInt8();

        setEntranceList->entrances.push_back(entranceEntry);
    }

    

    return setEntranceList;
}

} // namespace SOH
