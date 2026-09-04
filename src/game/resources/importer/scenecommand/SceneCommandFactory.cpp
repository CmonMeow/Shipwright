#include "resources/importer/scenecommand/SceneCommandFactory.h"
#include "resources/type/scenecommand/SceneCommand.h"
namespace Game::Resources {
void SceneCommandFactoryBinaryV0::ReadCommandId(std::shared_ptr<ISceneCommand> command,
                                                std::shared_ptr<Engine::BinaryReader> reader) {
    command->cmdId = (SceneCommandID)reader->ReadInt32();
}
} // namespace Game::Resources
