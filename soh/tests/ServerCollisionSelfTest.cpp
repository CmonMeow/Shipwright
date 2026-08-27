#include "Network/ServerCollisionWorld.h"

#include <sysdef.h>

namespace {
constexpr int32_t kTest01SceneId = 0x65;
}

int main() {
    SoH::Network::ServerCollisionWorld collision;
    if (!collision.LoadDefaultArchive()) {
        Error("Dedicated collision self-test: oot.o2r could not be loaded");
        return 1;
    }
    if (collision.SceneCount() < 102 || collision.TriangleCount() < 1000 || collision.WildFishCount() < 20 ||
        !collision.ValidateLoadedGeometry() || !collision.ValidateSceneGeometry(kTest01SceneId)) {
        Error("Dedicated collision self-test failed: scenes=%zu triangles=%zu wildFish=%zu", collision.SceneCount(),
              collision.TriangleCount(), collision.WildFishCount());
        return 1;
    }
    Error("Dedicated collision self-test passed: scenes=%zu triangles=%zu wildFish=%zu", collision.SceneCount(),
          collision.TriangleCount(), collision.WildFishCount());
    return 0;
}
