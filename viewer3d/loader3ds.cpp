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
#include "loader3ds.h"
#include <fstream>
#include <cstdint>
#include <array>
#include <cctype>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include "consolepanel.h"
#include <wx/wx.h>
#include <wx/image.h>

namespace fs = std::filesystem;

constexpr bool kLog3dsMessages = false;

struct Chunk {
    uint16_t id;
    uint32_t length;
};

struct FaceColorAccum {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double weight = 0.0;
};

static bool EnsureImageHandlersInitialized()
{
    static bool imageHandlersInitialized = false;
    if (!imageHandlersInitialized) {
        wxInitAllImageHandlers();
        imageHandlersInitialized = true;
    }
    return true;
}

static std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

static bool FilenameEqualsInsensitive(const fs::path& a, const fs::path& b)
{
    return ToLower(a.filename().string()) == ToLower(b.filename().string());
}

static bool FindTextureByFilename(const fs::path& root, const fs::path& targetName, fs::path& outPath)
{
    if (root.empty() || !fs::exists(root) || !fs::is_directory(root))
        return false;

    // Avoid blocking scene build when an archive is extracted into a large
    // temp folder tree. Keep the search bounded and local to model content.
    constexpr size_t kMaxScannedEntries = 5000;
    constexpr int kMaxSearchDepth = 4;

    std::error_code ec;
    size_t scannedEntries = 0;
    for (fs::recursive_directory_iterator it(root,
                                             fs::directory_options::skip_permission_denied,
                                             ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (it.depth() > kMaxSearchDepth) {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_symlink(ec))
            continue;
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec))
            continue;
        if (ec) {
            ec.clear();
            continue;
        }
        if (FilenameEqualsInsensitive(it->path(), targetName)) {
            outPath = it->path();
            return true;
        }
        ++scannedEntries;
        if (scannedEntries >= kMaxScannedEntries)
            break;
    }
    return false;
}

static bool ResolveTexturePath(const fs::path& modelDir, const std::string& filename, fs::path& outPath)
{
    if (filename.empty())
        return false;

    fs::path texturePath = fs::u8path(filename);
    if (texturePath.is_absolute() && fs::exists(texturePath)) {
        outPath = texturePath;
        return true;
    }

    const fs::path directCandidate = modelDir / texturePath;
    if (fs::exists(directCandidate)) {
        outPath = directCandidate;
        return true;
    }

    fs::path currentDir = modelDir;
    while (!currentDir.empty()) {
        const fs::path ancestorCandidate = currentDir / texturePath;
        if (fs::exists(ancestorCandidate)) {
            outPath = ancestorCandidate;
            return true;
        }
        if (currentDir == currentDir.parent_path())
            break;
        currentDir = currentDir.parent_path();
    }

    if (FindTextureByFilename(modelDir, texturePath, outPath))
        return true;

    return false;
}

static bool LoadTextureImage(const fs::path& modelDir, const std::string& filename, Mesh& outMesh)
{
    EnsureImageHandlersInitialized();

    fs::path texturePath;
    if (!ResolveTexturePath(modelDir, filename, texturePath))
        return false;

    wxImage textureImage;
    if (!textureImage.LoadFile(wxString::FromUTF8(texturePath.string())))
        return false;
    if (!textureImage.IsOk())
        return false;

    const int width = static_cast<int>(textureImage.GetWidth());
    const int height = static_cast<int>(textureImage.GetHeight());
    const unsigned char* rgb = textureImage.GetData();
    if (!rgb || width <= 0 || height <= 0)
        return false;

    const unsigned char* alpha = textureImage.HasAlpha() ? textureImage.GetAlpha() : nullptr;
    outMesh.textureRgba.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    for (int i = 0; i < width * height; ++i) {
        outMesh.textureRgba[static_cast<size_t>(i) * 4u + 0u] = rgb[i * 3 + 0];
        outMesh.textureRgba[static_cast<size_t>(i) * 4u + 1u] = rgb[i * 3 + 1];
        outMesh.textureRgba[static_cast<size_t>(i) * 4u + 2u] = rgb[i * 3 + 2];
        outMesh.textureRgba[static_cast<size_t>(i) * 4u + 3u] = alpha ? alpha[i] : 255u;
    }
    outMesh.textureWidth = width;
    outMesh.textureHeight = height;
    return true;
}

static bool readChunk(std::ifstream& file, Chunk& c)
{
    if(!file.read(reinterpret_cast<char*>(&c.id), 2)) return false;
    if(!file.read(reinterpret_cast<char*>(&c.length), 4)) return false;
    return true;
}

// Parses a single TRIANGULAR MESH (0x4100) chunk.
// vertexBase is the current number of vertices already stored in the mesh
// so we can offset face indices when concatenating multiple objects.
static std::string readCString(std::ifstream& file, long endPos)
{
    std::string out;
    char ch = 0;
    while (file.tellg() < endPos && file.read(&ch, 1)) {
        if (ch == '\0')
            break;
        out.push_back(ch);
    }
    return out;
}

