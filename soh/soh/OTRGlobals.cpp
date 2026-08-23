#include "OTRGlobals.h"
#include "OTRAudio.h"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <vector>
#include <chrono>
#include <optional>

#include "ResourceManagerHelpers.h"
#include <fast/Fast3dWindow.h>
#include <ship/resource/File.h>
#include <ship/window/Window.h>
#include <soh/GameVersions.h>
#include <spdlog/sinks/rotating_file_sink.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif
#include <ship/audio/AudioPlayer.h>
#include "Enhancements/controls/SohInputEditorWindow.h"
#include "Enhancements/debugger/colViewer.h"
#include "frame_interpolation.h"
#include "SohGui/SohMenu.h"
#include "SohGui/SohGui.hpp"
#include "variables.h"
#include "z64.h"
#include "macros.h"
#include <ship/window/gui/Fonts.h>
#include <ship/window/FileDropMgr.h>
#include <ship/window/gui/resource/Font.h>
#include <ship/utils/StringHelper.h>
#include "util.h"

#if not defined(__SWITCH__) && not defined(__WIIU__)
#include "Extractor/Extract.h"
#endif

#include <fast/interpreter.h>

#ifdef __APPLE__
#include <SDL_scancode.h>
#else
#include <SDL2/SDL_scancode.h>
#endif

#ifdef __SWITCH__
#include <port/switch/SwitchImpl.h>
#elif defined(__WIIU__)
#include <port/wiiu/WiiUImpl.h>
#include <coreinit/debug.h> // OSFatal
#endif

#include <functions.h>
#include "soh/SohGui/ImGuiUtils.h"
#include "ActorDB.h"
#include "SaveManager.h"
#include <libultraship/libultraship.h>
#include <libultraship/controller/controldeck/ControlDeck.h>
#include <fast/resource/ResourceType.h>

// Resource Types/Factories
#include "soh/resource/type/Array.h"
#include <ship/resource/type/Blob.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Matrix.h>
#include <fast/resource/type/Texture.h>
#include <fast/resource/type/Vertex.h>
#include "soh/resource/type/SohResourceType.h"
#include "soh/resource/type/Animation.h"
#include "soh/resource/type/AudioSample.h"
#include "soh/resource/type/AudioSequence.h"
#include "soh/resource/type/AudioSoundFont.h"
#include "soh/resource/type/CollisionHeader.h"
#include "soh/resource/type/Cutscene.h"
#include "soh/resource/type/Path.h"
#include "soh/resource/type/PlayerAnimation.h"
#include "soh/resource/type/Scene.h"
#include "soh/resource/type/Skeleton.h"
#include "soh/resource/type/SkeletonLimb.h"
#include "soh/resource/type/Text.h"
#include <ship/resource/factory/BlobFactory.h>
#include <fast/resource/factory/DisplayListFactory.h>
#include <fast/resource/factory/MatrixFactory.h>
#include <fast/resource/factory/TextureFactory.h>
#include <fast/resource/factory/VertexFactory.h>
#include "soh/resource/importer/ArrayFactory.h"
#include "soh/resource/importer/AnimationFactory.h"
#include "soh/resource/importer/AudioSampleFactory.h"
#include "soh/resource/importer/AudioSequenceFactory.h"
#include "soh/resource/importer/AudioSoundFontFactory.h"
#include "soh/resource/importer/CollisionHeaderFactory.h"
#include "soh/resource/importer/CutsceneFactory.h"
#include "soh/resource/importer/PathFactory.h"
#include "soh/resource/importer/PlayerAnimationFactory.h"
#include "soh/resource/importer/SceneFactory.h"
#include "soh/resource/importer/SkeletonFactory.h"
#include "soh/resource/importer/SkeletonLimbFactory.h"
#include "soh/resource/importer/TextFactory.h"
#include "soh/resource/importer/BackgroundFactory.h"


bool SoH_HandleConfigDrop(char* filePath);

OTRGlobals* OTRGlobals::Instance;
SaveManager* SaveManager::Instance;

extern "C" char** cameraStrings;
extern "C" PlayState* gPlayState;

extern "C" void PadMgr_ThreadEntry(PadMgr* padMgr);
std::vector<std::shared_ptr<std::string>> cameraStdStrings;

Color_RGB8 kokiriColor = { 0x1E, 0x69, 0x1B };
Color_RGB8 goronColor = { 0x64, 0x14, 0x00 };
Color_RGB8 zoraColor = { 0x00, 0xEC, 0x64 };

int32_t previousImGuiScaleIndex;
float previousImGuiScale;

// Same as NaviColor type from OoT src (z_actor.c), but modified to be sans alpha channel for Controller LED.
typedef struct {
    Color_RGB8 inner;
    Color_RGB8 outer;
} NaviColor_RGB8;

static NaviColor_RGB8 defaultIdleColor = { { 255, 255, 255 }, { 0, 0, 255 } };
static NaviColor_RGB8 defaultNPCColor = { { 150, 150, 255 }, { 150, 150, 255 } };
static NaviColor_RGB8 defaultEnemyColor = { { 255, 255, 0 }, { 200, 155, 0 } };
static NaviColor_RGB8 defaultPropsColor = { { 0, 255, 0 }, { 0, 255, 0 } };

// Labeled according to ActorCategory (included through ActorDB.h)
const NaviColor_RGB8 LEDColorDefaultNaviColorList[] = {
    defaultPropsColor, // ACTORCAT_SWITCH       Switch
    defaultPropsColor, // ACTORCAT_BG           Background (Prop type 1)
    defaultIdleColor,  // ACTORCAT_PLAYER       Player
    defaultPropsColor, // ACTORCAT_EXPLOSIVE    Bomb
    defaultNPCColor,   // ACTORCAT_NPC          NPC
    defaultEnemyColor, // ACTORCAT_ENEMY        Enemy
    defaultPropsColor, // ACTORCAT_PROP         Prop type 2
    defaultPropsColor, // ACTORCAT_ITEMACTION   Item/Action
    defaultPropsColor, // ACTORCAT_MISC         Misc.
    defaultEnemyColor, // ACTORCAT_BOSS         Boss
    defaultPropsColor, // ACTORCAT_DOOR         Door
    defaultPropsColor, // ACTORCAT_CHEST        Chest
    defaultPropsColor, // ACTORCAT_MAX
};

// OTRTODO: A lot of these left in Japanese are used by the mempak manager. LUS does not currently support mempaks.
// Ignore unused ones.
const char* constCameraStrings[] = {
    "INSUFFICIENT",
    "KEYFRAMES",
    "YOU CAN ADD MORE",
    "FINISHED",
    "PLAYING",
    "DEMO CAMERA TOOL",
    "CANNOT PLAY",
    "KEYFRAME   ",
    "PNT   /      ",
    ">            >",
    "<            <",
    "<          >",
    GFXP_KATAKANA "*ﾌﾟﾚｲﾔ-*",
    "E MODE FIX",
    "E MODE ABS",
    GFXP_HIRAGANA "ｶﾞﾒﾝ" GFXP_KATAKANA "   ﾃﾞﾓ", // OTRTODO: Unused, get a translation! Number 15
    GFXP_HIRAGANA "ｶﾞﾒﾝ   ﾌﾂｳ",                  // OTRTODO: Unused, get a translation! Number 16
    "P TIME  MAX",
    GFXP_KATAKANA "ﾘﾝｸ" GFXP_HIRAGANA "    ｷｵｸ", // OTRTODO: Unused, get a translation! Number 18
    GFXP_KATAKANA "ﾘﾝｸ" GFXP_HIRAGANA "     ﾑｼ", // OTRTODO: Unused, get a translation! Number 19
    "*VIEWPT*",
    "*CAMPOS*",
    "DEBUG CAMERA",
    "CENTER/LOCK",
    "CENTER/FREE",
    "DEMO CONTROL",
    GFXP_KATAKANA "ﾒﾓﾘ" GFXP_HIRAGANA "ｶﾞﾀﾘﾏｾﾝ",
    "p",
    "e",
    "s",
    "l",
    "c",
    GFXP_KATAKANA "ﾒﾓﾘﾊﾟｯｸ",
    GFXP_KATAKANA "ｾｰﾌﾞ",
    GFXP_KATAKANA "ﾛｰﾄﾞ",
    GFXP_KATAKANA "ｸﾘｱ-",
    GFXP_HIRAGANA "ｦﾇｶﾅｲﾃﾞﾈ",
    "FREE      BYTE",
    "NEED      BYTE",
    GFXP_KATAKANA "*ﾒﾓﾘ-ﾊﾟｯｸ*",
    GFXP_HIRAGANA "ｦﾐﾂｹﾗﾚﾏｾﾝ",
    GFXP_KATAKANA "ﾌｧｲﾙ " GFXP_HIRAGANA "ｦ",
    GFXP_HIRAGANA "ｼﾃﾓｲｲﾃﾞｽｶ?",
    GFXP_HIRAGANA "ｹﾞﾝｻﾞｲﾍﾝｼｭｳﾁｭｳﾉ",              // OTRTODO: Unused, get a translation! Number 43
    GFXP_KATAKANA "ﾌｧｲﾙ" GFXP_HIRAGANA "ﾊﾊｷｻﾚﾏｽ", // OTRTODO: Unused, get a translation! Number 44
    GFXP_HIRAGANA "ﾊｲ",
    GFXP_HIRAGANA "ｲｲｴ",
    GFXP_HIRAGANA "ｼﾃｲﾏｽ",
    GFXP_HIRAGANA "ｳﾜｶﾞｷ", // OTRTODO: Unused, get a translation! Number 48
    GFXP_HIRAGANA "ｼﾏｼﾀ",
    "USE       BYTE",
    GFXP_HIRAGANA "ﾆｼｯﾊﾟｲ",
    "E MODE REL",
    "FRAME       ",
    "KEY   /       ",
    "(CENTER)",
    "(ORIG)",
    "(PLAYER)",
    "(ALIGN)",
    "(SET)",
    "(OBJECT)",
    GFXP_KATAKANA "ﾎﾟｲﾝﾄNo.     ", // OTRTODO: Unused, need translation. Number 62
    "FOV              ",
    "N FRAME          ",
    "Z ROT            ",
    GFXP_KATAKANA "ﾓ-ﾄﾞ        ", // OTRTODO: Unused, need translation. Number 65
    "  R FOCUS   ",
    "PMAX              ",
    "DEPTH             ",
    "XROT              ",
    "YROT              ",
    GFXP_KATAKANA "ﾌﾚ-ﾑ         ",
    GFXP_KATAKANA "ﾄ-ﾀﾙ         ",
    GFXP_KATAKANA "ｷ-     /   ",
};

typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} OTRVersion;

std::shared_ptr<Fast::Fast3dWindow> sohFast3dWindow;
static OTRVersion DetectOTRVersion(std::string path, bool isMq);
static bool VerifyArchiveVersion(OTRVersion version);
std::string portArchivePath = "";
static bool sohArchiveVersionMatch = false;

OTRGlobals::OTRGlobals() {
    context = Ship::Context::CreateUninitializedInstance("Ship of Harkinian", appShortName, "shipofharkinian.json");

    portArchivePath = Ship::Context::LocateFileAcrossAppDirs("soh.o2r");
    OTRVersion portArchiveVersion = DetectOTRVersion("soh.o2r", false);
    sohArchiveVersionMatch = portArchiveVersion.major == gBuildVersionMajor &&
                             portArchiveVersion.minor == gBuildVersionMinor &&
                             portArchiveVersion.patch == gBuildVersionPatch;

    context->InitConfiguration();
    context->InitConsoleVariables();

    auto controlDeck = std::make_shared<LUS::ControlDeck>(std::vector<CONTROLLERBUTTONS_T>({
        BTN_CUSTOM_MODIFIER1,
        BTN_CUSTOM_MODIFIER2,
    }));
    context->InitControlDeck(controlDeck);
    context->InitResourceManager({ portArchivePath }, {}, 3, true);
    context->InitConsole();

    auto sohInputEditorWindow =
        std::make_shared<SohInputEditorWindow>(CVAR_WINDOW("ControllerConfiguration"), "Configure Controller");
    sohFast3dWindow =
        std::make_shared<Fast::Fast3dWindow>(std::vector<std::shared_ptr<Ship::GuiWindow>>({ sohInputEditorWindow }));
    context->InitWindow(sohFast3dWindow);

    SohGui::SetupMenu();

    if (sohArchiveVersionMatch) {

        auto overlay = context->GetInstance()->GetWindow()->GetGui()->GetGameOverlay();
        overlay->LoadFont("Press Start 2P", 12.0f, "fonts/PressStart2P-Regular.ttf");
        overlay->LoadFont("Fipps", 32.0f, "fonts/Fipps-Regular.otf");
        overlay->SetCurrentFont(CVarGetString(CVAR_GAME_OVERLAY_FONT, "Press Start 2P"));

        fontMonoSmall = CreateFontWithSize(14.0f, "fonts/Inconsolata-Regular.ttf");
        fontMono = CreateFontWithSize(16.0f, "fonts/Inconsolata-Regular.ttf");
        fontMonoLarger = CreateFontWithSize(20.0f, "fonts/Inconsolata-Regular.ttf");
        fontMonoLargest = CreateFontWithSize(24.0f, "fonts/Inconsolata-Regular.ttf");
        fontStandard = CreateFontWithSize(16.0f, "fonts/Montserrat-Regular.ttf");
        fontStandardLarger = CreateFontWithSize(20.0f, "fonts/Montserrat-Regular.ttf");
        fontStandardLargest = CreateFontWithSize(24.0f, "fonts/Montserrat-Regular.ttf");
        fontJapanese = CreateFontWithSize(24.0f, "fonts/NotoSansJP-Regular.ttf", true);
        ImGui::GetIO().FontDefault = fontStandardLarger;
    }

    previousImGuiScaleIndex = -1;
    previousImGuiScale = defaultImGuiScale;
    ScaleImGui();
}

