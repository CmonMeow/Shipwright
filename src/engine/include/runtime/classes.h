#pragma once
#ifdef __cplusplus

#include "engine/resource/archive/ArchiveManager.h"
#include "engine/resource/archive/Archive.h"
#include "engine/resource/archive/O2rArchive.h"
#include "engine/resource/ResourceManager.h"
#include "engine/Context.h"
#include "engine/window/Window.h"
#include "engine/debug/Console.h"
#include "engine/debug/CrashHandler.h"
#include "engine/config/ConsoleVariable.h"
#include "engine/config/Config.h"
#include "engine/utils/binarytools/BinaryReader.h"
#include "engine/utils/binarytools/MemoryStream.h"
#include "engine/utils/binarytools/BinaryWriter.h"
#include "engine/audio/Audio.h"
#include "engine/audio/AudioPlayer.h"
#if defined(_WIN32)
#include "engine/audio/WasapiAudioPlayer.h"
#endif
#ifdef __APPLE__
#include "engine/utils/AppleFolderManager.h"
#endif
#endif