static std::array<float, 3> readColorChunk(std::ifstream& file, long endPos, bool& ok)
{
    std::array<float, 3> c{1.0f, 1.0f, 1.0f};
    ok = false;
    while (file.tellg() < endPos) {
        Chunk ch;
        if (!readChunk(file, ch))
            return c;
        const long dataStart = file.tellg();
        const long next = dataStart + ch.length - 6;
        if (ch.id == 0x0010 || ch.id == 0x0013) { // float color
            file.read(reinterpret_cast<char*>(&c[0]), sizeof(float));
            file.read(reinterpret_cast<char*>(&c[1]), sizeof(float));
            file.read(reinterpret_cast<char*>(&c[2]), sizeof(float));
            ok = true;
            file.seekg(next);
            return c;
        }
        if (ch.id == 0x0011 || ch.id == 0x0012) { // byte color
            unsigned char rgb[3] = {255, 255, 255};
            file.read(reinterpret_cast<char*>(rgb), 3);
            c = {rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f};
            ok = true;
            file.seekg(next);
            return c;
        }
        file.seekg(next);
    }
    return c;
}

static void parseMesh(std::ifstream& file, long endPos, Mesh& mesh, size_t vertexBase,
                      const std::unordered_map<std::string, std::array<float, 3>>& materials,
                      FaceColorAccum& colorAccum)
{
    size_t objectVertexStart = vertexBase;
    size_t objectVertexCount = 0;
    while(file.tellg() < endPos) {
        Chunk ch; if(!readChunk(file, ch)) return;
        long dataStart = file.tellg();
        long next = dataStart + ch.length - 6;
        switch(ch.id) {
            case 0x4110: {
                uint16_t count;
                file.read(reinterpret_cast<char*>(&count), 2);

                size_t start = mesh.vertices.size();
                mesh.vertices.resize(start + static_cast<size_t>(count) * 3);
                file.read(reinterpret_cast<char*>(mesh.vertices.data() + start),
                          static_cast<size_t>(count) * 3 * sizeof(float));
                objectVertexStart = start / 3;
                objectVertexCount = static_cast<size_t>(count);

                break;
            }
            case 0x4120: {
                uint16_t count;
                file.read(reinterpret_cast<char*>(&count), 2);

                size_t start = mesh.indices.size();
                mesh.indices.resize(start + static_cast<size_t>(count) * 3);

                for (int i = 0; i < count; i++) {
                    uint16_t a, b, c, flag;
                    file.read(reinterpret_cast<char*>(&a), 2);
                    file.read(reinterpret_cast<char*>(&b), 2);
                    file.read(reinterpret_cast<char*>(&c), 2);
                    file.read(reinterpret_cast<char*>(&flag), 2);

                    mesh.indices[start + i * 3] = static_cast<unsigned short>(a + vertexBase);
                    mesh.indices[start + i * 3 + 1] = static_cast<unsigned short>(b + vertexBase);
                    mesh.indices[start + i * 3 + 2] = static_cast<unsigned short>(c + vertexBase);
                }
                while (file.tellg() < next) {
                    Chunk sub;
                    if (!readChunk(file, sub))
                        break;
                    const long subDataStart = file.tellg();
                    const long subNext = subDataStart + sub.length - 6;
                    if (sub.id == 0x4130) { // face material group
                        std::string materialName = readCString(file, subNext);
                        uint16_t faceCount = 0;
                        file.read(reinterpret_cast<char*>(&faceCount), 2);
                        const auto mit = materials.find(materialName);
                        if (mit != materials.end()) {
                            const auto& c = mit->second;
                            colorAccum.r += static_cast<double>(c[0]) * faceCount;
                            colorAccum.g += static_cast<double>(c[1]) * faceCount;
                            colorAccum.b += static_cast<double>(c[2]) * faceCount;
                            colorAccum.weight += faceCount;
                        }
                    }
                    file.seekg(subNext);
                }
                break;
            }
            case 0x4140: {
                uint16_t count = 0;
                file.read(reinterpret_cast<char*>(&count), 2);
                const size_t uvCount = static_cast<size_t>(count);
                if (objectVertexCount > 0 && uvCount == objectVertexCount) {
                    const size_t requiredSize = (objectVertexStart + objectVertexCount) * 2;
                    if (mesh.texcoords.size() < requiredSize)
                        mesh.texcoords.resize(requiredSize, 0.0f);
                    for (size_t i = 0; i < uvCount; ++i) {
                        float u = 0.0f;
                        float v = 0.0f;
                        file.read(reinterpret_cast<char*>(&u), sizeof(float));
                        file.read(reinterpret_cast<char*>(&v), sizeof(float));
                        const float flippedV = 1.0f - v;
                        mesh.texcoords[(objectVertexStart + i) * 2] = u;
                        mesh.texcoords[(objectVertexStart + i) * 2 + 1] = flippedV;
                    }
                } else {
                    file.seekg(next);
                }
                break;
            }
            default:
                file.seekg(next);
        }
    }
}

