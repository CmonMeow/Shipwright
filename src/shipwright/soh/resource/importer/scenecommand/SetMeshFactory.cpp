#include <libultraship/log/PathEngineLog.hpp>

#include "soh/resource/importer/scenecommand/SetMeshFactory.h"
#include "soh/resource/type/scenecommand/SetMesh.h"

#include <tinyxml2.h>

namespace SOH {
namespace {

void AddDisplayList(SetMesh& setMesh, std::string opaquePath, std::string translucentPath) {
    PolygonDlist dlist = {};

    if (!opaquePath.empty()) {
        setMesh.opaPaths.push_back("__OTR__" + opaquePath);
        dlist.opa = reinterpret_cast<Gfx*>(const_cast<char*>(setMesh.opaPaths.back().c_str()));
    }
    if (!translucentPath.empty()) {
        setMesh.xluPaths.push_back("__OTR__" + translucentPath);
        dlist.xlu = reinterpret_cast<Gfx*>(const_cast<char*>(setMesh.xluPaths.back().c_str()));
    }

    setMesh.dlists.push_back(dlist);
}

bool InitializeTest01Mesh(SetMesh& setMesh, int32_t type, int32_t count) {
    if (type != 0 || count < 0 || count > UINT8_MAX) {
        PathEngineLog("Rejected non-test01 room mesh: type {}, polygon count {}", type, count);
        return false;
    }

    setMesh.meshHeader.base.type = 0;
    setMesh.meshHeader.polygon0.num = static_cast<uint8_t>(count);
    setMesh.opaPaths.reserve(count);
    setMesh.xluPaths.reserve(count);
    setMesh.dlists.reserve(count);
    return true;
}

void FinishTest01Mesh(SetMesh& setMesh) {
    setMesh.meshHeader.polygon0.start = setMesh.dlists.data();
    setMesh.meshHeader.polygon0.end = setMesh.dlists.data() + setMesh.dlists.size();
}

} // namespace

std::shared_ptr<Ship::IResource> SetMeshFactory::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                                              std::shared_ptr<Ship::BinaryReader> reader) {
    auto setMesh = std::make_shared<SetMesh>(initData);
    ReadCommandId(setMesh, reader);
    setMesh->data = reader->ReadInt8();

    const int32_t type = reader->ReadInt8();
    const int32_t count = reader->ReadUByte();
    if (!InitializeTest01Mesh(*setMesh, type, count)) {
        return nullptr;
    }

    for (int32_t i = 0; i < count; i++) {
        reader->ReadInt8(); // Exported polygon type; unused by a type-0 mesh.
        AddDisplayList(*setMesh, reader->ReadString(), reader->ReadString());
    }

    FinishTest01Mesh(*setMesh);
    return setMesh;
}

std::shared_ptr<Ship::IResource> SetMeshFactoryXML::ReadResource(std::shared_ptr<Ship::ResourceInitData> initData,
                                                                 tinyxml2::XMLElement* reader) {
    auto setMesh = std::make_shared<SetMesh>(initData);
    setMesh->cmdId = SceneCommandID::SetMesh;
    setMesh->data = static_cast<uint8_t>(reader->IntAttribute("Data"));

    const int32_t type = reader->IntAttribute("MeshHeaderType", -1);
    const int32_t count = reader->IntAttribute("PolyNum", -1);
    if (!InitializeTest01Mesh(*setMesh, type, count)) {
        return nullptr;
    }

    for (tinyxml2::XMLElement* child = reader->FirstChildElement("Polygon"); child != nullptr;
         child = child->NextSiblingElement("Polygon")) {
        const char* opaquePath = child->Attribute("MeshOpa");
        const char* translucentPath = child->Attribute("MeshXlu");
        AddDisplayList(*setMesh, opaquePath != nullptr ? opaquePath : "",
                       translucentPath != nullptr ? translucentPath : "");
    }

    if (setMesh->dlists.size() != static_cast<size_t>(count)) {
        PathEngineLog("Rejected test01 room mesh with {} polygons but {} entries", count, setMesh->dlists.size());
        return nullptr;
    }

    FinishTest01Mesh(*setMesh);
    return setMesh;
}

} // namespace SOH