typedef enum ExtractSteps {
    ES_PORT_ARCHIVE,
    ES_WINDOWS,
    ES_EXTRACT_ARGS,
    ES_EXTRACT,
    ES_VERIFY,
} ExtractSteps;

typedef enum PromptSteps {
    PS_FILE_CHECK,
    PS_LOCAL,
    PS_FIRST,
    PS_SECOND,
    PS_DUPE,
    PS_WAIT,
    PS_NONE,
} PromptSteps;

typedef enum WindowsSteps {
    WS_TEMP,
    WS_PERMS,
    WS_ONEDRIVE,
    WS_DONE,
} WindowsSteps;

bool IsSubpath(const std::filesystem::path& path, const std::filesystem::path& base) {
    auto rel = std::filesystem::relative(path, base);
    return !rel.empty() && rel.native()[0] != '.';
}

bool PathTestCleanup(FILE* tfile) {
    try {
        if (std::filesystem::exists("./text.txt"))
            std::filesystem::remove("./text.txt");
        if (std::filesystem::exists("./test/"))
            std::filesystem::remove("./test/");
    } catch (std::filesystem::filesystem_error const& ex) { return false; }
    return true;
}

namespace SohGui {
extern std::shared_ptr<SohGui::SohMenu> mSohMenu;
}

void OTRGlobals::RunExtract(int argc, char* argv[]) {
    bool extractDone = false;
    ExtractSteps extractStep = ES_PORT_ARCHIVE;
    WindowsSteps windowsStep = WS_TEMP;
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(OTRGlobals::Instance->context->GetWindow());
    auto gui = wnd->GetGui();

    OTRVersion vanillaVersion = DetectOTRVersion("oot.o2r", false);
    OTRVersion mqVersion = DetectOTRVersion("oot-mq.o2r", true);

    bool shouldRegen = VerifyArchiveVersion(vanillaVersion) || VerifyArchiveVersion(mqVersion);

    std::filesystem::path ownPath;
    std::vector<std::string> args;
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            args.push_back(argv[i]);
        }
    }
    Extractor extract;
    PromptSteps promptStep = PS_FILE_CHECK;
    bool generatedIsMQ = false;
    std::atomic<size_t> extractCount = 0, totalExtract = 0;

    std::string installPath = Ship::Context::GetAppBundlePath();
    std::string dataPath = Ship::Context::GetAppDirectoryPath(appShortName);
    std::string file;

#if defined(__SWITCH__)
    SohGui::RegisterPopup("Outdated ROM Archives",
                          "\x1b[2;2HYou've launched the Ship with an old ROM O2R file."
                          "\x1b[4;2HPlease regenerate a new ROM O2R and relaunch."
                          "\x1b[6;2HPress the Home button to exit...",
                          "OK", "", [&]() { exit(1); });
#elif defined(__WIIU__)
    SohGui::RegisterPopup("Outdated ROM Archives",
                          "You've launched the Ship with an old a ROM O2R file.\n\n"
                          "Please generate a ROM O2R and relaunch.\n\n"
                          "Press and hold the Power button to shutdown...",
                          "OK", "", [&]() { exit(1); });
    OSFatal();
#endif

    if (!std::filesystem::exists(installPath + "/assets")) {
        SohGui::RegisterPopup("Extractor assets not found",
                              "No O2R files found. Missing 'assets/' folder needed to generate OTR file.\nPlease "
                              "re-extract them from the download or.\n\nExiting...",
                              "OK", "", [&]() { exit(1); });
    } else if (shouldRegen) {
        SohGui::RegisterPopup("Outdated ROM Archives",
                              "Your oot.o2r or oot-mq.o2r were created with incompatible versions of SoH.\nYou will "
                              "now be redirected to re-extract them.");
        std::filesystem::remove("oot.o2r");
        std::filesystem::remove("oot-mq.o2r");
    }

    std::shared_ptr<BS::thread_pool> threadPool = std::make_shared<BS::thread_pool>(1);
    std::optional<std::future<void>> extractionTask;

    while (!extractDone) {
        if (SohGui::PopupsQueued() > 0 || extractionTask.has_value()) {
            goto render;
        }
        switch (extractStep) {
            case ES_PORT_ARCHIVE: {
                if (sohArchiveVersionMatch) {
#ifdef _WIN32
                    extractStep = ES_WINDOWS;
#elif (defined(__WIIU__) || defined(__SWITCH__))
                    extractStep = ES_VERIFY;
#else
                    extractStep = args.empty() ? ES_EXTRACT : ES_EXTRACT_ARGS;
#endif
                } else {
                    std::string msg;

#if defined(__SWITCH__)
                    msg = "\x1b[4;2HPlease re-extract it from the download.\n"
                          "\x1b[6;2HPress the Home button to exit...";
#elif defined(__WIIU__)
                    msg = "Please extract the soh.o2r from the Ship of Harkinian download\nto your folder.\n\nPress "
                          "and hold the power\n"
                          "button to shutdown...";
#else
                    msg =
                        "Please extract the soh.o2r from the Ship of Harkinian download to your folder.\n\nExiting...";
#endif
                    std::string title =
                        !std::filesystem::exists(portArchivePath) ? "Missing soh.o2r" : "soh.o2r is outdated";
                    SohGui::RegisterPopup(title, msg, "OK", "", [&]() { exit(1); });
                }
                continue;
            }
            case ES_WINDOWS: {
                switch (windowsStep) {
                    case WS_TEMP: {
#ifdef _WIN32
                        char* tempVar = getenv("TEMP");
                        std::filesystem::path tempPath;
                        try {
                            tempPath = std::filesystem::canonical(tempVar);
                        } catch (std::filesystem::filesystem_error const& ex) {
                            std::string userPath = getenv("USERPROFILE");
                            userPath.append("\\AppData\\Local\\Temp");
                            tempPath = std::filesystem::canonical(userPath);
                        }
                        wchar_t buffer[MAX_PATH];
                        GetModuleFileName(NULL, buffer, _countof(buffer));
                        ownPath = std::filesystem::canonical(buffer).parent_path();
                        if (IsSubpath(ownPath, tempPath)) {
                            SohGui::RegisterPopup("SoH Path Error",
                                                  "SoH is running in a temp folder.\nExtract the .zip and run again.",
                                                  "OK", "", [&]() { exit(0); });
                        } else {
                            windowsStep = WS_PERMS;
                        }
#endif
                        continue;
                    }
                    case WS_PERMS: {
                        FILE* tfile = fopen("./text.txt", "w");
                        std::filesystem::path tfolder = std::filesystem::path("./test/");
                        bool error = false;
                        try {
                            create_directories(tfolder);
                        } catch (std::filesystem::filesystem_error const& ex) { error = true; }
                        if (tfile == NULL || error) {
                            SohGui::RegisterPopup("SoH Permissions Error",
                                                  "SoH does not have proper file permissions.\nPlease move it to a "
                                                  "folder that does and run again.",
                                                  "OK", "", [&]() {
                                                      fclose(tfile);
                                                      PathTestCleanup(tfile);
                                                      exit(0);
                                                  });
                        } else {
                            fclose(tfile);
                            if (!PathTestCleanup(tfile)) {
                                SohGui::RegisterPopup("SoH Permissions Error",
                                                      "SoH does not have proper file permissions.\nPlease move it to a "
                                                      "folder that does and run again.",
                                                      "OK", "", [&]() { exit(0); });
                            }
                            windowsStep = WS_ONEDRIVE;
                        }
                        continue;
                    }
                    case WS_ONEDRIVE: {
                        if (ownPath.string().find("OneDrive") != std::string::npos) {
                            SohGui::RegisterPopup("SoH Path Error",
                                                  "SoH appears to be in a OneDrive folder, which will cause issues.\n"
                                                  "Please move it to a folder outside of OneDrive, like the root of a\n"
                                                  "drive (e.g. \"C:\\Games\\SoH\").",
                                                  "OK", "", [&]() { exit(0); });
                        } else {
                            windowsStep = WS_DONE;
                            extractStep = args.empty() ? ES_EXTRACT : ES_EXTRACT_ARGS;
                        }
                        continue;
                    }
                    default:
                        continue;
                }
                break;
            }
            case ES_EXTRACT_ARGS: {
#if !defined(__SWITCH__) && !defined(__WIIU__)
                if (args.empty()) {
                    SohGui::RegisterPopup(
                        "Run Ship of Harkinian", "All files have been processed. Run SoH?", "Yes", "No",
                        [&]() {
                            if (!std::filesystem::exists(Ship::Context::GetAppDirectoryPath(appShortName) +
                                                         "/oot.o2r") &&
                                !std::filesystem::exists(Ship::Context::GetAppDirectoryPath(appShortName) +
                                                         "/oot-mq.o2r")) {
                                extractStep = ES_EXTRACT;
                                promptStep = PS_FILE_CHECK;
                            } else {
                                extractStep = ES_VERIFY;
                            }
                        },
                        [&]() { exit(0); });
                    break;
                }
                file = args.at(0);
                args.erase(args.begin());
                extract = Extractor();
                if (extract.RunFileStandalone(file)) {
                    bool doExtract = true;
                    std::string archive = (extract.IsMasterQuest() ? "oot-mq.o2r" : "oot.o2r");
                    if (std::filesystem::exists(Ship::Context::GetAppDirectoryPath(appShortName) + "/" + archive)) {
                        std::string msg = "Archive for current ROM, " + archive + ", already exists.\nExtract again?";
                        SohGui::RegisterPopup("Confirm Re-extract", msg.c_str(), "Yes", "No", [&]() {
                            extractionTask = threadPool->submit_task([&]() -> void {
                                extract.CallZapd(installPath, Ship::Context::GetAppDirectoryPath(appShortName),
                                                 &extractCount, &totalExtract);
                                extractCount = totalExtract = 0;
                            });
                        });
                    } else {
                        extractionTask = threadPool->submit_task([&]() -> void {
                            extract.CallZapd(installPath, Ship::Context::GetAppDirectoryPath(appShortName),
                                             &extractCount, &totalExtract);
                            extractCount = totalExtract = 0;
                        });
                    }
                } else {
                    bool open = true;
                    std::string msg = "File\n" + std::string(file) + "\nis not a ROM or does not match supported ROMs.";
                    SohGui::RegisterPopup("SoH ROM Error", msg.c_str());
                }
#else
                extractStep = ES_VERIFY;
#endif
                break;
            }
            case ES_EXTRACT: {
                switch (promptStep) {
                    case PS_FILE_CHECK: {
                        const bool ootO2RExists =
                            std::filesystem::exists(
                                Ship::Context::LocateFileAcrossAppDirs("oot-mq.o2r", appShortName)) ||
                            std::filesystem::exists(Ship::Context::LocateFileAcrossAppDirs("oot.o2r", appShortName));

                        if (!ootO2RExists) {
                            SohGui::RegisterPopup(
                                "No O2R Files", "No O2R files found. Generate one now?", "Yes", "No",
                                [&]() { promptStep = PS_LOCAL; }, [&]() { exit(0); });
                        } else {
                            extractStep = ES_VERIFY;
                        }
                        continue;
                    }
                    case PS_LOCAL: {
                        extract = Extractor();
                        extract.SetSearchPath(installPath);
                        extract.GetRoms(args);
                        extract.SetSearchPath(dataPath);
                        extract.GetRoms(args);
                        if (!args.empty()) {
                            promptStep = PS_WAIT;
                            SohGui::RegisterPopup(
                                "ROMs found", "ROMs found in application directory. Would you like to process them?",
                                "Yes", "No", [&]() { extractStep = ES_EXTRACT_ARGS; },
                                [&]() { promptStep = PS_FIRST; });
                        } else {
                            promptStep = PS_FIRST;
                        }
                        continue;
                    }
                    case PS_FIRST: {
                        if (!extract.ManuallySearchForRomMatchingType(RomSearchMode::Both)) {
                            promptStep = PS_FILE_CHECK;
                            continue;
                        }
                        extractionTask = threadPool->submit_task([&]() -> void {
                            extract.CallZapd(installPath, Ship::Context::GetAppDirectoryPath(appShortName),
                                             &extractCount, &totalExtract);
                            generatedIsMQ = extract.IsMasterQuest();
                            promptStep = PS_SECOND;
                            extractCount = 0;
                            totalExtract = 0;
                        });
                        continue;
                    }
                    case PS_SECOND: {
                        SohGui::RegisterPopup(
                            "Extraction Complete", "ROM Extracted. Extract another?", "Yes", "No",
                            [&]() {
                                if (!extract.ManuallySearchForRomMatchingType(generatedIsMQ ? RomSearchMode::Vanilla
                                                                                            : RomSearchMode::MQ)) {
                                    extractStep = ES_VERIFY;
                                } else {
                                    extractionTask = threadPool->submit_task([&]() -> void {
                                        extract.CallZapd(installPath, Ship::Context::GetAppDirectoryPath(appShortName),
                                                         &extractCount, &totalExtract);
                                        extractStep = ES_VERIFY;
                                        extractCount = 0;
                                        totalExtract = 0;
                                    });
                                }
                            },
                            [&]() { extractStep = ES_VERIFY; });
                        continue;
                    }
                    default:
                        break;
                }
                break;
            }
            case ES_VERIFY: {
                const bool ootO2RExists =
                    std::filesystem::exists(Ship::Context::LocateFileAcrossAppDirs("oot-mq.o2r", appShortName)) ||
                    std::filesystem::exists(Ship::Context::LocateFileAcrossAppDirs("oot.o2r", appShortName));

                if (!ootO2RExists) {
                    SohGui::RegisterPopup("No ROM Archives",
                                          "No ROM O2R files detected. Please generate a ROM O2R and relaunch.", "OK",
                                          "", [&]() { exit(0); });
                }
                extractDone = true;
                continue;
            }
            default:
                break;
        }

    render:
        if (!WindowIsRunning()) {
            exit(0);
        }
        // Process window events for resize, mouse, keyboard events
        wnd->HandleEvents();
        UIWidgets::Colors themeColor =
            static_cast<UIWidgets::Colors>(CVarGetInteger(CVAR_SETTING("Menu.Theme"), UIWidgets::Colors::LightBlue));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UIWidgets::ColorValues.at(themeColor));
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, UIWidgets::ColorValues.at(UIWidgets::Colors::DarkGray));

        // Skip dropped frames
        if (!wnd->IsFrameReady()) {
            continue;
        }
        gui->StartDraw();
        sohFast3dWindow->StartFrame();
        sohFast3dWindow->RunGuiOnly();
        if (extractionTask.has_value()) {
            auto status = extractionTask->wait_for(std::chrono::milliseconds(0));
            if (status == std::future_status::ready) {
                try {
                    extractionTask->get();
                } catch (const std::exception& e) {
                    SohGui::RegisterPopup("Extraction Crashed", e.what(), "Close", "", []() { exit(1); });
                }
                extractionTask.reset();
            } else {
                if (!ImGui::IsPopupOpen("ROM Extraction")) {
                    ImGui::OpenPopup("ROM Extraction");
                }
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 8.0f));
                auto color = UIWidgets::ColorValues.at(THEME_COLOR);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(color.x, color.y, color.z, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(color.x, color.y, color.z, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.3f));
                if (ImGui::BeginPopupModal("ROM Extraction", NULL,
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
                                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                               ImGuiWindowFlags_NoSavedSettings)) {
                    float progress = (totalExtract > 0.0f ? (float)extractCount / (float)totalExtract : 0) * 100.0f;
                    auto filename = std::filesystem::path(file).filename().string();
                    ImGui::Text("Extracting %s...%s", filename.c_str(),
                                roundf(progress) == 100.0f ? " Done. Finishing up." : "");
                    std::string overlay = extractCount > 0 ? fmt::format("{:.0f}%", progress) : "Starting Up";
                    ImGui::ProgressBar(progress / 100.0f, ImVec2(600.0f, 50.0f), overlay.c_str());
                    ImGui::EndPopup();
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(2);
            }
        }
        gui->EndDraw();
        sohFast3dWindow->EndFrame();
        ImGui::PopStyleColor(2);
    }

