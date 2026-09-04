#include "rendering/resource/factory/DisplayListFactory.h"
#include "rendering/resource/type/DisplayList.h"
#include "runtime/libultra/gbi.h"
#include "rendering/ResourceGbi.h"
#include <string>
#include <unordered_map>

namespace Engine::Rendering {
std::unordered_map<std::string, uint32_t> renderModes = {
    { "G_RM_ZB_OPA_SURF", G_RM_ZB_OPA_SURF },
    { "G_RM_AA_ZB_OPA_SURF", G_RM_AA_ZB_OPA_SURF },
    { "G_RM_AA_ZB_OPA_DECAL", G_RM_AA_ZB_OPA_DECAL },
    { "G_RM_AA_ZB_OPA_INTER", G_RM_AA_ZB_OPA_INTER },
    { "G_RM_AA_ZB_TEX_EDGE", G_RM_AA_ZB_TEX_EDGE },
    { "G_RM_AA_ZB_XLU_SURF", G_RM_AA_ZB_XLU_SURF },
    { "G_RM_AA_ZB_XLU_DECAL", G_RM_AA_ZB_XLU_DECAL },
    { "G_RM_AA_ZB_XLU_INTER", G_RM_AA_ZB_XLU_INTER },
    { "G_RM_FOG_SHADE_A", G_RM_FOG_SHADE_A },
    { "G_RM_FOG_PRIM_A", G_RM_FOG_PRIM_A },
    { "G_RM_PASS", G_RM_PASS },
    { "G_RM_ADD", G_RM_ADD },
    { "G_RM_NOOP", G_RM_NOOP },
    { "G_RM_ZB_OPA_SURF", G_RM_ZB_OPA_SURF },
    { "G_RM_ZB_OPA_DECAL", G_RM_ZB_OPA_DECAL },
    { "G_RM_ZB_XLU_SURF", G_RM_ZB_XLU_SURF },
    { "G_RM_ZB_XLU_DECAL", G_RM_ZB_XLU_DECAL },
    { "G_RM_OPA_SURF", G_RM_OPA_SURF },
    { "G_RM_ZB_CLD_SURF", G_RM_ZB_CLD_SURF },
    { "G_RM_ZB_OPA_SURF2", G_RM_ZB_OPA_SURF2 },
    { "G_RM_AA_ZB_OPA_SURF2", G_RM_AA_ZB_OPA_SURF2 },
    { "G_RM_AA_ZB_OPA_DECAL2", G_RM_AA_ZB_OPA_DECAL2 },
    { "G_RM_AA_ZB_OPA_INTER2", G_RM_AA_ZB_OPA_INTER2 },
    { "G_RM_AA_ZB_TEX_EDGE2", G_RM_AA_ZB_TEX_EDGE2 },
    { "G_RM_AA_ZB_XLU_SURF2", G_RM_AA_ZB_XLU_SURF2 },
    { "G_RM_AA_ZB_XLU_DECAL2", G_RM_AA_ZB_XLU_DECAL2 },
    { "G_RM_AA_ZB_XLU_INTER2", G_RM_AA_ZB_XLU_INTER2 },
    { "G_RM_ADD2", G_RM_ADD2 },
    { "G_RM_ZB_OPA_SURF2", G_RM_ZB_OPA_SURF2 },
    { "G_RM_ZB_OPA_DECAL2", G_RM_ZB_OPA_DECAL2 },
    { "G_RM_ZB_XLU_SURF2", G_RM_ZB_XLU_SURF2 },
    { "G_RM_ZB_XLU_DECAL2", G_RM_ZB_XLU_DECAL2 },
    { "G_RM_ZB_CLD_SURF2", G_RM_ZB_CLD_SURF2 },
};

uint32_t ResourceFactoryDisplayList::GetCombineLERPValue(const char* valStr) {
    static const char* strings[] = {
        "G_CCMUX_COMBINED",
        "G_CCMUX_TEXEL0",
        "G_CCMUX_TEXEL1",
        "G_CCMUX_PRIMITIVE",
        "G_CCMUX_SHADE",
        "G_CCMUX_ENVIRONMENT",
        "G_CCMUX_1",
        "G_CCMUX_NOISE",
        "G_CCMUX_0",
        "G_CCMUX_CENTER",
        "G_CCMUX_K4",
        "G_CCMUX_SCALE",
        "G_CCMUX_COMBINED_ALPHA",
        "G_CCMUX_TEXEL0_ALPHA",
        "G_CCMUX_TEXEL1_ALPHA",
        "G_CCMUX_PRIMITIVE_ALPHA",
        "G_CCMUX_SHADE_ALPHA",
        "G_CCMUX_ENV_ALPHA",
        "G_CCMUX_LOD_FRACTION",
        "G_CCMUX_PRIM_LOD_FRAC",
        "G_CCMUX_K5",
        "G_ACMUX_COMBINED",
        "G_ACMUX_TEXEL0",
        "G_ACMUX_TEXEL1",
        "G_ACMUX_PRIMITIVE",
        "G_ACMUX_SHADE",
        "G_ACMUX_ENVIRONMENT",
        "G_ACMUX_1",
        "G_ACMUX_0",
        "G_ACMUX_LOD_FRACTION",
        "G_ACMUX_PRIM_LOD_FRAC",
    };
    static uint32_t values[] = {
        G_CCMUX_COMBINED,
        G_CCMUX_TEXEL0,
        G_CCMUX_TEXEL1,
        G_CCMUX_PRIMITIVE,
        G_CCMUX_SHADE,
        G_CCMUX_ENVIRONMENT,
        G_CCMUX_1,
        G_CCMUX_NOISE,
        G_CCMUX_0,
        G_CCMUX_CENTER,
        G_CCMUX_K4,
        G_CCMUX_SCALE,
        G_CCMUX_COMBINED_ALPHA,
        G_CCMUX_TEXEL0_ALPHA,
        G_CCMUX_TEXEL1_ALPHA,
        G_CCMUX_PRIMITIVE_ALPHA,
        G_CCMUX_SHADE_ALPHA,
        G_CCMUX_ENV_ALPHA,
        G_CCMUX_LOD_FRACTION,
        G_CCMUX_PRIM_LOD_FRAC,
        G_CCMUX_K5,
        G_ACMUX_COMBINED,
        G_ACMUX_TEXEL0,
        G_ACMUX_TEXEL1,
        G_ACMUX_PRIMITIVE,
        G_ACMUX_SHADE,
        G_ACMUX_ENVIRONMENT,
        G_ACMUX_1,
        G_ACMUX_0,
        G_ACMUX_LOD_FRACTION,
        G_ACMUX_PRIM_LOD_FRAC,
    };

    for (size_t i = 0; i < std::size(values); i++) {
        if (strncmp(valStr, strings[i], strlen(strings[i])) == 0) {
            return values[i];
        }
    }

    return G_CCMUX_1;
}

int8_t GetEndOpcodeByUCode(UcodeHandlers ucode) {
    switch (ucode) {
        case ucode_f3d:
        case ucode_f3db:
        case ucode_f3dex:
        case ucode_f3dexb:
            return F3DEX_G_ENDDL;
        case ucode_f3dex2:
        case ucode_s2dex: {
            return F3DEX2_G_ENDDL;
        }
        case ucode_max:
            break;
    }
    return -1;
}

std::shared_ptr<Engine::IResource>
ResourceFactoryBinaryDisplayListV0::ReadResource(std::shared_ptr<Engine::File> file,
                                                 std::shared_ptr<Engine::ResourceInitData> initData) {
    if (!FileHasValidFormatAndReader(file, initData)) {
        return nullptr;
    }

    auto displayList = std::make_shared<DisplayList>(initData);
    auto reader = std::get<std::shared_ptr<Engine::BinaryReader>>(file->Reader);
    auto ucode = (UcodeHandlers)reader->ReadInt8();

    displayList->UCode = ucode;

    while (reader->GetBaseAddress() % 8 != 0) {
        reader->ReadInt8();
    }

    size_t idx = 0;
    while (true) {
        Gfx command;
        command.words.w0 = reader->ReadUInt32();
        command.words.w1 = reader->ReadUInt32();

        int8_t opcode = (int8_t)(command.words.w0 >> 24);
        bool isExpanded = opcode == G_SETTIMG_RESOURCE_HASH || opcode == G_DL_RESOURCE_HASH ||
                          opcode == G_VTX_RESOURCE_HASH || opcode == G_BRANCH_Z_RESOURCE || opcode == G_MARKER ||
                          opcode == G_MTX_RESOURCE || opcode == G_MOVEMEM_RESOURCE;

        // These are 128-bit commands, so read an extra 64 bits...
        if (isExpanded) {
#ifdef USE_GBI_TRACE
            command.words.trace.file = initData->Path.c_str();
            command.words.trace.idx = idx++;
            command.words.trace.valid = true;
#endif
            displayList->Instructions.push_back(command);
            command.words.w0 = reader->ReadUInt32();
            command.words.w1 = reader->ReadUInt32();
        }

#ifdef USE_GBI_TRACE
        command.words.trace.file = initData->Path.c_str();
        command.words.trace.idx = idx++;
        command.words.trace.valid = true;
#endif

        displayList->Instructions.push_back(command);

        if (opcode == GetEndOpcodeByUCode(ucode)) {
            break;
        }
    }

    return displayList;
}

} // namespace Engine::Rendering