bool Load3DS(const std::string& path, Mesh& outMesh)
{
    std::ifstream file(path, std::ios::binary);
    if(!file.is_open())
        return false;

    Chunk root; if(!readChunk(file, root)) return false;
    if(root.id != 0x4D4D) return false;
    long rootEnd = root.length;

    std::unordered_map<std::string, std::array<float, 3>> materials;
    FaceColorAccum colorAccum;
    std::string textureFilename;
    const fs::path modelPath = fs::u8path(path);
    const fs::path modelDir = modelPath.has_parent_path() ? modelPath.parent_path() : fs::path();

    while(file.tellg() < rootEnd) {
        Chunk c; if(!readChunk(file,c)) break;
        long dataStart = file.tellg();
        long next = dataStart + c.length - 6;
        if(c.id == 0x3D3D) {
            while(file.tellg() < next) {
                Chunk sub; if(!readChunk(file,sub)) break;
                long sd = file.tellg();
                long se = sd + sub.length - 6;
                if(sub.id == 0x4000) {
                    // skip object name
                    char ch;
                    do { file.read(&ch,1); } while(ch!=0 && file.tellg() < se);
                    while(file.tellg() < se) {
                        Chunk mc; if(!readChunk(file,mc)) break;
                        long md = file.tellg();
                        long me = md + mc.length - 6;
                        if(mc.id == 0x4100) {
                            size_t base = outMesh.vertices.size() / 3;
                            parseMesh(file, me, outMesh, base, materials, colorAccum);
                        } else {
                            file.seekg(me);
                        }
                    }
                } else if (sub.id == 0xAFFF) { // material block
                    std::string materialName;
                    std::array<float, 3> materialColor{1.0f, 1.0f, 1.0f};
                    while (file.tellg() < se) {
                        Chunk mc;
                        if (!readChunk(file, mc))
                            break;
                        const long md = file.tellg();
                        const long me = md + mc.length - 6;
                        if (mc.id == 0xA000) {
                            materialName = readCString(file, me);
                        } else if (mc.id == 0xA020) {
                            bool colorOk = false;
                            materialColor = readColorChunk(file, me, colorOk);
                        } else if (mc.id == 0xA200) {
                            while (file.tellg() < me) {
                                Chunk texChunk;
                                if (!readChunk(file, texChunk))
                                    break;
                                const long texDataStart = file.tellg();
                                const long texNext = texDataStart + texChunk.length - 6;
                                if (texChunk.id == 0xA300) {
                                    std::string candidate = readCString(file, texNext);
                                    if (!candidate.empty())
                                        textureFilename = candidate;
                                } else if (textureFilename.empty()) {
                                    std::string candidate = readCString(file, texNext);
                                    if (!candidate.empty())
                                        textureFilename = candidate;
                                }
                                file.seekg(texNext);
                            }
                        }
                        file.seekg(me);
                    }
                    if (!materialName.empty())
                        materials[materialName] = materialColor;
                } else {
                    file.seekg(se);
                }
            }
        } else {
            file.seekg(next);
        }
    }

    bool ok = !outMesh.vertices.empty() && !outMesh.indices.empty();
    if (ok && colorAccum.weight > 0.0) {
        outMesh.materialBaseColor = {
            static_cast<float>(colorAccum.r / colorAccum.weight),
            static_cast<float>(colorAccum.g / colorAccum.weight),
            static_cast<float>(colorAccum.b / colorAccum.weight)};
        outMesh.hasMaterialBaseColor = true;
    }
    const size_t vertexCount = outMesh.vertices.size() / 3;
    if (!outMesh.texcoords.empty()) {
        const size_t expectedSize = vertexCount * 2;
        if (outMesh.texcoords.size() < expectedSize)
            outMesh.texcoords.resize(expectedSize, 0.0f);
        else if (outMesh.texcoords.size() > expectedSize)
            outMesh.texcoords.resize(expectedSize);
    }
    if (ok && !textureFilename.empty())
        LoadTextureImage(modelDir, textureFilename, outMesh);
    if (ok)
        ComputeNormals(outMesh);
    if (kLog3dsMessages && ConsolePanel::Instance()) {
        if (ok) {
            wxString msg = wxString::Format("3DS: %s -> v=%zu i=%zu",
                                           wxString::FromUTF8(path),
                                           outMesh.vertices.size()/3,
                                           outMesh.indices.size()/3);
            ConsolePanel::Instance()->AppendMessage(msg);
        } else {
            wxString msg = wxString::Format("3DS: parsed but empty %s", wxString::FromUTF8(path));
            ConsolePanel::Instance()->AppendMessage(msg);
        }
    }
    return ok;
}