#ifdef __SWITCH__
    Ship::Switch::Init(Ship::PreInitPhase);
#elif defined(__WIIU__)
    Ship::WiiU::Init(appShortName);
#endif
}

void OTRGlobals::Initialize() {
    std::string mqPath = Ship::Context::LocateFileAcrossAppDirs("oot-mq.o2r", appShortName);
    if (std::filesystem::exists(mqPath)) {
        context->GetResourceManager()->GetArchiveManager()->AddArchive(mqPath);
    }
    std::string ootPath = Ship::Context::LocateFileAcrossAppDirs("oot.o2r", appShortName);
    if (std::filesystem::exists(ootPath)) {
        context->GetResourceManager()->GetArchiveManager()->AddArchive(ootPath);
    }

    std::unordered_set<uint32_t> ValidHashes = {
        OOT_PAL_MQ,     OOT_NTSC_JP_MQ, OOT_NTSC_US_MQ, OOT_PAL_GC_MQ_DBG, OOT_NTSC_US_10,
        OOT_NTSC_US_11, OOT_NTSC_US_12, OOT_PAL_10,     OOT_PAL_11,        OOT_NTSC_JP_GC_CE,
        OOT_NTSC_JP_GC, OOT_NTSC_US_GC, OOT_PAL_GC,     OOT_PAL_GC_DBG1,   OOT_PAL_GC_DBG2,
    };

#if (_DEBUG)
    auto defaultLogLevel = spdlog::level::trace;
#else
    auto defaultLogLevel = spdlog::level::info;
#endif
    context->InitConfiguration();
    context->InitConsoleVariables();
    auto logLevel =
        static_cast<spdlog::level::level_enum>((defaultLogLevel));
    context->InitLogging(logLevel, logLevel);
    Ship::Context::GetInstance()->GetLogger()->set_pattern("[%H:%M:%S.%e] [%s:%#] [%l] %v");

    context->InitGfxDebugger();
    context->InitFileDropMgr();

    // tell LUS to reserve 3 SoH specific threads (Game, Audio, Save)
    context->GetResourceManager()->SetAltAssetsEnabled(false);

    context->InitCrashHandler();

    context->GetWindow()->SetForceCursorVisibility(CVarGetInteger(CVAR_SETTING("CursorVisibility"), 0));

    context->InitAudio({ .SampleRate = 32000, .SampleLength = 1024, .DesiredBuffered = 1680 });

    SPDLOG_INFO("Starting Ship of Harkinian version {} (Branch: {} | Commit: {})", (char*)gBuildVersion,
                (char*)gGitBranch, (char*)gGitCommitHash);

    auto loader = context->GetResourceManager()->GetResourceLoader();
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV0>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryTextureV1>(), RESOURCE_FORMAT_BINARY,
                                    "Texture", static_cast<uint32_t>(Fast::ResourceType::Texture), 1);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryVertexV0>(), RESOURCE_FORMAT_BINARY,
                                    "Vertex", static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLVertexV0>(), RESOURCE_FORMAT_XML, "Vertex",
                                    static_cast<uint32_t>(Fast::ResourceType::Vertex), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryDisplayListV0>(),
                                    RESOURCE_FORMAT_BINARY, "DisplayList",
                                    static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryXMLDisplayListV0>(), RESOURCE_FORMAT_XML,
                                    "DisplayList", static_cast<uint32_t>(Fast::ResourceType::DisplayList), 0);
    loader->RegisterResourceFactory(std::make_shared<Fast::ResourceFactoryBinaryMatrixV0>(), RESOURCE_FORMAT_BINARY,
                                    "Matrix", static_cast<uint32_t>(Fast::ResourceType::Matrix), 0);
    loader->RegisterResourceFactory(std::make_shared<Ship::ResourceFactoryBinaryBlobV0>(), RESOURCE_FORMAT_BINARY,
                                    "Blob", static_cast<uint32_t>(Ship::ResourceType::Blob), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryArrayV0>(), RESOURCE_FORMAT_BINARY,
                                    "Array", static_cast<uint32_t>(SOH::ResourceType::SOH_Array), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAnimationV0>(), RESOURCE_FORMAT_BINARY,
                                    "Animation", static_cast<uint32_t>(SOH::ResourceType::SOH_Animation), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryPlayerAnimationV0>(),
                                    RESOURCE_FORMAT_BINARY, "PlayerAnimation",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_PlayerAnimation), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySceneV0>(), RESOURCE_FORMAT_BINARY,
                                    "Room", static_cast<uint32_t>(SOH::ResourceType::SOH_Room),
                                    0); // Is room scene? maybe?
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSceneV0>(), RESOURCE_FORMAT_XML, "Room",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_Room), 0); // Is room scene? maybe?
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryCollisionHeaderV0>(),
                                    RESOURCE_FORMAT_BINARY, "CollisionHeader",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_CollisionHeader), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLCollisionHeaderV0>(), RESOURCE_FORMAT_XML,
                                    "CollisionHeader", static_cast<uint32_t>(SOH::ResourceType::SOH_CollisionHeader),
                                    0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonV0>(), RESOURCE_FORMAT_BINARY,
                                    "Skeleton", static_cast<uint32_t>(SOH::ResourceType::SOH_Skeleton), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSkeletonV0>(), RESOURCE_FORMAT_XML,
                                    "Skeleton", static_cast<uint32_t>(SOH::ResourceType::SOH_Skeleton), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinarySkeletonLimbV0>(),
                                    RESOURCE_FORMAT_BINARY, "SkeletonLimb",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_SkeletonLimb), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSkeletonLimbV0>(), RESOURCE_FORMAT_XML,
                                    "SkeletonLimb", static_cast<uint32_t>(SOH::ResourceType::SOH_SkeletonLimb), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryPathV0>(), RESOURCE_FORMAT_BINARY,
                                    "Path", static_cast<uint32_t>(SOH::ResourceType::SOH_Path), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLPathV0>(), RESOURCE_FORMAT_XML, "Path",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_Path), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryCutsceneV0>(), RESOURCE_FORMAT_BINARY,
                                    "Cutscene", static_cast<uint32_t>(SOH::ResourceType::SOH_Cutscene), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryTextV0>(), RESOURCE_FORMAT_BINARY,
                                    "Text", static_cast<uint32_t>(SOH::ResourceType::SOH_Text), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLTextV0>(), RESOURCE_FORMAT_XML, "Text",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_Text), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSampleV2>(), RESOURCE_FORMAT_BINARY,
                                    "AudioSample", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSample), 2);

    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLAudioSampleV0>(), RESOURCE_FORMAT_XML,
                                    "Sample", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSample), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSoundFontV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSoundFont",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSoundFont), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLSoundFontV0>(), RESOURCE_FORMAT_XML,
                                    "SoundFont", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSoundFont), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryAudioSequenceV2>(),
                                    RESOURCE_FORMAT_BINARY, "AudioSequence",
                                    static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSequence), 2);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryXMLAudioSequenceV0>(), RESOURCE_FORMAT_XML,
                                    "Sequence", static_cast<uint32_t>(SOH::ResourceType::SOH_AudioSequence), 0);
    loader->RegisterResourceFactory(std::make_shared<SOH::ResourceFactoryBinaryBackgroundV0>(), RESOURCE_FORMAT_BINARY,
                                    "Background", static_cast<uint32_t>(SOH::ResourceType::SOH_Background), 0);

    hasMasterQuest = hasOriginal = false;

    // Move the camera strings from read only memory onto the heap (writable memory)
    // This is in OTRGlobals right now because this is a place that will only ever be run once at the beginning of
    // startup. We should probably find some code in db_camera that does initialization and only run once, and then
    // dealloc on deinitialization.
    cameraStrings = (char**)malloc(sizeof(constCameraStrings));
    for (size_t i = 0; i < sizeof(constCameraStrings) / sizeof(char*); i++) {
        // OTRTODO: never deallocated...
        cameraStrings[i] = strdup(constCameraStrings[i]);
    }

    auto versions = context->GetResourceManager()->GetArchiveManager()->GetGameVersions();

    for (uint32_t version : versions) {
        if (!ValidHashes.contains(version)) {
#if defined(__SWITCH__)
            SPDLOG_ERROR("Invalid OTR File!");
#elif defined(__WIIU__)
            Ship::WiiU::ThrowInvalidOTR();
#else
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Invalid OTR File",
                                     "Attempted to load an invalid OTR file. Try regenerating.", nullptr);
            SPDLOG_ERROR("Invalid OTR File!");
#endif
            exit(1);
        }
        switch (version) {
            case OOT_PAL_MQ:
            case OOT_NTSC_JP_MQ:
            case OOT_NTSC_US_MQ:
            case OOT_PAL_GC_MQ_DBG:
                hasMasterQuest = true;
                break;
            case OOT_NTSC_US_10:
            case OOT_NTSC_US_11:
            case OOT_NTSC_US_12:
            case OOT_PAL_10:
            case OOT_PAL_11:
            case OOT_NTSC_JP_GC_CE:
            case OOT_NTSC_JP_GC:
            case OOT_NTSC_US_GC:
            case OOT_PAL_GC:
            case OOT_PAL_GC_DBG1:
            case OOT_PAL_GC_DBG2:
                hasOriginal = true;
                break;
            default:
                break;
        }
    }
}

OTRGlobals::~OTRGlobals() {
}

void OTRGlobals::ScaleImGui() {
    int32_t imGuiScaleIndex = CVarGetInteger(CVAR_SETTING("ImGuiScale"), defaultImGuiScale);
    if (imGuiScaleIndex == previousImGuiScaleIndex) {
        return;
    }

    float scale = imguiScaleOptionToValue[imGuiScaleIndex];
    float newScale = scale / previousImGuiScale;
    ImGui::GetStyle().ScaleAllSizes(newScale);
    ImGui::GetIO().FontGlobalScale = scale;
    previousImGuiScale = scale;
    previousImGuiScaleIndex = imGuiScaleIndex;
}

