#include "port/resource/importer/TextFactory.h"
#include "port/resource/type/Text.h"

namespace SOH {
std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryTextV0::ReadResource(std::shared_ptr<Engine::File> file,
                                          std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto text = std::make_shared<Text>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);

    uint32_t msgCount = reader->ReadUInt32();
    text->messages.reserve(msgCount);

    for (uint32_t i = 0; i < msgCount; i++) {
        MessageEntry entry;
        entry.id = reader->ReadUInt16();
        entry.textboxType = reader->ReadUByte();
        entry.textboxYPos = reader->ReadUByte();
        entry.msg = reader->ReadString();

        text->messages.push_back(entry);
    }

    return text;
}

} // namespace SOH
