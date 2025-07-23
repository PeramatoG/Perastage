#include "loaderglb.h"
#include "consolepanel.h"
#include "../external/json.hpp"
#include <fstream>
#include <vector>
#include <algorithm>
#include <wx/wx.h>

using json = nlohmann::json;

static size_t ComponentSize(int compType)
{
    switch(compType) {
        case 5120: // BYTE
        case 5121: return 1; // UNSIGNED_BYTE
        case 5122: // SHORT
        case 5123: return 2; // UNSIGNED_SHORT
        case 5124: // INT
        case 5125: return 4; // UNSIGNED_INT
        case 5126: return 4; // FLOAT
        default: return 0;
    }
}

static size_t TypeCount(const std::string& type)
{
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT2") return 4;
    if (type == "MAT3") return 9;
    if (type == "MAT4") return 16;
    return 0;
}

bool LoadGLB(const std::string& path, Mesh& outMesh)
{
    outMesh.vertices.clear();
    outMesh.indices.clear();

    std::ifstream file(path, std::ios::binary);
    if(!file.is_open())
        return false;

    uint32_t magic=0, version=0, length=0;
    if(!file.read(reinterpret_cast<char*>(&magic),4)) return false;
    if(!file.read(reinterpret_cast<char*>(&version),4)) return false;
    if(!file.read(reinterpret_cast<char*>(&length),4)) return false;
    if(magic != 0x46546C67 || version != 2)
        return false;

    std::string jsonText;
    std::vector<unsigned char> binData;

    while(file.tellg() < static_cast<std::streampos>(length)) {
        uint32_t chunkLength=0, chunkType=0;
        if(!file.read(reinterpret_cast<char*>(&chunkLength),4)) break;
        if(!file.read(reinterpret_cast<char*>(&chunkType),4)) break;
        if(chunkType == 0x4E4F534A) { // JSON
            jsonText.resize(chunkLength);
            file.read(jsonText.data(), chunkLength);
        } else if(chunkType == 0x004E4942) { // BIN
            binData.resize(chunkLength);
            file.read(reinterpret_cast<char*>(binData.data()), chunkLength);
        } else {
            file.seekg(chunkLength, std::ios::cur);
        }
    }
    file.close();

    if(jsonText.empty() || binData.empty())
        return false;

    json doc = json::parse(jsonText, nullptr, false);
    if(doc.is_discarded())
        return false;

    if(!doc.contains("meshes"))
        return false;
    const auto& meshes = doc["meshes"];
    if(!meshes.is_array() || meshes.empty())
        return false;
    const auto& prims = meshes[0]["primitives"];
    if(!prims.is_array() || prims.empty())
        return false;
    const auto& prim = prims[0];
    if(!prim.contains("attributes") || !prim.contains("indices"))
        return false;
    if(!prim["attributes"].contains("POSITION"))
        return false;

    int posAccessor = prim["attributes"]["POSITION"].get<int>();
    int idxAccessor = prim["indices"].get<int>();

    auto getAccessorInfo = [&](int idx, size_t& offset, size_t& stride,
                               int& compType, std::string& type, size_t& count) -> bool
    {
        if(idx < 0 || !doc.contains("accessors")) return false;
        const auto& accs = doc["accessors"];
        if(idx >= accs.size()) return false;
        const auto& acc = accs[idx];
        compType = acc["componentType"].get<int>();
        type = acc["type"].get<std::string>();
        count = acc["count"].get<size_t>();
        size_t accOffset = acc.value("byteOffset", 0);
        if(!acc.contains("bufferView")) return false;
        size_t viewIdx = acc["bufferView"].get<size_t>();
        if(!doc.contains("bufferViews")) return false;
        const auto& views = doc["bufferViews"];
        if(viewIdx >= views.size()) return false;
        const auto& view = views[viewIdx];
        offset = view.value("byteOffset", 0) + accOffset;
        stride = view.value("byteStride", 0);
        if(stride == 0)
            stride = ComponentSize(compType) * TypeCount(type);
        size_t bufferIdx = view.value("buffer", 0);
        if(bufferIdx != 0) return false; // only single embedded buffer supported
        return true;
    };

    size_t posOff, posStride, posCount; int posCT; std::string posType;
    if(!getAccessorInfo(posAccessor, posOff, posStride, posCT, posType, posCount))
        return false;
    if(posCT != 5126 || posType != "VEC3")
        return false;

    size_t idxOff, idxStride, idxCount; int idxCT; std::string idxType;
    if(!getAccessorInfo(idxAccessor, idxOff, idxStride, idxCT, idxType, idxCount))
        return false;
    if(idxType != "SCALAR")
        return false;

    if(posOff + posStride * posCount > binData.size())
        return false;
    if(idxOff + idxStride * idxCount > binData.size())
        return false;

    outMesh.vertices.resize(posCount * 3);
    for(size_t i=0;i<posCount;i++) {
        const float* src = reinterpret_cast<const float*>(binData.data() + posOff + posStride * i);
        // glTF uses Y-up with Z forward. Convert to our Z-up system where
        // positive Y goes into the screen. This keeps a right-handed coordinate
        // system so composed models match 3DS orientation.
        outMesh.vertices[i*3]     = src[0];          // X stays the same
        outMesh.vertices[i*3 + 1] = -src[2];         // Y <- -Z
        outMesh.vertices[i*3 + 2] =  src[1];         // Z <- Y
    }

    outMesh.indices.resize(idxCount);
    for(size_t i=0;i<idxCount;i++) {
        if(idxCT == 5123) {
            uint16_t v = *reinterpret_cast<const uint16_t*>(binData.data() + idxOff + idxStride * i);
            outMesh.indices[i] = v;
        } else if(idxCT == 5125) {
            uint32_t v = *reinterpret_cast<const uint32_t*>(binData.data() + idxOff + idxStride * i);
            outMesh.indices[i] = static_cast<unsigned short>(v);
        } else if(idxCT == 5121) {
            uint8_t v = *(binData.data() + idxOff + idxStride * i);
            outMesh.indices[i] = v;
        } else {
            return false;
        }
    }

    bool ok = !outMesh.vertices.empty() && !outMesh.indices.empty();
    if(ConsolePanel::Instance()) {
        wxString msg;
        if(ok) {
            msg = wxString::Format("GLB: %s -> v=%zu i=%zu",
                                   wxString::FromUTF8(path),
                                   outMesh.vertices.size()/3,
                                   outMesh.indices.size()/3);
        } else {
            msg = wxString::Format("GLB: parsed but empty %s", wxString::FromUTF8(path));
        }
        ConsolePanel::Instance()->AppendMessage(msg);
    }
    return ok;
}