ImFont* OTRGlobals::CreateDefaultFontWithSize(float size) {
    auto mImGuiIo = &ImGui::GetIO();
    ImFontConfig fontCfg = ImFontConfig();
    fontCfg.OversampleH = fontCfg.OversampleV = 1;
    fontCfg.PixelSnapH = true;
    fontCfg.SizePixels = size;
    ImFont* font = mImGuiIo->Fonts->AddFontDefault(&fontCfg);
    // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly
    float iconFontSize = size * 2.0f / 3.0f;
    static const ImWchar sIconsRanges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = true;
    iconsConfig.PixelSnapH = true;
    iconsConfig.GlyphMinAdvanceX = iconFontSize;
    mImGuiIo->Fonts->AddFontFromMemoryCompressedBase85TTF(fontawesome_compressed_data_base85, iconFontSize,
                                                          &iconsConfig, sIconsRanges);
    return font;
}

bool OTRGlobals::HasMasterQuest() {
    return hasMasterQuest;
}

bool OTRGlobals::HasOriginal() {
    return hasOriginal;
}

uint32_t OTRGlobals::GetInterpolationFPS() {
    if (CVarGetInteger(CVAR_SETTING("MatchRefreshRate"), 0)) {
        return Ship::Context::GetInstance()->GetWindow()->GetCurrentRefreshRate();
    } else if (CVarGetInteger(CVAR_VSYNC_ENABLED, 1) ||
               !Ship::Context::GetInstance()->GetWindow()->CanDisableVerticalSync()) {
        return std::min<uint32_t>(Ship::Context::GetInstance()->GetWindow()->GetCurrentRefreshRate(),
                                  CVarGetInteger(CVAR_SETTING("InterpolationFPS"), 20));
    }
    return CVarGetInteger(CVAR_SETTING("InterpolationFPS"), 20);
}

extern "C" void OTRMessage_Init();
extern "C" void AudioMgr_CreateNextAudioBuffer(s16* samples, u32 num_samples);
extern "C" void AudioPlayer_Play(const uint8_t* buf, uint32_t len);
extern "C" int AudioPlayer_Buffered(void);
extern "C" int AudioPlayer_GetDesiredBuffered(void);
std::unordered_map<std::string, ExtensionEntry> ExtensionCache;

void OTRAudio_Thread() {
    while (audio.running) {
        {
            std::unique_lock<std::mutex> Lock(audio.mutex);
            while (!audio.processing && audio.running) {
                audio.cv_to_thread.wait(Lock);
            }

            if (!audio.running) {
                break;
            }
        }
        std::unique_lock<std::mutex> Lock(audio.mutex);
// AudioMgr_ThreadEntry(&gAudioMgr);
//  528 and 544 relate to 60 fps at 32 kHz 32000/60 = 533.333..
//  in an ideal world, one third of the calls should use num_samples=544 and two thirds num_samples=528
#define SAMPLES_HIGH 560
#define SAMPLES_LOW 528

#define AUDIO_FRAMES_PER_UPDATE (R_UPDATE_RATE > 0 ? R_UPDATE_RATE : 1)
#define NUM_AUDIO_CHANNELS 2

        int samples_left = AudioPlayer_Buffered();
        u32 num_audio_samples = samples_left < AudioPlayer_GetDesiredBuffered() ? SAMPLES_HIGH : SAMPLES_LOW;

        // 3 is the maximum authentic frame divisor.
        s16 audio_buffer[SAMPLES_HIGH * NUM_AUDIO_CHANNELS * 3];
        for (int i = 0; i < AUDIO_FRAMES_PER_UPDATE; i++) {
            AudioMgr_CreateNextAudioBuffer(audio_buffer + i * (num_audio_samples * NUM_AUDIO_CHANNELS),
                                           num_audio_samples);
        }

        AudioPlayer_Play((u8*)audio_buffer,
                         num_audio_samples * (sizeof(int16_t) * NUM_AUDIO_CHANNELS * AUDIO_FRAMES_PER_UPDATE));

        audio.processing = false;
        audio.cv_from_thread.notify_one();
    }
}

// C->C++ Bridge
extern "C" void OTRAudio_Init() {
    // Precache all our samples, sequences, etc...
    ResourceMgr_LoadDirectory("audio");

    if (!audio.running) {
        audio.running = true;
        audio.thread = std::thread(OTRAudio_Thread);
    }
}

extern "C" char** sequenceMap;
extern "C" size_t sequenceMapSize;

extern "C" char** fontMap;
extern "C" size_t fontMapSize;

extern "C" void OTRAudio_Exit() {
    // Tell the audio thread to stop
    {
        std::unique_lock<std::mutex> Lock(audio.mutex);
        audio.running = false;
    }
    audio.cv_to_thread.notify_all();

    // Wait until the audio thread quit
    audio.thread.join();
#if 0
    for (size_t i = 0; i < sequenceMapSize; i++) {
        free(sequenceMap[i]);
    }
    free(sequenceMap);

    for (size_t i = 0; i < fontMapSize; i++) {
        free(fontMap[i]);
    }
    free(fontMap);
    free(gAudioContext.seqLoadStatus);
    free(gAudioContext.fontLoadStatus);
#endif
}

