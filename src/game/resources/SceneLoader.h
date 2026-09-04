#pragma once

#include "z64.h"

#ifdef __cplusplus
namespace Game::Resources {
class Scene;

int32_t ExecuteSceneCommands(PlayState* play, Scene* scene);
}

extern "C" {
#endif

void SceneLoader_SpawnScene(PlayState* play, int32_t sceneId, int32_t spawn);
int32_t SceneLoader_RequestRoom(PlayState* play, RoomContext* roomContext, int32_t roomNumber);
int32_t SceneLoader_FinalizeRoom(PlayState* play, RoomContext* roomContext);

#ifdef __cplusplus
}
#endif
