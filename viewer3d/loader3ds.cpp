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
#include "loader3ds_reader.h"
#include <limits>
#include "filesystem_path_utils.h"
#include <fstream>
#include <cstdint>
#include <array>
#include <cctype>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <wx/wx.h>
#include <wx/image.h>

namespace fs = std::filesystem;

using loader3ds::Reader;

struct FaceColorAccum {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double weight = 0.0;
};

struct MeshTransform3ds {
    std::array<float, 3> xAxis = {1.0f, 0.0f, 0.0f};
    std::array<float, 3> yAxis = {0.0f, 1.0f, 0.0f};
    std::array<float, 3> zAxis = {0.0f, 0.0f, 1.0f};
    std::array<float, 3> origin = {0.0f, 0.0f, 0.0f};
};

// Returns whether a 3DS local basis is effectively an identity basis.
static bool IsIdentityBasis(const MeshTransform3ds& transform)
{
    constexpr float kEpsilon = 1e-5f;
    auto almostEqual = [](float a, float b) {
        return std::abs(a - b) <= kEpsilon;
    };
    return almostEqual(transform.xAxis[0], 1.0f) &&
           almostEqual(transform.xAxis[1], 0.0f) &&
           almostEqual(transform.xAxis[2], 0.0f) &&
           almostEqual(transform.yAxis[0], 0.0f) &&
           almostEqual(transform.yAxis[1], 1.0f) &&
           almostEqual(transform.yAxis[2], 0.0f) &&
           almostEqual(transform.zAxis[0], 0.0f) &&
           almostEqual(transform.zAxis[1], 0.0f) &&
           almostEqual(transform.zAxis[2], 1.0f);
}