static GetItemEntry sVanillaItemTable[] = {
        // clang-format off
        GET_ITEM(ITEM_BOMBS_5,          OBJECT_GI_BOMB_1,        GID_BOMB,             0x32, 0x59, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_BOMBS_5),
        GET_ITEM(ITEM_NUTS_5,           OBJECT_GI_NUTS,          GID_NUTS,             0x34, 0x0C, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_NUTS_5),
        GET_ITEM(ITEM_BOMBCHU,          OBJECT_GI_BOMB_2,        GID_BOMBCHU,          0x33, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BOMBCHUS_10),
        GET_ITEM(ITEM_BOW,              OBJECT_GI_BOW,           GID_BOW,              0x31, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BOW),
        GET_ITEM(ITEM_SLINGSHOT,        OBJECT_GI_PACHINKO,      GID_SLINGSHOT,        0x30, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_SLINGSHOT),
        GET_ITEM(ITEM_BOOMERANG,        OBJECT_GI_BOOMERANG,     GID_BOOMERANG,        0x35, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BOOMERANG),
        GET_ITEM(ITEM_STICK,            OBJECT_GI_STICK,         GID_STICK,            0x37, 0x0D, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_STICKS_1),
        GET_ITEM(ITEM_HOOKSHOT,         OBJECT_GI_HOOKSHOT,      GID_HOOKSHOT,         0x36, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_HOOKSHOT),
        GET_ITEM(ITEM_LONGSHOT,         OBJECT_GI_HOOKSHOT,      GID_LONGSHOT,         0x4F, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_LONGSHOT),
        GET_ITEM(ITEM_LENS,             OBJECT_GI_GLASSES,       GID_LENS,             0x39, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_LENS),
        GET_ITEM(ITEM_LETTER_ZELDA,     OBJECT_GI_LETTER,        GID_LETTER_ZELDA,     0x69, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_LETTER_ZELDA),
        GET_ITEM(ITEM_OCARINA_TIME,     OBJECT_GI_OCARINA,       GID_OCARINA_TIME,     0x3A, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_OCARINA_OOT),
        GET_ITEM(ITEM_HAMMER,           OBJECT_GI_HAMMER,        GID_HAMMER,           0x38, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_HAMMER),
        GET_ITEM(ITEM_COJIRO,           OBJECT_GI_NIWATORI,      GID_COJIRO,           0x02, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_COJIRO),
        GET_ITEM(ITEM_BOTTLE,           OBJECT_GI_BOTTLE,        GID_BOTTLE,           0x42, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BOTTLE),
        GET_ITEM(ITEM_POTION_RED,       OBJECT_GI_LIQUID,        GID_POTION_RED,       0x43, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_POTION_RED),
        GET_ITEM(ITEM_POTION_GREEN,     OBJECT_GI_LIQUID,        GID_POTION_GREEN,     0x44, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_POTION_GREEN),
        GET_ITEM(ITEM_POTION_BLUE,      OBJECT_GI_LIQUID,        GID_POTION_BLUE,      0x45, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_POTION_BLUE),
        GET_ITEM(ITEM_FAIRY,            OBJECT_GI_BOTTLE,        GID_BOTTLE,           0x46, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_FAIRY),
        GET_ITEM(ITEM_MILK_BOTTLE,      OBJECT_GI_MILK,          GID_MILK,             0x98, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_MILK_BOTTLE),
        GET_ITEM(ITEM_LETTER_RUTO,      OBJECT_GI_BOTTLE_LETTER, GID_LETTER_RUTO,      0x99, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_LETTER_RUTO),
        GET_ITEM(ITEM_BEAN,             OBJECT_GI_BEAN,          GID_BEAN,             0x48, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BEAN),
        GET_ITEM(ITEM_MASK_SKULL,       OBJECT_GI_SKJ_MASK,      GID_MASK_SKULL,       0x10, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_MASK_SKULL),
        GET_ITEM(ITEM_MASK_SPOOKY,      OBJECT_GI_REDEAD_MASK,   GID_MASK_SPOOKY,      0x11, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_MASK_SPOOKY),
        GET_ITEM(ITEM_CHICKEN,          OBJECT_GI_NIWATORI,      GID_CHICKEN,          0x48, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_CHICKEN),
        GET_ITEM(ITEM_MASK_KEATON,      OBJECT_GI_KI_TAN_MASK,   GID_MASK_KEATON,      0x12, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_MASK_KEATON),
        GET_ITEM(ITEM_MASK_BUNNY,       OBJECT_GI_RABIT_MASK,    GID_MASK_BUNNY,       0x13, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_MASK_BUNNY),
        GET_ITEM(ITEM_MASK_TRUTH,       OBJECT_GI_TRUTH_MASK,    GID_MASK_TRUTH,       0x17, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_MASK_TRUTH),
        GET_ITEM(ITEM_POCKET_EGG,       OBJECT_GI_EGG,           GID_EGG,              0x01, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_POCKET_EGG),
        GET_ITEM(ITEM_POCKET_CUCCO,     OBJECT_GI_NIWATORI,      GID_CHICKEN,          0x48, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_POCKET_CUCCO),
        GET_ITEM(ITEM_ODD_MUSHROOM,     OBJECT_GI_MUSHROOM,      GID_ODD_MUSHROOM,     0x03, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_ODD_MUSHROOM),
        GET_ITEM(ITEM_ODD_POTION,       OBJECT_GI_POWDER,        GID_ODD_POTION,       0x04, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_ODD_POTION),
        GET_ITEM(ITEM_SAW,              OBJECT_GI_SAW,           GID_SAW,              0x05, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_SAW),
        GET_ITEM(ITEM_SWORD_BROKEN,     OBJECT_GI_BROKENSWORD,   GID_SWORD_BROKEN,     0x08, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_SWORD_BROKEN),
        GET_ITEM(ITEM_PRESCRIPTION,     OBJECT_GI_PRESCRIPTION,  GID_PRESCRIPTION,     0x09, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_PRESCRIPTION),
        GET_ITEM(ITEM_FROG,             OBJECT_GI_FROG,          GID_FROG,             0x0D, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_FROG),
        GET_ITEM(ITEM_EYEDROPS,         OBJECT_GI_EYE_LOTION,    GID_EYEDROPS,         0x0E, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_EYEDROPS),
        GET_ITEM(ITEM_CLAIM_CHECK,      OBJECT_GI_TICKETSTONE,   GID_CLAIM_CHECK,      0x0A, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_CLAIM_CHECK),
        GET_ITEM(ITEM_SWORD_KOKIRI,     OBJECT_GI_SWORD_1,       GID_SWORD_KOKIRI,     0xA4, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_SWORD_KOKIRI),
        GET_ITEM(ITEM_SWORD_BGS,        OBJECT_GI_LONGSWORD,     GID_SWORD_BGS,        0x4B, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_SWORD_KNIFE),
        GET_ITEM(ITEM_SHIELD_DEKU,      OBJECT_GI_SHIELD_1,      GID_SHIELD_DEKU,      0x4C, 0xA0, CHEST_ANIM_SHORT, ITEM_CATEGORY_LESSER,          MOD_NONE, GI_SHIELD_DEKU),
        GET_ITEM(ITEM_SHIELD_HYLIAN,    OBJECT_GI_SHIELD_2,      GID_SHIELD_HYLIAN,    0x4D, 0xA0, CHEST_ANIM_SHORT, ITEM_CATEGORY_LESSER,          MOD_NONE, GI_SHIELD_HYLIAN),
        GET_ITEM(ITEM_SHIELD_MIRROR,    OBJECT_GI_SHIELD_3,      GID_SHIELD_MIRROR,    0x4E, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_SHIELD_MIRROR),
        GET_ITEM(ITEM_TUNIC_GORON,      OBJECT_GI_CLOTHES,       GID_TUNIC_GORON,      0x50, 0xA0, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_TUNIC_GORON),
        GET_ITEM(ITEM_TUNIC_ZORA,       OBJECT_GI_CLOTHES,       GID_TUNIC_ZORA,       0x51, 0xA0, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_TUNIC_ZORA),
        GET_ITEM(ITEM_BOOTS_IRON,       OBJECT_GI_BOOTS_2,       GID_BOOTS_IRON,       0x53, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BOOTS_IRON),
        GET_ITEM(ITEM_BOOTS_HOVER,      OBJECT_GI_HOVERBOOTS,    GID_BOOTS_HOVER,      0x54, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BOOTS_HOVER),
        GET_ITEM(ITEM_QUIVER_40,        OBJECT_GI_ARROWCASE,     GID_QUIVER_40,        0x56, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_QUIVER_40),
        GET_ITEM(ITEM_QUIVER_50,        OBJECT_GI_ARROWCASE,     GID_QUIVER_50,        0x57, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_QUIVER_50),
        GET_ITEM(ITEM_BOMB_BAG_20,      OBJECT_GI_BOMBPOUCH,     GID_BOMB_BAG_20,      0x58, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BOMB_BAG_20),
        GET_ITEM(ITEM_BOMB_BAG_30,      OBJECT_GI_BOMBPOUCH,     GID_BOMB_BAG_30,      0x59, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_BOMB_BAG_30),
        GET_ITEM(ITEM_BOMB_BAG_40,      OBJECT_GI_BOMBPOUCH,     GID_BOMB_BAG_40,      0x5A, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_BOMB_BAG_40),
        GET_ITEM(ITEM_GAUNTLETS_SILVER, OBJECT_GI_GLOVES,        GID_GAUNTLETS_SILVER, 0x5B, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_GAUNTLETS_SILVER),
        GET_ITEM(ITEM_GAUNTLETS_GOLD,   OBJECT_GI_GLOVES,        GID_GAUNTLETS_GOLD,   0x5C, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_GAUNTLETS_GOLD),
        GET_ITEM(ITEM_SCALE_SILVER,     OBJECT_GI_SCALE,         GID_SCALE_SILVER,     0xCD, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_SCALE_SILVER),
        GET_ITEM(ITEM_SCALE_GOLDEN,     OBJECT_GI_SCALE,         GID_SCALE_GOLDEN,     0xCE, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_SCALE_GOLDEN),
        GET_ITEM(ITEM_STONE_OF_AGONY,   OBJECT_GI_MAP,           GID_STONE_OF_AGONY,   0x68, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_STONE_OF_AGONY),
        GET_ITEM(ITEM_GERUDO_CARD,      OBJECT_GI_GERUDO,        GID_GERUDO_CARD,      0x7B, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_GERUDO_CARD),
        GET_ITEM(ITEM_OCARINA_FAIRY,    OBJECT_GI_OCARINA_0,     GID_OCARINA_FAIRY,    0x4A, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_OCARINA_FAIRY),
        GET_ITEM(ITEM_SEEDS,            OBJECT_GI_SEED,          GID_SEEDS,            0xDC, 0x50, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_SEEDS_5),
        GET_ITEM(ITEM_HEART_CONTAINER,  OBJECT_GI_HEARTS,        GID_HEART_CONTAINER,  0xC6, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_HEALTH,          MOD_NONE, GI_HEART_CONTAINER),
        GET_ITEM(ITEM_HEART_PIECE_2,    OBJECT_GI_HEARTS,        GID_HEART_PIECE,      0xC2, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_HEALTH,          MOD_NONE, GI_HEART_PIECE),
        GET_ITEM(ITEM_KEY_BOSS,         OBJECT_GI_BOSSKEY,       GID_KEY_BOSS,         0xC7, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_BOSS_KEY,        MOD_NONE, GI_KEY_BOSS),
        GET_ITEM(ITEM_COMPASS,          OBJECT_GI_COMPASS,       GID_COMPASS,          0x67, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_COMPASS),
        GET_ITEM(ITEM_DUNGEON_MAP,      OBJECT_GI_MAP,           GID_DUNGEON_MAP,      0x66, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_MAP),
        GET_ITEM(ITEM_KEY_SMALL,        OBJECT_GI_KEY,           GID_KEY_SMALL,        0x60, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_SMALL_KEY,       MOD_NONE, GI_KEY_SMALL),
        GET_ITEM(ITEM_MAGIC_SMALL,      OBJECT_GI_MAGICPOT,      GID_MAGIC_SMALL,      0x52, 0x6F, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_MAGIC_SMALL),
        GET_ITEM(ITEM_MAGIC_LARGE,      OBJECT_GI_MAGICPOT,      GID_MAGIC_LARGE,      0x52, 0x6E, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_MAGIC_LARGE),
        GET_ITEM(ITEM_WALLET_ADULT,     OBJECT_GI_PURSE,         GID_WALLET_ADULT,     0x5E, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_WALLET_ADULT),
        GET_ITEM(ITEM_WALLET_GIANT,     OBJECT_GI_PURSE,         GID_WALLET_GIANT,     0x5F, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_WALLET_GIANT),
        GET_ITEM(ITEM_WEIRD_EGG,        OBJECT_GI_EGG,           GID_EGG,              0x9A, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_WEIRD_EGG),
        GET_ITEM(ITEM_HEART,            OBJECT_GI_HEART,         GID_HEART,            0x55, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_HEART),
        GET_ITEM(ITEM_ARROWS_SMALL,     OBJECT_GI_ARROW,         GID_ARROWS_SMALL,     0xE6, 0x48, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_ARROWS_SMALL),
        GET_ITEM(ITEM_ARROWS_MEDIUM,    OBJECT_GI_ARROW,         GID_ARROWS_MEDIUM,    0xE6, 0x49, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_ARROWS_MEDIUM),
        GET_ITEM(ITEM_ARROWS_LARGE,     OBJECT_GI_ARROW,         GID_ARROWS_LARGE,     0xE6, 0x4A, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_ARROWS_LARGE),
        GET_ITEM(ITEM_RUPEE_GREEN,      OBJECT_GI_RUPY,          GID_RUPEE_GREEN,      0x6F, 0x00, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_RUPEE_GREEN),
        GET_ITEM(ITEM_RUPEE_BLUE,       OBJECT_GI_RUPY,          GID_RUPEE_BLUE,       0xCC, 0x01, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_RUPEE_BLUE),
        GET_ITEM(ITEM_RUPEE_RED,        OBJECT_GI_RUPY,          GID_RUPEE_RED,        0xF0, 0x02, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_RUPEE_RED),
        GET_ITEM(ITEM_HEART_CONTAINER,  OBJECT_GI_HEARTS,        GID_HEART_CONTAINER,  0xC6, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_HEALTH,          MOD_NONE, GI_HEART_CONTAINER_2),
        GET_ITEM(ITEM_MILK,             OBJECT_GI_MILK,          GID_MILK,             0x98, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_MILK),
        GET_ITEM(ITEM_MASK_GORON,       OBJECT_GI_GOLONMASK,     GID_MASK_GORON,       0x14, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_MASK_GORON),
        GET_ITEM(ITEM_MASK_ZORA,        OBJECT_GI_ZORAMASK,      GID_MASK_ZORA,        0x15, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_MASK_ZORA),
        GET_ITEM(ITEM_MASK_GERUDO,      OBJECT_GI_GERUDOMASK,    GID_MASK_GERUDO,      0x16, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_MASK_GERUDO),
        GET_ITEM(ITEM_BRACELET,         OBJECT_GI_BRACELET,      GID_BRACELET,         0x79, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BRACELET),
        GET_ITEM(ITEM_RUPEE_PURPLE,     OBJECT_GI_RUPY,          GID_RUPEE_PURPLE,     0xF1, 0x14, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_RUPEE_PURPLE),
        GET_ITEM(ITEM_RUPEE_GOLD,       OBJECT_GI_RUPY,          GID_RUPEE_GOLD,       0xF2, 0x13, CHEST_ANIM_SHORT, ITEM_CATEGORY_LESSER,          MOD_NONE, GI_RUPEE_GOLD),
        GET_ITEM(ITEM_SWORD_BGS,        OBJECT_GI_LONGSWORD,     GID_SWORD_BGS,        0x0C, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_SWORD_BGS),
        GET_ITEM(ITEM_ARROW_FIRE,       OBJECT_GI_M_ARROW,       GID_ARROW_FIRE,       0x70, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_ARROW_FIRE),
        GET_ITEM(ITEM_ARROW_ICE,        OBJECT_GI_M_ARROW,       GID_ARROW_ICE,        0x71, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_ARROW_ICE),
        GET_ITEM(ITEM_ARROW_LIGHT,      OBJECT_GI_M_ARROW,       GID_ARROW_LIGHT,      0x72, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_ARROW_LIGHT),
        GET_ITEM(ITEM_SKULL_TOKEN,      OBJECT_GI_SUTARU,        GID_SKULL_TOKEN,      0xB4, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_SKULLTULA_TOKEN, MOD_NONE, GI_SKULL_TOKEN),
        GET_ITEM(ITEM_DINS_FIRE,        OBJECT_GI_GODDESS,       GID_DINS_FIRE,        0xAD, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_DINS_FIRE),
        GET_ITEM(ITEM_FARORES_WIND,     OBJECT_GI_GODDESS,       GID_FARORES_WIND,     0xAE, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_FARORES_WIND),
        GET_ITEM(ITEM_NAYRUS_LOVE,      OBJECT_GI_GODDESS,       GID_NAYRUS_LOVE,      0xAF, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_NAYRUS_LOVE),
        GET_ITEM(ITEM_BULLET_BAG_30,    OBJECT_GI_DEKUPOUCH,     GID_BULLET_BAG,       0x07, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_BULLET_BAG_30),
        GET_ITEM(ITEM_BULLET_BAG_40,    OBJECT_GI_DEKUPOUCH,     GID_BULLET_BAG,       0x07, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_BULLET_BAG_40),
        GET_ITEM(ITEM_STICKS_5,         OBJECT_GI_STICK,         GID_STICK,            0x37, 0x0D, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_STICKS_5),
        GET_ITEM(ITEM_STICKS_10,        OBJECT_GI_STICK,         GID_STICK,            0x37, 0x0D, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_STICKS_10),
        GET_ITEM(ITEM_NUTS_5,           OBJECT_GI_NUTS,          GID_NUTS,             0x34, 0x0C, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_NUTS_5_2),
        GET_ITEM(ITEM_NUTS_10,          OBJECT_GI_NUTS,          GID_NUTS,             0x34, 0x0C, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_NUTS_10),
        GET_ITEM(ITEM_BOMB,             OBJECT_GI_BOMB_1,        GID_BOMB,             0x32, 0x59, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_BOMBS_1),
        GET_ITEM(ITEM_BOMBS_10,         OBJECT_GI_BOMB_1,        GID_BOMB,             0x32, 0x59, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_BOMBS_10),
        GET_ITEM(ITEM_BOMBS_20,         OBJECT_GI_BOMB_1,        GID_BOMB,             0x32, 0x59, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_BOMBS_20),
        GET_ITEM(ITEM_BOMBS_30,         OBJECT_GI_BOMB_1,        GID_BOMB,             0x32, 0x59, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_BOMBS_30),
        GET_ITEM(ITEM_SEEDS_30,         OBJECT_GI_SEED,          GID_SEEDS,            0xDC, 0x50, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_SEEDS_30),
        GET_ITEM(ITEM_BOMBCHUS_5,       OBJECT_GI_BOMB_2,        GID_BOMBCHU,          0x33, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BOMBCHUS_5),
        GET_ITEM(ITEM_BOMBCHUS_20,      OBJECT_GI_BOMB_2,        GID_BOMBCHU,          0x33, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_MAJOR,           MOD_NONE, GI_BOMBCHUS_20),
        GET_ITEM(ITEM_FISH,             OBJECT_GI_FISH,          GID_FISH,             0x47, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_FISH),
        GET_ITEM(ITEM_BUG,              OBJECT_GI_INSECT,        GID_BUG,              0x7A, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_BUGS),
        GET_ITEM(ITEM_BLUE_FIRE,        OBJECT_GI_FIRE,          GID_BLUE_FIRE,        0x5D, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_BLUE_FIRE),
        GET_ITEM(ITEM_POE,              OBJECT_GI_GHOST,         GID_POE,              0x97, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_POE),
        GET_ITEM(ITEM_BIG_POE,          OBJECT_GI_GHOST,         GID_BIG_POE,          0xF9, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_JUNK,            MOD_NONE, GI_BIG_POE),
        GET_ITEM(ITEM_KEY_SMALL,        OBJECT_GI_KEY,           GID_KEY_SMALL,        0xF3, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_SMALL_KEY,       MOD_NONE, GI_DOOR_KEY),
        GET_ITEM(ITEM_RUPEE_GREEN,      OBJECT_GI_RUPY,          GID_RUPEE_GREEN,      0xF4, 0x00, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_RUPEE_GREEN_LOSE),
        GET_ITEM(ITEM_RUPEE_BLUE,       OBJECT_GI_RUPY,          GID_RUPEE_BLUE,       0xF5, 0x01, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_RUPEE_BLUE_LOSE),
        GET_ITEM(ITEM_RUPEE_RED,        OBJECT_GI_RUPY,          GID_RUPEE_RED,        0xF6, 0x02, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_RUPEE_RED_LOSE),
        GET_ITEM(ITEM_RUPEE_PURPLE,     OBJECT_GI_RUPY,          GID_RUPEE_PURPLE,     0xF7, 0x14, CHEST_ANIM_SHORT, ITEM_CATEGORY_JUNK,            MOD_NONE, GI_RUPEE_PURPLE_LOSE),
        GET_ITEM(ITEM_HEART_PIECE_2,    OBJECT_GI_HEARTS,        GID_HEART_PIECE,      0xFA, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_HEALTH,          MOD_NONE, GI_HEART_PIECE_WIN),
        GET_ITEM(ITEM_STICK_UPGRADE_20, OBJECT_GI_STICK,         GID_STICK,            0x90, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_LESSER,          MOD_NONE, GI_STICK_UPGRADE_20),
        GET_ITEM(ITEM_STICK_UPGRADE_30, OBJECT_GI_STICK,         GID_STICK,            0x91, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_LESSER,          MOD_NONE, GI_STICK_UPGRADE_30),
        GET_ITEM(ITEM_NUT_UPGRADE_30,   OBJECT_GI_NUTS,          GID_NUTS,             0xA7, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_LESSER,          MOD_NONE, GI_NUT_UPGRADE_30),
        GET_ITEM(ITEM_NUT_UPGRADE_40,   OBJECT_GI_NUTS,          GID_NUTS,             0xA8, 0x80, CHEST_ANIM_SHORT, ITEM_CATEGORY_LESSER,          MOD_NONE, GI_NUT_UPGRADE_40),
        GET_ITEM(ITEM_BULLET_BAG_50,    OBJECT_GI_DEKUPOUCH,     GID_BULLET_BAG_50,    0x6C, 0x80, CHEST_ANIM_LONG,  ITEM_CATEGORY_LESSER,          MOD_NONE, GI_BULLET_BAG_50),
        GET_ITEM_NONE,
        GET_ITEM_NONE,
        GET_ITEM_NONE // GI_MAX - if you need to add to this table insert it before this entry.
        // clang-format on
};

