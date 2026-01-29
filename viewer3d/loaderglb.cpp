/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "loaderglb.h"
#include "consolepanel.h"
#include "../external/json.hpp"
#include "../models/matrixutils.h"
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>
#include <wx/wx.h>
#include <optional>

constexpr bool kLogGlbMessages = false;

using json = nlohmann::json;

// glTF specification defines distances in meters whereas MVR expects
// millimeters. Apply a constant scale so that loaded meshes match the
// coordinate system used for 3DS files and the rest of the viewer.
static constexpr float GLB_TO_MVR_SCALE = 1000.0f;

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

struct GLBFile
{
    json doc;
    std::vector<unsigned char> binData;
};

static std::optional<GLBFile> ParseGLBFile(const std::string& path, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        error = "No se puede abrir el archivo";
        return std::nullopt;
    }

    constexpr size_t kMaxChunkSize = 100u * 1024u * 1024u;

    uint32_t magic = 0, version = 0, length = 0;
    if (!file.read(reinterpret_cast<char*>(&magic), 4) ||
        !file.read(reinterpret_cast<char*>(&version), 4) ||
        !file.read(reinterpret_cast<char*>(&length), 4)) {
        error = "Cabecera GLB incompleta";
        return std::nullopt;
    }
    if (magic != 0x46546C67 || version != 2) {
        error = "Formato GLB no reconocido";
        return std::nullopt;
    }

    std::string jsonText;
    std::vector<unsigned char> binData;

    while (file.tellg() < static_cast<std::streampos>(length)) {
        uint32_t chunkLength = 0, chunkType = 0;
        if (!file.read(reinterpret_cast<char*>(&chunkLength), 4) ||
            !file.read(reinterpret_cast<char*>(&chunkType), 4)) {
            error = "Cabecera de chunk GLB incompleta";
            return std::nullopt;
        }

        const auto chunkDataPos = file.tellg();
        if (chunkDataPos < 0) {
            error = "Chunk GLB con tamaño inválido";
            return std::nullopt;
        }
        const auto chunkDataOffset = static_cast<size_t>(chunkDataPos);
        if (chunkDataOffset > length) {
            error = "Chunk GLB con tamaño inválido";
            return std::nullopt;
        }
        const auto remainingBytes = static_cast<size_t>(length) - chunkDataOffset;
        if (chunkLength > remainingBytes) {
            error = "Chunk GLB con tamaño inválido";
            return std::nullopt;
        }

        if (chunkType == 0x4E4F534A) { // JSON
            if (chunkLength > kMaxChunkSize) {
                error = "Chunk GLB con tamaño inválido";
                return std::nullopt;
            }
            jsonText.resize(chunkLength);
            if (!file.read(jsonText.data(), chunkLength) ||
                static_cast<size_t>(file.gcount()) != chunkLength) {
                error = "Chunk GLB con tamaño inválido";
                return std::nullopt;
            }
        } else if (chunkType == 0x004E4942) { // BIN
            if (chunkLength > kMaxChunkSize) {
                error = "Chunk GLB con tamaño inválido";
                return std::nullopt;
            }
            binData.resize(chunkLength);
            if (!file.read(reinterpret_cast<char*>(binData.data()), chunkLength) ||
                static_cast<size_t>(file.gcount()) != chunkLength) {
                error = "Chunk GLB con tamaño inválido";
                return std::nullopt;
            }
        } else {
            if (!file.seekg(chunkLength, std::ios::cur)) {
                error = "Chunk GLB con tamaño inválido";
                return std::nullopt;
            }
        }
    }
    file.close();

    if (jsonText.empty() || binData.empty()) {
        error = "Chunks JSON/BIN ausentes";
        return std::nullopt;
    }

    json doc = json::parse(jsonText, nullptr, false);
    if (doc.is_discarded()) {
        error = "JSON de escena inválido";
        return std::nullopt;
    }

    GLBFile result;
    result.doc = std::move(doc);
    result.binData = std::move(binData);
    return result;
}

