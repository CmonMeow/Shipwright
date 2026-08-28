#include "port/resource/importer/scenecommand/SceneCommandFactory.h"
#include "port/resource/type/scenecommand/SceneCommand.h"
namespace SOH {
void SceneCommandFactoryBinaryV0::ReadCommandId(std::shared_ptr<ISceneCommand> command,
                                                std::shared_ptr<Engine::BinaryReader> reader) {
    command->cmdId = (SceneCommandID)reader->ReadInt32();
}
} // namespace SOH