std::unordered_map<ItemID, GetItemID> ItemIDtoGetItemIDMap{
    { ITEM_ARROWS_LARGE, GI_ARROWS_LARGE },
    { ITEM_ARROWS_MEDIUM, GI_ARROWS_MEDIUM },
    { ITEM_ARROWS_SMALL, GI_ARROWS_SMALL },
    { ITEM_ARROW_FIRE, GI_ARROW_FIRE },
    { ITEM_ARROW_ICE, GI_ARROW_ICE },
    { ITEM_ARROW_LIGHT, GI_ARROW_LIGHT },
    { ITEM_BEAN, GI_BEAN },
    { ITEM_BIG_POE, GI_BIG_POE },
    { ITEM_BLUE_FIRE, GI_BLUE_FIRE },
    { ITEM_BOMB, GI_BOMBS_1 },
    { ITEM_BOMBCHU, GI_BOMBCHUS_10 },
    { ITEM_BOMBCHUS_20, GI_BOMBCHUS_20 },
    { ITEM_BOMBCHUS_5, GI_BOMBCHUS_5 },
    { ITEM_BOMBS_10, GI_BOMBS_10 },
    { ITEM_BOMBS_20, GI_BOMBS_20 },
    { ITEM_BOMBS_30, GI_BOMBS_30 },
    { ITEM_BOMBS_5, GI_BOMBS_5 },
    { ITEM_BOMB_BAG_20, GI_BOMB_BAG_20 },
    { ITEM_BOMB_BAG_30, GI_BOMB_BAG_30 },
    { ITEM_BOMB_BAG_40, GI_BOMB_BAG_40 },
    { ITEM_BOOMERANG, GI_BOOMERANG },
    { ITEM_BOOTS_HOVER, GI_BOOTS_HOVER },
    { ITEM_BOOTS_IRON, GI_BOOTS_IRON },
    { ITEM_BOTTLE, GI_BOTTLE },
    { ITEM_BOW, GI_BOW },
    { ITEM_BRACELET, GI_BRACELET },
    { ITEM_BUG, GI_BUGS },
    { ITEM_BULLET_BAG_30, GI_BULLET_BAG_30 },
    { ITEM_BULLET_BAG_40, GI_BULLET_BAG_40 },
    { ITEM_BULLET_BAG_50, GI_BULLET_BAG_50 },
    { ITEM_CHICKEN, GI_CHICKEN },
    { ITEM_CLAIM_CHECK, GI_CLAIM_CHECK },
    { ITEM_COJIRO, GI_COJIRO },
    { ITEM_COMPASS, GI_COMPASS },
    { ITEM_DINS_FIRE, GI_DINS_FIRE },
    { ITEM_DUNGEON_MAP, GI_MAP },
    { ITEM_EYEDROPS, GI_EYEDROPS },
    { ITEM_FAIRY, GI_FAIRY },
    { ITEM_FARORES_WIND, GI_FARORES_WIND },
    { ITEM_FISH, GI_FISH },
    { ITEM_FROG, GI_FROG },
    { ITEM_GAUNTLETS_GOLD, GI_GAUNTLETS_GOLD },
    { ITEM_GAUNTLETS_SILVER, GI_GAUNTLETS_SILVER },
    { ITEM_GERUDO_CARD, GI_GERUDO_CARD },
    { ITEM_HAMMER, GI_HAMMER },
    { ITEM_HEART, GI_HEART },
    { ITEM_HEART_CONTAINER, GI_HEART_CONTAINER },
    { ITEM_HEART_CONTAINER, GI_HEART_CONTAINER_2 },
    { ITEM_HEART_PIECE_2, GI_HEART_PIECE },
    { ITEM_HEART_PIECE_2, GI_HEART_PIECE_WIN },
    { ITEM_HOOKSHOT, GI_HOOKSHOT },
    { ITEM_KEY_BOSS, GI_KEY_BOSS },
    { ITEM_KEY_SMALL, GI_DOOR_KEY },
    { ITEM_KEY_SMALL, GI_KEY_SMALL },
    { ITEM_LENS, GI_LENS },
    { ITEM_LETTER_RUTO, GI_LETTER_RUTO },
    { ITEM_LETTER_ZELDA, GI_LETTER_ZELDA },
    { ITEM_LONGSHOT, GI_LONGSHOT },
    { ITEM_MAGIC_LARGE, GI_MAGIC_LARGE },
    { ITEM_MAGIC_SMALL, GI_MAGIC_SMALL },
    { ITEM_MASK_BUNNY, GI_MASK_BUNNY },
    { ITEM_MASK_GERUDO, GI_MASK_GERUDO },
    { ITEM_MASK_GORON, GI_MASK_GORON },
    { ITEM_MASK_KEATON, GI_MASK_KEATON },
    { ITEM_MASK_SKULL, GI_MASK_SKULL },
    { ITEM_MASK_SPOOKY, GI_MASK_SPOOKY },
    { ITEM_MASK_TRUTH, GI_MASK_TRUTH },
    { ITEM_MASK_ZORA, GI_MASK_ZORA },
    { ITEM_MILK, GI_MILK },
    { ITEM_MILK_BOTTLE, GI_MILK_BOTTLE },
    { ITEM_NAYRUS_LOVE, GI_NAYRUS_LOVE },
    { ITEM_NUT, GI_NUTS_5 },
    { ITEM_NUTS_10, GI_NUTS_10 },
    { ITEM_NUTS_5, GI_NUTS_5 },
    { ITEM_NUTS_5, GI_NUTS_5_2 },
    { ITEM_NUT_UPGRADE_30, GI_NUT_UPGRADE_30 },
    { ITEM_NUT_UPGRADE_40, GI_NUT_UPGRADE_40 },
    { ITEM_OCARINA_FAIRY, GI_OCARINA_FAIRY },
    { ITEM_OCARINA_TIME, GI_OCARINA_OOT },
    { ITEM_ODD_MUSHROOM, GI_ODD_MUSHROOM },
    { ITEM_ODD_POTION, GI_ODD_POTION },
    { ITEM_POCKET_CUCCO, GI_POCKET_CUCCO },
    { ITEM_POCKET_EGG, GI_POCKET_EGG },
    { ITEM_POE, GI_POE },
    { ITEM_POTION_BLUE, GI_POTION_BLUE },
    { ITEM_POTION_GREEN, GI_POTION_GREEN },
    { ITEM_POTION_RED, GI_POTION_RED },
    { ITEM_PRESCRIPTION, GI_PRESCRIPTION },
    { ITEM_QUIVER_40, GI_QUIVER_40 },
    { ITEM_QUIVER_50, GI_QUIVER_50 },
    { ITEM_RUPEE_BLUE, GI_RUPEE_BLUE },
    { ITEM_RUPEE_BLUE, GI_RUPEE_BLUE_LOSE },
    { ITEM_RUPEE_GOLD, GI_RUPEE_GOLD },
    { ITEM_RUPEE_GREEN, GI_RUPEE_GREEN },
    { ITEM_RUPEE_GREEN, GI_RUPEE_GREEN_LOSE },
    { ITEM_RUPEE_PURPLE, GI_RUPEE_PURPLE },
    { ITEM_RUPEE_PURPLE, GI_RUPEE_PURPLE_LOSE },
    { ITEM_RUPEE_RED, GI_RUPEE_RED },
    { ITEM_RUPEE_RED, GI_RUPEE_RED_LOSE },
    { ITEM_SAW, GI_SAW },
    { ITEM_SCALE_GOLDEN, GI_SCALE_GOLDEN },
    { ITEM_SCALE_SILVER, GI_SCALE_SILVER },
    { ITEM_SEEDS, GI_SEEDS_5 },
    { ITEM_SEEDS_30, GI_SEEDS_30 },
    { ITEM_SHIELD_DEKU, GI_SHIELD_DEKU },
    { ITEM_SHIELD_HYLIAN, GI_SHIELD_HYLIAN },
    { ITEM_SHIELD_MIRROR, GI_SHIELD_MIRROR },
    { ITEM_SKULL_TOKEN, GI_SKULL_TOKEN },
    { ITEM_SLINGSHOT, GI_SLINGSHOT },
    { ITEM_STICK, GI_STICKS_1 },
    { ITEM_STICKS_10, GI_STICKS_10 },
    { ITEM_STICKS_5, GI_STICKS_5 },
    { ITEM_STICK_UPGRADE_20, GI_STICK_UPGRADE_20 },
    { ITEM_STICK_UPGRADE_30, GI_STICK_UPGRADE_30 },
    { ITEM_STONE_OF_AGONY, GI_STONE_OF_AGONY },
    { ITEM_SWORD_BGS, GI_SWORD_BGS },
    { ITEM_SWORD_BGS, GI_SWORD_KNIFE },
    { ITEM_SWORD_BROKEN, GI_SWORD_BROKEN },
    { ITEM_SWORD_KOKIRI, GI_SWORD_KOKIRI },
    { ITEM_TUNIC_GORON, GI_TUNIC_GORON },
    { ITEM_TUNIC_ZORA, GI_TUNIC_ZORA },
    { ITEM_WALLET_ADULT, GI_WALLET_ADULT },
    { ITEM_WALLET_GIANT, GI_WALLET_GIANT },
    { ITEM_WEIRD_EGG, GI_WEIRD_EGG },
};

extern "C" GetItemID RetrieveGetItemIDFromItemID(ItemID itemID) {
    if (ItemIDtoGetItemIDMap.contains(itemID)) {
        return ItemIDtoGetItemIDMap.at(itemID);
    }
    return GI_MAX;
}

extern "C" void OTRExtScanner() {
    auto lst = *Ship::Context::GetInstance()->GetResourceManager()->GetArchiveManager()->ListFiles().get();

    for (auto& rPath : lst) {
        std::vector<std::string> raw = StringHelper::Split(rPath, ".");
        std::string ext = raw[raw.size() - 1];
        std::string nPath = rPath.substr(0, rPath.size() - (ext.size() + 1));
        replace(nPath.begin(), nPath.end(), '\\', '/');

        ExtensionCache[nPath] = { rPath, ext };
    }
}

// Read the port version from an OTR file
OTRVersion ReadPortVersionFromOTR(std::string otrPath) {
    OTRVersion version = {};

    // Use a temporary archive instance to load the otr and read the version file
    auto archive = std::make_shared<Ship::O2rArchive>(otrPath);
    if (archive->Open()) {
        auto t = archive->LoadFile("portVersion");
        if (t != nullptr && t->IsLoaded) {
            auto stream = std::make_shared<Ship::MemoryStream>(t->Buffer->data(), t->Buffer->size());
            auto reader = std::make_shared<Ship::BinaryReader>(stream);
            Ship::Endianness endianness = (Ship::Endianness)reader->ReadUByte();
            reader->SetEndianness(endianness);
            version.major = reader->ReadUInt16();
            version.minor = reader->ReadUInt16();
            version.patch = reader->ReadUInt16();
        }
    }

    return version;
}

// Checks the program version stored in the otr and compares the major value to soh
// For Windows/Mac/Linux if the version doesn't match, offer to
OTRVersion DetectOTRVersion(std::string fileName, bool isMQ) {
    bool isOtrOld = false;
    std::string otrPath = Ship::Context::LocateFileAcrossAppDirs(fileName, appShortName);

    // Doesn't exist so nothing to do here
    if (!std::filesystem::exists(otrPath)) {
        return { INT16_MAX, INT16_MAX, INT16_MAX };
    }

    return ReadPortVersionFromOTR(otrPath);
}

extern "C" void Messagebox_ShowErrorBox(char* title, char* body) {
    Extractor::ShowErrorBox(title, body);
}

bool VerifyArchiveVersion(OTRVersion version) {
    return version.major != INT16_MAX && version.major != gBuildVersionMajor;
}

extern "C" void InitOTR(int argc, char* argv[]) {
    OTRGlobals::Instance = new OTRGlobals();
    OTRGlobals::Instance->RunExtract(argc, argv);

    OTRGlobals::Instance->Initialize();
    InitColViewer();
    SaveManager::Instance = new SaveManager();

    SohGui::SetupMenuElements();

    ActorDB::Instance = new ActorDB();
    OTRMessage_Init();
    OTRAudio_Init();
    OTRExtScanner();
    ActorDB::AddBuiltInCustomActors();
    Ship::Context::GetInstance()->GetFileDropMgr()->RegisterDropHandler(SoH_HandleConfigDrop);

    RegisterImGuiItemIcons();

    srand(time(NULL));
}

extern "C" void SaveManager_ThreadPoolWait() {
    SaveManager::Instance->ThreadPoolWait();
}