bool LoadGLB(const std::string& path, Mesh& outMesh)
{
    outMesh.vertices.clear();
    outMesh.indices.clear();

    std::string parseError;
    auto glbFile = ParseGLBFile(path, parseError);
    if (!glbFile) {
        if (kLogGlbMessages && ConsolePanel::Instance()) {
            ConsolePanel::Instance()->AppendMessage(
                wxString::Format("GLB: %s (se omite carga - %s)",
                                 wxString::FromUTF8(path),
                                 wxString::FromUTF8(parseError)));
        }
        return false;
    }

    const json& doc = glbFile->doc;
    const auto& binData = glbFile->binData;

    if(!doc.contains("meshes"))
        return false;

    auto readInt = [](const json& j, int& out) {
        if(!j.is_number_integer())
            return false;
        out = j.get<int>();
        return true;
    };

    auto readSize = [](const json& j, size_t& out) {
        if(!j.is_number_integer())
            return false;
        out = j.get<size_t>();
        return true;
    };

    auto readString = [](const json& j, std::string& out) {
        if(!j.is_string())
            return false;
        out = j.get<std::string>();
        return true;
    };

    auto readFloat = [](const json& j, float& out) {
        if(!(j.is_number_float() || j.is_number_integer()))
            return false;
        out = j.get<float>();
        return true;
    };

    auto getAccessorInfo = [&](int idx, size_t& offset, size_t& stride,
                               int& compType, std::string& type, size_t& count) -> bool
    {
        if(idx < 0 || !doc.contains("accessors")) return false;
        const auto& accs = doc["accessors"];
        if(!accs.is_array() || idx >= accs.size()) return false;
        const auto& acc = accs[idx];
        if(!acc.contains("componentType") || !acc["componentType"].is_number_integer())
            return false;
        if(!acc.contains("type") || !acc["type"].is_string())
            return false;
        if(!acc.contains("count") || !acc["count"].is_number_integer())
            return false;
        compType = acc["componentType"].get<int>();
        type = acc["type"].get<std::string>();
        count = acc["count"].get<size_t>();
        size_t accOffset = acc.value("byteOffset", 0);
        if(!acc.contains("bufferView") || !acc["bufferView"].is_number_integer()) return false;
        size_t viewIdx = acc["bufferView"].get<size_t>();
        if(!doc.contains("bufferViews")) return false;
        const auto& views = doc["bufferViews"];
        if(!views.is_array() || viewIdx >= views.size()) return false;
        const auto& view = views[viewIdx];
        size_t viewOffset = 0;
        if(view.contains("byteOffset") && !readSize(view["byteOffset"], viewOffset))
            return false;
        offset = viewOffset + accOffset;
        stride = 0;
        if(view.contains("byteStride")) {
            if(!readSize(view["byteStride"], stride))
                return false;
        }
        if(stride == 0)
            stride = ComponentSize(compType) * TypeCount(type);
        size_t bufferIdx = 0;
        if(view.contains("buffer") && !readSize(view["buffer"], bufferIdx))
            return false;
        if(bufferIdx != 0) return false; // only single embedded buffer supported
        return true;
    };

    auto transformPoint = [](const Matrix& m, const std::array<float,3>& p) {
        return std::array<float,3>{
            (m.u[0]*p[0] + m.v[0]*p[1] + m.w[0]*p[2] + m.o[0]) * GLB_TO_MVR_SCALE,
            (m.u[1]*p[0] + m.v[1]*p[1] + m.w[1]*p[2] + m.o[1]) * GLB_TO_MVR_SCALE,
            (m.u[2]*p[0] + m.v[2]*p[1] + m.w[2]*p[2] + m.o[2]) * GLB_TO_MVR_SCALE
        };
    };

    auto nodeMatrix = [&readFloat](const json& node) {
        Matrix m = MatrixUtils::Identity();
        if(!node.is_object())
            return m;
        if(node.contains("matrix")) {
            const auto& arr = node["matrix"];
            if(arr.is_array() && arr.size() == 16) {
                std::array<float, 16> vals{};
                for(size_t i = 0; i < 16; ++i) {
                    if(!readFloat(arr[i], vals[i]))
                        return m;
                }
                m.u = {vals[0], vals[1], vals[2]};
                m.v = {vals[4], vals[5], vals[6]};
                m.w = {vals[8], vals[9], vals[10]};
                m.o = {vals[12], vals[13], vals[14]};
            }
            return m;
        }

        std::array<float,3> t{0.f,0.f,0.f};
        std::array<float,3> s{1.f,1.f,1.f};
        std::array<float,4> r{0.f,0.f,0.f,1.f};

        if(node.contains("translation")) {
            const auto& tr = node["translation"];
            if(tr.is_array() && tr.size() >= 3) {
                float x = 0, y = 0, z = 0;
                if(readFloat(tr[0], x) && readFloat(tr[1], y) && readFloat(tr[2], z))
                    t = {x, y, z};
            }
        }
        if(node.contains("scale")) {
            const auto& sc = node["scale"];
            if(sc.is_array() && sc.size() >= 3) {
                float x = 1, y = 1, z = 1;
                if(readFloat(sc[0], x) && readFloat(sc[1], y) && readFloat(sc[2], z))
                    s = {x, y, z};
            }
        }
        if(node.contains("rotation")) {
            const auto& rot = node["rotation"];
            if(rot.is_array() && rot.size() >= 4) {
                float x = 0, y = 0, z = 0, w = 1;
                if(readFloat(rot[0], x) && readFloat(rot[1], y) && readFloat(rot[2], z) && readFloat(rot[3], w))
                    r = {x, y, z, w};
            }
        }

        float x=r[0], y=r[1], z=r[2], w=r[3];
        float xx=x*x, yy=y*y, zz=z*z;
        float xy=x*y, xz=x*z, yz=y*z;
        float wx=w*x, wy=w*y, wz=w*z;

        std::array<float,3> col0{
            (1.f - 2.f*(yy + zz)) * s[0],
            (2.f*(xy + wz)) * s[0],
            (2.f*(xz - wy)) * s[0]
        };
        std::array<float,3> col1{
            (2.f*(xy - wz)) * s[1],
            (1.f - 2.f*(xx + zz)) * s[1],
            (2.f*(yz + wx)) * s[1]
        };
        std::array<float,3> col2{
            (2.f*(xz + wy)) * s[2],
            (2.f*(yz - wx)) * s[2],
            (1.f - 2.f*(xx + yy)) * s[2]
        };

        m.u = col0;
        m.v = col1;
        m.w = col2;
        m.o = t;
        return m;
    };

    auto readPrimitive = [&](const json& prim, const Matrix& transform) -> bool {
        if(!prim.contains("attributes") || !prim.contains("indices"))
            return false;
        auto attributes = prim.find("attributes");
        if(attributes == prim.end() || !attributes->is_object())
            return false;
        auto posIt = attributes->find("POSITION");
        if(posIt == attributes->end())
            return false;

        int posAccessor = 0;
        if(!readInt(*posIt, posAccessor))
            return false;
        int idxAccessor = 0;
        auto idxIt = prim.find("indices");
        if(idxIt == prim.end() || !readInt(*idxIt, idxAccessor))
            return false;

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

        size_t base = outMesh.vertices.size() / 3;
        outMesh.vertices.resize(base*3 + posCount*3);
        for(size_t i=0;i<posCount;i++) {
            const float* src = reinterpret_cast<const float*>(binData.data()+posOff+posStride*i);
            std::array<float,3> p{src[0], src[1], src[2]};
            p = transformPoint(transform, p);
            outMesh.vertices[(base+i)*3 + 0] = p[0];
            outMesh.vertices[(base+i)*3 + 1] = p[1];
            outMesh.vertices[(base+i)*3 + 2] = p[2];
        }

        outMesh.indices.resize(outMesh.indices.size() + idxCount);
        for(size_t i=0;i<idxCount;i++) {
            unsigned int v = 0;
            if(idxCT == 5123) {
                v = *reinterpret_cast<const uint16_t*>(binData.data()+idxOff+idxStride*i);
            } else if(idxCT == 5125) {
                v = *reinterpret_cast<const uint32_t*>(binData.data()+idxOff+idxStride*i);
            } else if(idxCT == 5121) {
                v = *(binData.data()+idxOff+idxStride*i);
            } else {
                return false;
            }
            outMesh.indices[outMesh.indices.size()-idxCount+i] = static_cast<unsigned short>(v + base);
        }
        return true;
    };

    // glTF files use a Y-up coordinate system whereas GDTF specifies Z-up with
    // Y pointing into the screen. Apply a constant basis change so that models
    // match the GDTF orientation.
    Matrix axisConv;
    axisConv.u = {1.f, 0.f, 0.f};   // X -> X
    axisConv.v = {0.f, 0.f, 1.f};   // Y -> Z
    axisConv.w = {0.f, -1.f, 0.f};  // Z -> -Y
    axisConv.o = {0.f, 0.f, 0.f};

    std::function<bool(int,const Matrix&)> parseNode;
    parseNode = [&](int nodeIdx, const Matrix& parent) -> bool {
        if(!doc.contains("nodes")) return false;
        const auto& nodes = doc["nodes"];
        if(!nodes.is_array() || nodeIdx < 0 || nodeIdx >= nodes.size()) return false;
        const auto& node = nodes[nodeIdx];
        if(!node.is_object()) return false;
        Matrix local = nodeMatrix(node);
        Matrix transform = MatrixUtils::Multiply(parent, local);
        bool any = false;
        if(node.contains("mesh")) {
            int meshIdx = 0;
            if(readInt(node["mesh"], meshIdx) && meshIdx >=0 && doc.contains("meshes")) {
                const auto& meshes = doc["meshes"];
                if(meshes.is_array() && meshIdx < meshes.size()) {
                    const auto& mesh = meshes[meshIdx];
                    if(mesh.contains("primitives")) {
                        const auto& prims = mesh["primitives"];
                        if(prims.is_array()) {
                            for(const auto& p : prims) {
                                if(readPrimitive(p, transform))
                                    any = true;
                            }
                        }
                    }
                }
            }
        }
        if(node.contains("children")) {
            const auto& children = node["children"];
            if(children.is_array()) {
                for(const auto& c : children) {
                    if(c.is_number_integer())
                        if(parseNode(c.get<int>(), transform))
                            any = true;
                }
            }
        }
        return any;
    };

    bool ok = false;
    if(doc.contains("scenes")) {
        const auto& scenes = doc["scenes"];
        if(scenes.is_array() && !scenes.empty()) {
            const auto& scene = scenes[0];
            if(scene.contains("nodes") && scene["nodes"].is_array()) {
                for(const auto& n : scene["nodes"]) {
                    if(n.is_number_integer()) {
                        if(parseNode(n.get<int>(), axisConv))
                            ok = true;
                    }
                }
            }
        }
    }
    if(!ok && doc.contains("nodes")) {
        const auto& nodes = doc["nodes"];
        if(nodes.is_array()) {
            for(size_t i=0;i<nodes.size(); ++i)
                if(parseNode(static_cast<int>(i), axisConv))
                    ok = true;
        }
    }

    if (ok)
        ComputeNormals(outMesh);

    if(kLogGlbMessages && ConsolePanel::Instance()) {
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