// Ensures wx image handlers are available without registering duplicates.
static bool EnsureImageHandlersInitialized()
{
    if (wxImage::FindHandler(wxBITMAP_TYPE_PNG) == nullptr)
        wxInitAllImageHandlers();
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

    fs::path texturePath = PathUtils::PathFromUtf8(filename);
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

static std::array<float, 3> readColorChunk(Reader& reader, std::streamoff end)
{
    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
    bool found = false;
    reader.children(end, [&](Reader::Chunk ch) {
        if (ch.id == 0x0010 || ch.id == 0x0013) {
            std::array<float, 3> candidate;
            reader.read(candidate.data(), sizeof(float) * 3, ch.end);
            if (!found) color = candidate;
            found = true;
        } else if (ch.id == 0x0011 || ch.id == 0x0012) {
            unsigned char rgb[3];
            reader.read(rgb, 3, ch.end);
            if (!found) color = {rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f};
            found = true;
        }
    });
    return color;
}

// Keep indices local until the complete mesh has been read. Faces may precede
// vertices, and must never resolve against vertices from another object.
static void parseMesh(Reader& reader, std::streamoff end, Mesh& output,
                      const std::unordered_map<std::string, std::array<float, 3>>& materials,
                      FaceColorAccum& colorAccum, bool applyObjectLocalTransform)
{
    Mesh mesh;
    bool hasVertices = false;
    bool hasFaces = false;
    MeshTransform3ds localTransform;
    bool hasLocalTransform = false;
    std::array<float, 3> pivot = {0.0f, 0.0f, 0.0f};
    bool hasPivot = false;
    reader.children(end, [&](Reader::Chunk ch) {
        switch (ch.id) {
            case 0x4110: {
                if (hasVertices) throw std::runtime_error("duplicate 3DS vertex list");
                hasVertices = true;
                const auto count = reader.value<uint16_t>(ch.end);
                reader.require(static_cast<std::streamoff>(count) * 12, ch.end);
                mesh.vertices.resize(static_cast<size_t>(count) * 3);
                reader.read(mesh.vertices.data(), static_cast<std::streamsize>(count) * 12, ch.end);
                break;
            }
            case 0x4120: {
                if (hasFaces) throw std::runtime_error("duplicate 3DS face list");
                hasFaces = true;
                const auto count = reader.value<uint16_t>(ch.end);
                reader.require(static_cast<std::streamoff>(count) * 8, ch.end);
                mesh.indices.reserve(static_cast<size_t>(count) * 3);
                for (size_t i = 0; i < count; ++i) {
                    for (int corner = 0; corner < 3; ++corner)
                        mesh.indices.push_back(reader.value<uint16_t>(ch.end));
                    reader.value<uint16_t>(ch.end); // face flags
                }
                reader.children(ch.end, [&](Reader::Chunk sub) {
                    if (sub.id != 0x4130) return;
                    const auto materialName = reader.string(sub.end);
                    const auto faceCount = reader.value<uint16_t>(sub.end);
                    reader.require(static_cast<std::streamoff>(faceCount) * 2, sub.end);
                    for (size_t i = 0; i < faceCount; ++i) {
                        if (reader.value<uint16_t>(sub.end) >= count)
                            throw std::runtime_error("3DS material group face index is out of range");
                    }
                    const auto mit = materials.find(materialName);
                    if (mit != materials.end()) {
                        const auto& c = mit->second;
                        colorAccum.r += static_cast<double>(c[0]) * faceCount;
                        colorAccum.g += static_cast<double>(c[1]) * faceCount;
                        colorAccum.b += static_cast<double>(c[2]) * faceCount;
                        colorAccum.weight += faceCount;
                    }
                });
                break;
            }
            case 0x4140: {
                const auto count = reader.value<uint16_t>(ch.end);
                reader.require(static_cast<std::streamoff>(count) * 8, ch.end);
                mesh.texcoords.resize(static_cast<size_t>(count) * 2);
                reader.read(mesh.texcoords.data(), static_cast<std::streamsize>(count) * 8, ch.end);
                for (size_t i = 1; i < mesh.texcoords.size(); i += 2)
                    mesh.texcoords[i] = 1.0f - mesh.texcoords[i];
                break;
            }
            case 0x4160:
                reader.require(sizeof(float) * 12, ch.end);
                reader.read(localTransform.xAxis.data(), sizeof(float) * 3, ch.end);
                reader.read(localTransform.yAxis.data(), sizeof(float) * 3, ch.end);
                reader.read(localTransform.zAxis.data(), sizeof(float) * 3, ch.end);
                reader.read(localTransform.origin.data(), sizeof(float) * 3, ch.end);
                hasLocalTransform = true;
                break;
            case 0x4165:
                reader.read(pivot.data(), sizeof(float) * 3, ch.end);
                hasPivot = true;
                break;
        }
    });
    if (!HasValidTriangleIndices(mesh))
        throw std::runtime_error("3DS face index is outside its mesh vertex list");
    const size_t objectVertexStart = 0;
    const size_t objectVertexCount = mesh.vertices.size() / 3;
    const bool shouldApplyLocalBasis = applyObjectLocalTransform &&
                                       hasLocalTransform &&
                                       !IsIdentityBasis(localTransform);
    if (objectVertexCount > 0 && (shouldApplyLocalBasis || (applyObjectLocalTransform && hasPivot))) {
        for (size_t i = 0; i < objectVertexCount; ++i) {
            const size_t index = (objectVertexStart + i) * 3;
            float x = mesh.vertices[index];
            float y = mesh.vertices[index + 1];
            float z = mesh.vertices[index + 2];

            if (applyObjectLocalTransform && hasPivot) {
                x -= pivot[0];
                y -= pivot[1];
                z -= pivot[2];
            }

            if (shouldApplyLocalBasis) {
                const float tx = localTransform.origin[0] +
                                 localTransform.xAxis[0] * x +
                                 localTransform.yAxis[0] * y +
                                 localTransform.zAxis[0] * z;
                const float ty = localTransform.origin[1] +
                                 localTransform.xAxis[1] * x +
                                 localTransform.yAxis[1] * y +
                                 localTransform.zAxis[1] * z;
                const float tz = localTransform.origin[2] +
                                 localTransform.xAxis[2] * x +
                                 localTransform.yAxis[2] * y +
                                 localTransform.zAxis[2] * z;
                mesh.vertices[index] = tx;
                mesh.vertices[index + 1] = ty;
                mesh.vertices[index + 2] = tz;
            } else {
                mesh.vertices[index] = x;
                mesh.vertices[index + 1] = y;
                mesh.vertices[index + 2] = z;
            }
        }
    }
    const size_t base = output.vertices.size() / 3;
    if (base > std::numeric_limits<uint32_t>::max() - objectVertexCount)
        throw std::runtime_error("too many vertices in 3DS model");
    output.vertices.insert(output.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
    for (uint32_t index : mesh.indices)
        output.indices.push_back(static_cast<uint32_t>(base + index));
    if (mesh.texcoords.size() == objectVertexCount * 2 && !mesh.texcoords.empty()) {
        output.texcoords.resize((base + objectVertexCount) * 2, 0.0f);
        std::copy(mesh.texcoords.begin(), mesh.texcoords.end(), output.texcoords.begin() + base * 2);
    }

}

// Parse into a private mesh: failure leaves the caller's mesh untouched.
bool Load3DS(const std::string& path, Mesh& outMesh, bool applyObjectLocalTransform,
             std::string* error, bool loadTextures)
{
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) throw std::runtime_error("cannot open file");
        Reader reader(file);
        const auto root = reader.chunk(reader.size);
        if (root.id != 0x4D4D) throw std::runtime_error("unexpected 3DS root chunk id");
        if (root.end != reader.size) throw std::runtime_error("3DS root length does not match file size");

        Mesh mesh;
        std::unordered_map<std::string, std::array<float, 3>> materials;
        FaceColorAccum colorAccum;
        std::string textureFilename;
        reader.children(root.end, [&](Reader::Chunk editor) {
            if (editor.id != 0x3D3D) return;
            reader.children(editor.end, [&](Reader::Chunk sub) {
                if (sub.id == 0x4000) {
                    reader.string(sub.end); // object name
                    reader.children(sub.end, [&](Reader::Chunk child) {
                        if (child.id == 0x4100)
                            parseMesh(reader, child.end, mesh, materials, colorAccum,
                                      applyObjectLocalTransform);
                    });
                } else if (sub.id == 0xAFFF) {
                    std::string materialName;
                    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
                    reader.children(sub.end, [&](Reader::Chunk child) {
                        if (child.id == 0xA000) materialName = reader.string(child.end);
                        else if (child.id == 0xA020) color = readColorChunk(reader, child.end);
                        else if (child.id == 0xA200) {
                            reader.children(child.end, [&](Reader::Chunk texture) {
                                if (texture.id == 0xA300) {
                                    const auto candidate = reader.string(texture.end);
                                    if (!candidate.empty()) textureFilename = candidate;
                                }
                            });
                        }
                    });
                    if (!materialName.empty()) materials[materialName] = color;
                }
            });
        });
        if (mesh.vertices.empty() || mesh.indices.empty())
            throw std::runtime_error("no vertices or faces found");
        if (colorAccum.weight > 0.0) {
            mesh.materialBaseColor = {
                static_cast<float>(colorAccum.r / colorAccum.weight),
                static_cast<float>(colorAccum.g / colorAccum.weight),
                static_cast<float>(colorAccum.b / colorAccum.weight)};
            mesh.hasMaterialBaseColor = true;
        }
        if (!mesh.texcoords.empty()) mesh.texcoords.resize(mesh.vertices.size() / 3 * 2, 0.0f);
        if (loadTextures && !textureFilename.empty()) {
            // Missing or inaccessible textures must not discard valid geometry.
            try {
                const auto modelPath = PathUtils::PathFromUtf8(path);
                LoadTextureImage(modelPath.parent_path(), textureFilename, mesh);
            } catch (const fs::filesystem_error&) {}
        }
        ComputeNormals(mesh);
        outMesh = std::move(mesh);
        if (error) error->clear();
        return true;
    } catch (const std::exception& ex) {
        if (error) *error = ex.what();
        return false;
    }
}