extern "C" void DeinitOTR() {
    SaveManager_ThreadPoolWait();
    OTRAudio_Exit();

    // Destroying gui here because we have shared ptrs to LUS objects which output to SPDLOG which is destroyed before
    // these shared ptrs.
    SohGui::Destroy();
    sohFast3dWindow = nullptr;

    OTRGlobals::Instance->context = nullptr;
}

#ifdef _WIN32
extern "C" uint64_t GetFrequency() {
    LARGE_INTEGER nFreq;

    QueryPerformanceFrequency(&nFreq);

    return nFreq.QuadPart;
}

extern "C" uint64_t GetPerfCounter() {
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);

    return ticks.QuadPart;
}
#else
extern "C" uint64_t GetFrequency() {
    return 1000; // sec -> ms
}

extern "C" uint64_t GetPerfCounter() {
    struct timespec monotime;
    clock_gettime(CLOCK_MONOTONIC, &monotime);

    uint64_t remainingMs = (monotime.tv_nsec / 1000000);

    // in milliseconds
    return monotime.tv_sec * 1000 + remainingMs;
}
#endif

extern "C" uint64_t GetUnixTimestamp() {
    auto time = std::chrono::system_clock::now();
    auto since_epoch = time.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
    return (uint64_t)millis.count();
}

extern "C" void Graph_StartFrame() {
#ifndef __WIIU__
    using Ship::KbScancode;
    int32_t dwScancode = OTRGlobals::Instance->context->GetWindow()->GetLastScancode();
    OTRGlobals::Instance->context->GetWindow()->SetLastScancode(-1);

    switch (dwScancode) {
        case KbScancode::LUS_KB_F1: {
            ToggleColViewer();
            break;
        }
    }
#endif
}

void RunCommands(Gfx* Commands, const std::vector<std::unordered_map<Mtx*, MtxF>>& mtx_replacements) {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(OTRGlobals::Instance->context->GetWindow());

    if (wnd == nullptr) {
        return;
    }

    // Process window events for resize, mouse, keyboard events
    wnd->HandleEvents();

    auto intp = wnd->GetInterpreterWeak().lock().get();
    intp->mInterpolationIndex = 0;

    UIWidgets::Colors themeColor =
        static_cast<UIWidgets::Colors>(CVarGetInteger(CVAR_SETTING("Menu.Theme"), UIWidgets::Colors::LightBlue));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, UIWidgets::ColorValues.at(themeColor));
    for (const auto& m : mtx_replacements) {
        wnd->DrawAndRunGraphicsCommands(Commands, m);
        intp->mInterpolationIndex++;
    }
    ImGui::PopStyleColor();
}

// C->C++ Bridge
extern "C" void Graph_ProcessGfxCommands(Gfx* commands) {
    {
        std::unique_lock<std::mutex> Lock(audio.mutex);
        audio.processing = true;
    }

    audio.cv_to_thread.notify_one();
    std::vector<std::unordered_map<Mtx*, MtxF>> mtx_replacements;
    int target_fps = OTRGlobals::Instance->GetInterpolationFPS();
    static int last_fps;
    static int last_update_rate;
    static int time;
    int fps = target_fps;
    int original_fps = 60 / R_UPDATE_RATE;
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());

    if (target_fps == 20 || original_fps > target_fps) {
        fps = original_fps;
    }

    if (last_fps != fps || last_update_rate != R_UPDATE_RATE) {
        time = 0;
    }

    // time_base = fps * original_fps (one second)
    int next_original_frame = fps;

    while (time + original_fps <= next_original_frame) {
        time += original_fps;
        if (time != next_original_frame) {
            mtx_replacements.push_back(FrameInterpolation_Interpolate((float)time / next_original_frame));
        } else {
            mtx_replacements.emplace_back();
        }
    }

    time -= fps;

    if (wnd != nullptr) {
        wnd->SetTargetFps(fps);
    }

    // When the gfx debugger is active, only run with the final mtx
    if (GfxDebuggerIsDebugging()) {
        mtx_replacements.clear();
        mtx_replacements.emplace_back();
    }

    RunCommands(commands, mtx_replacements);

    last_fps = fps;
    last_update_rate = R_UPDATE_RATE;

    {
        std::unique_lock<std::mutex> Lock(audio.mutex);
        while (audio.processing) {
            audio.cv_from_thread.wait(Lock);
        }
    }

    // OTRTODO: FIGURE OUT END FRAME POINT
    /* if (OTRGlobals::Instance->context->lastScancode != -1)
         OTRGlobals::Instance->context->lastScancode = -1;*/
}

float divisor_num = 0.0f;

extern "C" void OTRGetPixelDepthPrepare(float x, float y) {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
    if (wnd == nullptr) {
        return;
    }

    wnd->GetPixelDepthPrepare(x, y);
}

extern "C" uint16_t OTRGetPixelDepth(float x, float y) {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
    if (wnd == nullptr) {
        return 0;
    }

    return wnd->GetPixelDepth(x, y);
}

std::map<std::string, SoundFontSample*> cachedCustomSFs;

extern "C" SoundFontSample* ReadCustomSample(const char* path) {
    return nullptr;
    /*
    if (!ExtensionCache.contains(path))
        return nullptr;

    ExtensionEntry entry = ExtensionCache[path];

    auto sampleRaw = Ship::Context::GetInstance()->GetResourceManager()->LoadFile(entry.path);
    uint32_t* strem = (uint32_t*)sampleRaw->Buffer.get();
    uint8_t* strem2 = (uint8_t*)strem;

    SoundFontSample* sampleC = new SoundFontSample;

    if (entry.ext == "wav") {
        drwav_uint32 channels;
        drwav_uint32 sampleRate;
        drwav_uint64 totalPcm;
        drmp3_int16* pcmData =
            drwav_open_memory_and_read_pcm_frames_s16(strem2, sampleRaw->BufferSize, &channels, &sampleRate, &totalPcm,
    NULL); sampleC->size = totalPcm; sampleC->sampleAddr = (uint8_t*)pcmData; sampleC->codec = CODEC_S16;

        sampleC->loop = new AdpcmLoop;
        sampleC->loop->start = 0;
        sampleC->loop->end = sampleC->size - 1;
        sampleC->loop->count = 0;
        sampleC->sampleRateMagicValue = 'RIFF';
        sampleC->sampleRate = sampleRate;

        cachedCustomSFs[path] = sampleC;
        return sampleC;
    } else if (entry.ext == "mp3") {
        drmp3_config mp3Info;
        drmp3_uint64 totalPcm;
        drmp3_int16* pcmData =
            drmp3_open_memory_and_read_pcm_frames_s16(strem2, sampleRaw->BufferSize, &mp3Info, &totalPcm, NULL);

        sampleC->size = totalPcm * mp3Info.channels * sizeof(short);
        sampleC->sampleAddr = (uint8_t*)pcmData;
        sampleC->codec = CODEC_S16;

        sampleC->loop = new AdpcmLoop;
        sampleC->loop->start = 0;
        sampleC->loop->end = sampleC->size;
        sampleC->loop->count = 0;
        sampleC->sampleRateMagicValue = 'RIFF';
        sampleC->sampleRate = mp3Info.sampleRate;

        cachedCustomSFs[path] = sampleC;
        return sampleC;
    }

    return nullptr;
    */
}

ImFont* OTRGlobals::CreateFontWithSize(float size, std::string fontPath, bool isJapaneseFont) {
    auto mImGuiIo = &ImGui::GetIO();
    ImFont* font;
    if (fontPath == "") {
        ImFontConfig fontCfg = ImFontConfig();
        fontCfg.OversampleH = fontCfg.OversampleV = 1;
        fontCfg.PixelSnapH = true;
        fontCfg.SizePixels = size;
        font = mImGuiIo->Fonts->AddFontDefault(&fontCfg);
    } else {
        auto initData = std::make_shared<Ship::ResourceInitData>();
        initData->Format = RESOURCE_FORMAT_BINARY;
        initData->Type = static_cast<uint32_t>(RESOURCE_TYPE_FONT);
        initData->ResourceVersion = 0;
        initData->Path = fontPath;
        std::shared_ptr<Ship::Font> fontData = std::static_pointer_cast<Ship::Font>(
            Ship::Context::GetInstance()->GetResourceManager()->LoadResource(fontPath, false, initData));
        ImFontConfig fontConf;
        fontConf.FontDataOwnedByAtlas = false;
        const ImWchar* glyph_ranges = isJapaneseFont ? mImGuiIo->Fonts->GetGlyphRangesJapanese() : nullptr;
        font = mImGuiIo->Fonts->AddFontFromMemoryTTF(fontData->Data, fontData->DataSize, size, &fontConf, glyph_ranges);
    }
    // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly
    float iconFontSize = size * 2.0f / 3.0f;
    static const ImWchar sIconsRanges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig iconsConfig;
    iconsConfig.MergeMode = true;
    iconsConfig.PixelSnapH = true;
    iconsConfig.GlyphMinAdvanceX = iconFontSize;
    mImGuiIo->Fonts->AddFontFromMemoryCompressedBase85TTF(fontawesome_compressed_data_base85, iconFontSize,
                                                          &iconsConfig, sIconsRanges);
    return font;
}

std::filesystem::path GetSaveFile(std::shared_ptr<Ship::Config> Conf) {
    const std::string fileName =
        Conf->GetString("Game.SaveName", Ship::Context::GetPathRelativeToAppDirectory("oot_save.sav"));
    std::filesystem::path saveFile = std::filesystem::absolute(fileName);

    if (!exists(saveFile.parent_path())) {
        create_directories(saveFile.parent_path());
    }

    return saveFile;
}

std::filesystem::path GetSaveFile() {
    const std::shared_ptr<Ship::Config> pConf = OTRGlobals::Instance->context->GetConfig();

    return GetSaveFile(pConf);
}

void OTRGlobals::CheckSaveFile(size_t sramSize) const {
    const std::shared_ptr<Ship::Config> pConf = Instance->context->GetConfig();

    std::filesystem::path savePath = GetSaveFile(pConf);
    std::fstream saveFile(savePath, std::fstream::in | std::fstream::out | std::fstream::binary);
    if (saveFile.fail()) {
        saveFile.open(savePath, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::app);
        for (size_t i = 0; i < sramSize; i++) {
            saveFile.write("\0", 1);
        }
    }
    saveFile.close();
}

extern "C" void Ctx_ReadSaveFile(uintptr_t addr, void* dramAddr, size_t size) {
    SaveManager::ReadSaveFile(GetSaveFile(), addr, dramAddr, size);
}

extern "C" void Ctx_WriteSaveFile(uintptr_t addr, void* dramAddr, size_t size) {
    SaveManager::WriteSaveFile(GetSaveFile(), addr, dramAddr, size);
}

std::wstring StringToU16(const std::string& s) {
    std::vector<unsigned long> result;
    size_t i = 0;

    while (i < s.size()) {
        unsigned long uni;
        size_t nbytes = 0;
        bool error = false;
        unsigned char c = s[i++];
        if (c < 0x80) { // ascii
            uni = c;
            nbytes = 0;
        } else if (c == GFXP_HIRAGANA_CHAR) { // Start Hiragana Mode
            uni = c;
            nbytes = 0;
        } else if (c == GFXP_KATAKANA_CHAR) { // Start Katakana Mode
            uni = c;
            nbytes = 0;
        } else if (c <= 0xBF) { // Invalid Characters (Skipped)
            nbytes = 0;
            uni = '\1';
        } else if (c <= 0xDF) {
            uni = c & 0x1F;
            nbytes = 1;
        } else if (c <= 0xEF) {
            uni = c & 0x0F;
            nbytes = 2;
        } else if (c <= 0xF7) {
            uni = c & 0x07;
            nbytes = 3;
        }
        for (size_t j = 0; j < nbytes; ++j) {
            unsigned char c = s[i++];
            uni <<= 6;
            uni += c & 0x3F;
        }
        if (uni != '\1')
            result.push_back(uni);
    }
    std::wstring utf16;
    for (size_t i = 0; i < result.size(); ++i) {
        unsigned long uni = result[i];
        if (uni <= 0xFFFF) {
            utf16 += (wchar_t)uni;
        } else {
            uni -= 0x10000;
            utf16 += (wchar_t)((uni >> 10) + 0xD800);
            utf16 += (wchar_t)((uni & 0x3FF) + 0xDC00);
        }
    }
    return utf16;
}

