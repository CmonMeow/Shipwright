#pragma once

namespace Game::Resources {
enum class ResourceType {
    Array = 0x4F415252,           // OARR
    Animation = 0x4F414E4D,       // OANM
    PlayerAnimation = 0x4F50414D, // OPAM
    Room = 0x4F524F4D,            // OROM
    CollisionHeader = 0x4F434F4C, // OCOL
    Skeleton = 0x4F534B4C,        // OSKL
    SkeletonLimb = 0x4F534C42,    // OSLB
    // Archive compatibility only; the runtime no longer registers or loads it.
    Cutscene = 0x4F435654,        // OCUT
    Text = 0x4F545854,            // OTXT
    Audio = 0x4F415544,           // OAUD
    AudioSample = 0x4F534D50,     // OSMP
    AudioSoundFont = 0x4F534654,  // OSFT
    AudioSequence = 0x4F534551,   // OSEQ
    SceneCommand = 0x4F52434D,    // ORCM
};
} // namespace Game::Resources