extern "C" void OTRGfxPrint(const char* str, void* printer, void (*printImpl)(void*, char)) {
    const std::vector<uint32_t> hira1 = {
        u'を', u'ぁ', u'ぃ', u'ぅ', u'ぇ', u'ぉ', u'ゃ', u'ゅ', u'ょ', u'っ', u'-',  u'あ', u'い',
        u'う', u'え', u'お', u'か', u'き', u'く', u'け', u'こ', u'さ', u'し', u'す', u'せ', u'そ',
    };

    const std::vector<uint32_t> hira2 = {
        u'た', u'ち', u'つ', u'て', u'と', u'な', u'に', u'ぬ', u'ね', u'の', u'は', u'ひ', u'ふ', u'へ', u'ほ', u'ま',
        u'み', u'む', u'め', u'も', u'や', u'ゆ', u'よ', u'ら', u'り', u'る', u'れ', u'ろ', u'わ', u'ん', u'゛', u'゜',
    };

    const std::vector<uint32_t> kata1 = {
        u'ヲ', u'ァ', u'ィ', u'ゥ', u'ェ', u'ォ', u'ャ', u'ュ', u'ョ', u'ッ', u'ー',
    };

    const std::vector<uint32_t> kata2 = {
        u'ア', u'イ', u'ウ', u'エ', u'オ', u'カ', u'キ', u'ク', u'ケ', u'コ', u'サ', u'シ', u'ス', u'セ', u'ソ',
        u'タ', u'チ', u'ツ', u'テ', u'ト', u'ナ', u'ニ', u'ヌ', u'ネ', u'ノ', u'ハ', u'ヒ', u'フ', u'ヘ', u'ホ',
        u'マ', u'ミ', u'ム', u'メ', u'モ', u'ヤ', u'ユ', u'ヨ', u'ラ', u'リ', u'ル', u'レ', u'ロ', u'ワ', u'ン',
    };

    std::wstring wstr = StringToU16(str);
    bool hiraganaMode = false;

    for (const auto& c : wstr) {
        if (c < 0x80) {
            printImpl(printer, c);
        } else if (c == GFXP_HIRAGANA_CHAR) {
            hiraganaMode = true;
        } else if (c == GFXP_KATAKANA_CHAR) {
            hiraganaMode = false;
        } else if (c >= u'｡' && c <= u'ﾟ') { // katakana (hankaku)
            if (hiraganaMode && c >= u'ｦ' && c <= u'ｿ') {
                printImpl(printer, c - 0xFEC0 - 0x20); // Hiragana Mode, Block 1
            } else if (hiraganaMode && c >= u'ﾀ' && c <= u'ﾝ') {
                printImpl(printer, c - 0xFEC0 + 0x20); // Hiragana Mode, Block 2
            } else {
                printImpl(printer, c - 0xFEC0);
            }
        } else if (c == u'　') { // zenkaku space
            printImpl(printer, u' ');
        } else {
            auto it = std::find(hira1.begin(), hira1.end(), c);
            if (it != hira1.end()) { // hiragana block 1
                printImpl(printer, 0x86 + std::distance(hira1.begin(), it));
            }

            auto it2 = std::find(hira2.begin(), hira2.end(), c);
            if (it2 != hira2.end()) { // hiragana block 2
                printImpl(printer, 0xe0 + std::distance(hira2.begin(), it2));
            }

            auto it3 = std::find(kata1.begin(), kata1.end(), c);
            if (it3 != kata1.end()) { // katakana zenkaku block 1
                printImpl(printer, 0xa6 + std::distance(kata1.begin(), it3));
            }

            auto it4 = std::find(kata2.begin(), kata2.end(), c);
            if (it4 != kata2.end()) { // katakana zenkaku block 2
                printImpl(printer, 0xb1 + std::distance(kata2.begin(), it4));
            }
        }
    }
}

extern "C" uint32_t OTRGetCurrentWidth() {
    return OTRGlobals::Instance->context->GetWindow()->GetWidth();
}

extern "C" uint32_t OTRGetCurrentHeight() {
    return OTRGlobals::Instance->context->GetWindow()->GetHeight();
}

Color_RGB8 GetColorForControllerLED() {
    auto brightness = CVarGetFloat(CVAR_SETTING("LEDBrightness"), 1.0f) / 1.0f;
    Color_RGB8 color = { 0, 0, 0 };
    if (brightness > 0.0f) {
        LEDColorSource source =
            static_cast<LEDColorSource>(CVarGetInteger(CVAR_SETTING("LEDColorSource"), LED_SOURCE_TUNIC_ORIGINAL));
        bool criticalOverride = CVarGetInteger(CVAR_SETTING("LEDCriticalOverride"), 1);
        if (gPlayState && source == LED_SOURCE_TUNIC_ORIGINAL) {
            switch (CUR_EQUIP_VALUE(EQUIP_TYPE_TUNIC)) {
                case EQUIP_VALUE_TUNIC_KOKIRI:
                    color = kokiriColor;
                    break;
                case EQUIP_VALUE_TUNIC_GORON:
                    color = goronColor;
                    break;
                case EQUIP_VALUE_TUNIC_ZORA:
                    color = zoraColor;
                    break;
            }
        }
        if (gPlayState && source == LED_SOURCE_NAVI_ORIGINAL) {
            Actor* arrowPointedActor = gPlayState->actorCtx.targetCtx.arrowPointedActor;
            if (arrowPointedActor) {
                ActorCategory category = (ActorCategory)arrowPointedActor->category;
                color = LEDColorDefaultNaviColorList[category].inner;
            } else { // No target actor.
                color = LEDColorDefaultNaviColorList[ACTORCAT_PLAYER].inner;
            }
        }
        if (source == LED_SOURCE_CUSTOM) {
            color = CVarGetColor24(CVAR_SETTING("LEDPort1Color"), { 255, 255, 255 });
        }
        if (gPlayState && (criticalOverride || source == LED_SOURCE_HEALTH)) {
            if (HealthMeter_IsCritical()) {
                color = { 0xFF, 0, 0 };
            } else if (gSaveContext.healthCapacity != 0 && source == LED_SOURCE_HEALTH) {
                if (gSaveContext.health / (float)gSaveContext.healthCapacity <= 0.4f) {
                    color = { 0xFF, 0xFF, 0 };
                } else {
                    color = { 0, 0xFF, 0 };
                }
            }
        }
        color.r = color.r * brightness;
        color.g = color.g * brightness;
        color.b = color.b * brightness;
    }

    return color;
}

extern "C" void OTRControllerCallback(uint8_t rumble) {
    // We call this every tick, SDL accounts for this use and prevents driver spam
    // https://github.com/libsdl-org/SDL/blob/f17058b562c8a1090c0c996b42982721ace90903/src/joystick/SDL_joystick.c#L1114-L1144
    Ship::Context::GetInstance()->GetControlDeck()->GetControllerByPort(0)->GetLED()->SetLEDColor(
        GetColorForControllerLED());

    static std::shared_ptr<SohInputEditorWindow> controllerConfigWindow = nullptr;
    if (controllerConfigWindow == nullptr) {
        controllerConfigWindow = std::dynamic_pointer_cast<SohInputEditorWindow>(
            Ship::Context::GetInstance()->GetWindow()->GetGui()->GetGuiWindow("Controller Configuration"));
    } else if (controllerConfigWindow->TestingRumble()) {
        return;
    }

    if (rumble) {
        Ship::Context::GetInstance()->GetControlDeck()->GetControllerByPort(0)->GetRumble()->StartRumble();
    } else {
        Ship::Context::GetInstance()->GetControlDeck()->GetControllerByPort(0)->GetRumble()->StopRumble();
    }
}

extern "C" float OTRGetAspectRatio() {
    return Ship::Context::GetInstance()->GetWindow()->GetAspectRatio();
}

extern "C" float OTRGetDimensionFromLeftEdge(float v) {
    return (SCREEN_WIDTH / 2 - SCREEN_HEIGHT / 2 * OTRGetAspectRatio() + (v));
}

extern "C" float OTRGetDimensionFromRightEdge(float v) {
    return (SCREEN_WIDTH / 2 + SCREEN_HEIGHT / 2 * OTRGetAspectRatio() - (SCREEN_WIDTH - v));
}

// Gets the width of the current render target area
extern "C" uint32_t OTRGetGameRenderWidth() {
    auto fastWnd = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
    auto intP = fastWnd->GetInterpreterWeak().lock();

    if (!intP) {
        assert(false && "Lost reference to Fast::Interpreter");
        return 320;
    }

    uint32_t height, width;
    intP->GetCurDimensions(&width, &height);

    return width;
}

// Gets the height of the current render target area
extern "C" uint32_t OTRGetGameRenderHeight() {
    auto fastWnd = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow());
    auto intP = fastWnd->GetInterpreterWeak().lock();

    if (!intP) {
        assert(false && "Lost reference to Fast::Interpreter");
        return 240;
    }

    uint32_t height, width;
    intP->GetCurDimensions(&width, &height);

    return height;
}

f32 floorf(f32 x);
f32 ceilf(f32 x);  // This gets annoying

extern "C" int16_t OTRGetRectDimensionFromLeftEdge(float v) {
    return ((int)floorf(OTRGetDimensionFromLeftEdge(v)));
}

extern "C" int16_t OTRGetRectDimensionFromRightEdge(float v) {
    return ((int)ceilf(OTRGetDimensionFromRightEdge(v)));
}

extern "C" int AudioPlayer_Buffered(void) {
    return AudioPlayerBuffered();
}

extern "C" int AudioPlayer_GetDesiredBuffered(void) {
    return AudioPlayerGetDesiredBuffered();
}

extern "C" void AudioPlayer_Play(const uint8_t* buf, uint32_t len) {
    AudioPlayerPlayFrame(buf, len);
}

extern "C" int Controller_ShouldRumble(size_t slot) {
    // don't rumble if we don't have rumble mappings
    if (Ship::Context::GetInstance()
            ->GetControlDeck()
            ->GetControllerByPort(static_cast<uint8_t>(slot))
            ->GetRumble()
            ->GetAllRumbleMappings()
            .empty()) {
        return 0;
    }

    // don't rumble if we don't have connected gamepads
    if (Ship::Context::GetInstance()
            ->GetControlDeck()
            ->GetConnectedPhysicalDeviceManager()
            ->GetConnectedSDLGamepadsForPort(slot)
            .empty()) {
        return 0;
    }

    // rumble
    return 1;
}

extern "C" GetItemEntry ItemTable_Retrieve(int16_t getItemID) {
    if (getItemID <= GI_NONE || getItemID > ARRAY_COUNT(sVanillaItemTable)) {
        return GET_ITEM_NONE;
    }

    GetItemEntry giEntry = sVanillaItemTable[getItemID - 1];
    giEntry.drawItemId = giEntry.itemId;
    giEntry.drawModIndex = MOD_NONE;
    return giEntry;
}

extern "C" GetItemEntry ItemTable_RetrieveEntry(s16 tableID, s16 getItemID) {
    if (tableID == MOD_NONE) {
        return ItemTable_Retrieve(getItemID);
    }

    GetItemEntry none = GET_ITEM_NONE;
    return none;
}

extern "C" void Gfx_RegisterBlendedTexture(const char* name, u8* mask, u8* replacement) {
    if (auto intP = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow())
                        ->GetInterpreterWeak()
                        .lock()) {
        intP->RegisterBlendedTexture(name, mask, replacement);
    } else {
        assert(false && "Lost reference to Fast::Interpreter");
    }
}

extern "C" void Gfx_UnregisterBlendedTexture(const char* name) {
    if (auto intP = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow())
                        ->GetInterpreterWeak()
                        .lock()) {
        intP->UnregisterBlendedTexture(name);
    } else {
        assert(false && "Lost reference to Fast::Interpreter");
    }
}

extern "C" void Gfx_TextureCacheDelete(const uint8_t* texAddr) {
    char* imgName = (char*)texAddr;

    if (texAddr == nullptr) {
        return;
    }

    if (ResourceMgr_OTRSigCheck(imgName)) {
        texAddr = (const uint8_t*)ResourceMgr_GetResourceDataByNameHandlingMQ(imgName);
    }

    if (auto intP = dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetInstance()->GetWindow())
                        ->GetInterpreterWeak()
                        .lock()) {
        intP->TextureCacheDelete(texAddr);
    } else {
        assert(false && "Lost reference to Fast::Interpreter");
    }
}

bool SoH_HandleConfigDrop(char* filePath) {
    if (SohUtils::IsStringEmpty(filePath)) {
        return false;
    }
    try {
        std::ifstream configStream(filePath);
        if (!configStream) {
            return false;
        }

        nlohmann::json configJson;
        configStream >> configJson;

        if (!configJson.contains("CVars")) {
            return false;
        }

        // Flatten everything under CVars into a single array
        auto cvars = configJson["CVars"].flatten();

        for (auto& [key, value] : cvars.items()) {
            // Replace slashes with dots in key, and remove leading dot
            std::string path = key;
            std::replace(path.begin(), path.end(), '/', '.');
            if (path[0] == '.') {
                path.erase(0, 1);
            }
            if (value.is_string()) {
                CVarSetString(path.c_str(), value.get<std::string>().c_str());
            } else if (value.is_number_integer()) {
                CVarSetInteger(path.c_str(), value.get<int>());
            } else if (value.is_number_float()) {
                CVarSetFloat(path.c_str(), value.get<float>());
            }
        }

        auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
        gui->SaveConsoleVariablesNextFrame();
        uint32_t finalHash = SohUtils::Hash(configJson.dump());
        gui->GetGameOverlay()->TextDrawNotification(30.0f, true, "Configuration Loaded. Hash: %d", finalHash);
        return true;
    } catch (std::exception& e) {
        SPDLOG_ERROR("Failed to load config file: {}", e.what());
        auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
        gui->GetGameOverlay()->TextDrawNotification(30.0f, true, "Failed to load config file");
        return false;
    } catch (...) {
        SPDLOG_ERROR("Failed to load config file");
        auto gui = Ship::Context::GetInstance()->GetWindow()->GetGui();
        gui->GetGameOverlay()->TextDrawNotification(30.0f, true, "Failed to load config file");
        return false;
    }
    return false;
}

extern "C" uint32_t Ship_GetInterpolationFPS() {
    return OTRGlobals::Instance->GetInterpolationFPS();
}

// Number of interpolated frames
extern "C" uint32_t Ship_GetInterpolationFrameCount() {
    return ceil((float)Ship_GetInterpolationFPS() / 20.0f);
}
