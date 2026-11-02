#include "loader3ds.h"
#include "loaderglb.h"
#include "loader_obj.h"
#include "support/three_ds_test_utils.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <wx/init.h>

using namespace three_ds_test;
using Bytes = std::vector<uint8_t>;
namespace fs = std::filesystem;

static void Join(Bytes& to, const Bytes& from) {
    to.insert(to.end(), from.begin(), from.end());
}

static Bytes Header(uint16_t id, uint32_t length) {
    Bytes bytes;
    AppendValue(bytes, id);
    AppendValue(bytes, length);
    return bytes;
}

static Bytes Vertices(uint16_t count = 3) {
    Bytes bytes;
    AppendValue(bytes, count);
    const float triangle[] = {1, 2, 3, 11, 2, 3, 1, 22, 3};
    for (size_t i = 0; i < static_cast<size_t>(count) * 3; ++i)
        AppendFloat(bytes, i < 9 ? triangle[i] : 0.0f);
    return bytes;
}

static Bytes Faces(uint16_t index = 2) {
    Bytes bytes;
    for (uint16_t value : {uint16_t(1), uint16_t(0), uint16_t(1), index, uint16_t(0)})
        AppendValue(bytes, value);
    return bytes;
}

static Bytes Geometry(uint16_t index = 2, bool facesFirst = false) {
    Bytes bytes;
    if (facesFirst) AppendChunk(bytes, 0x4120, Faces(index));
    AppendChunk(bytes, 0x4110, Vertices());
    if (!facesFirst) AppendChunk(bytes, 0x4120, Faces(index));
    return bytes;
}

static Bytes Object(const Bytes& geometry) {
    Bytes object{'o', 0};
    AppendChunk(object, 0x4100, geometry);
    return Chunk(0x4000, object);
}

static Bytes File(const Bytes& editor) {
    return Chunk(0x4D4D, Chunk(0x3D3D, editor));
}

static bool Write(const fs::path& path, const Bytes& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return file.good();
}

static Bytes Glb(uint16_t index) {
    std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":44}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":6}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1},{"attributes":{"POSITION":0},"indices":1}]}],
"nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
    while (json.size() % 4) json.push_back(' ');
    Bytes bin;
    for (float f : {0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f}) AppendFloat(bin, f);
    for (uint16_t v : {uint16_t(0),uint16_t(1),index}) AppendValue(bin, v);
    while (bin.size() % 4) bin.push_back(0);
    Bytes bytes;
    for (uint32_t v : {0x46546c67u,2u,static_cast<uint32_t>(28 + json.size() + bin.size()),
                       static_cast<uint32_t>(json.size()),0x4e4f534au}) AppendValue(bytes, v);
    bytes.insert(bytes.end(), json.begin(), json.end());
    AppendValue<uint32_t>(bytes, bin.size()); AppendValue<uint32_t>(bytes, 0x004e4942);
    Join(bytes, bin);
    return bytes;
}

static int failures = 0;
static void Check(bool condition, const std::string& name) {
    if (!condition) {
        std::cerr << name << '\n';
        ++failures;
    }
}

int main(int argc, char** argv) {
    wxInitializer wx;
    if (!wx.IsOk()) return 1;
    const auto dir = fs::temp_directory_path() /
        ("perastage_3ds_input_validation_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); }
    } cleanup{dir};
    const auto path = dir / "model.3ds";
    auto reject = [&](const std::string& name, const Bytes& bytes) {
        if (argc > 1 && name != argv[1]) return;
        Check(Write(path, bytes), name + ": write failed");
        Mesh mesh;
        mesh.vertices = {9, 8, 7}; // failures must not publish partial geometry
        const auto original = mesh.vertices;
        std::string error;
        const bool loaded = Load3DS(path.string(), mesh, true, &error, false);
        Check(!loaded && !error.empty(), name + ": accepted malformed input: " + error);
        Check(mesh.vertices == original && mesh.indices.empty(), name + ": output changed on failure");
    };
    auto accept = [&](const Bytes& bytes, Mesh& mesh, bool transform = true) {
        Check(Write(path, bytes), "valid write failed");
        std::string error;
        const bool loaded = Load3DS(path.string(), mesh, transform, &error, false);
        Check(loaded, "valid load failed: " + error);
        Check(HasValidTriangleIndices(mesh), "invalid successful topology");
    };

    // The optional filter allows isolated, bounded characterization of old code.
    if (argc == 1) {
        Mesh mesh;
        accept(File(Object(Geometry())), mesh);
        Check(mesh.vertices == std::vector<float>({1,2,3,11,2,3,1,22,3}) &&
              mesh.indices == std::vector<uint32_t>({0,1,2}), "minimal coordinates/topology");
        Check(mesh.normals.size() == 9 && mesh.normals[2] == 1, "minimal normals");
        EnsureOutwardWinding(mesh);
        BuildFlatShadingBuffers(mesh);
        Check(mesh.flatVertices == mesh.vertices && mesh.flatNormals == mesh.normals &&
              mesh.indices == std::vector<uint32_t>({0,1,2}), "valid flat shading and winding");
        Bytes objects = Object(Geometry());
        Join(objects, Object(Geometry(2, true)));
        accept(File(objects), mesh);
        Check(mesh.indices == std::vector<uint32_t>({0,1,2,3,4,5}) && mesh.vertices.size() == 18,
              "multiple objects and face-first ordering");
        Bytes geometry;
        Bytes uv; AppendValue<uint16_t>(uv, 3);
        for (float f : {0.f,0.f,1.f,0.f,0.f,1.f}) AppendFloat(uv, f);
        // UVs before positions are valid and remain local to this mesh.
        geometry = Chunk(0x4140, uv);
        Join(geometry, Geometry());
        Bytes basis;
        for (float f : {2.f,0.f,0.f,0.f,3.f,0.f,0.f,0.f,4.f,10.f,20.f,30.f})
            AppendFloat(basis, f);
        AppendChunk(geometry, 0x4160, basis);
        Bytes pivot;
        for (float f : {1.f,2.f,3.f}) AppendFloat(pivot, f);
        AppendChunk(geometry, 0x4165, pivot);
        accept(File(Object(geometry)), mesh);
        Check(mesh.vertices == std::vector<float>({10,20,30,30,20,30,10,80,30}), "local transform/pivot");
        Check(mesh.texcoords == std::vector<float>({0,1,1,1,0,0}), "UV coordinates");
        Check(mesh.indices == std::vector<uint32_t>({0,1,2}), "transformed topology");
        accept(File(Object(geometry)), mesh, false);
        Check(mesh.vertices == std::vector<float>({1,2,3,11,2,3,1,22,3}), "native dimensions");
        Bytes large;
        AppendChunk(large, 0x4110, Vertices(65535));
        AppendChunk(large, 0x4120, Faces());
        objects = Object(large); Join(objects, Object(large));
        accept(File(objects), mesh);
        Check(mesh.indices == std::vector<uint32_t>({0,1,2,65535,65536,65537}), "32-bit object offsets");
        Bytes unknown = Geometry(); AppendChunk(unknown, 0x7777, {0,0,0});
        accept(File(Object(unknown)), mesh);
        Bytes material = Chunk(0xA000, {'m',0});
        AppendChunk(material, 0xA020, Chunk(0x0011, {255,128,0}));
        Bytes texture = Chunk(0xA300, {'m','.','p','n','g',0});
        AppendChunk(texture, 0xA351, {1,0}); // binary map option, not a filename
        AppendChunk(material, 0xA200, texture);
        Bytes materialFaces = Faces();
        AppendChunk(materialFaces, 0x4130, {'m',0,1,0,0,0});
        Bytes coloredGeometry = Chunk(0x4110, Vertices());
        AppendChunk(coloredGeometry, 0x4120, materialFaces);
        Bytes coloredEditor = Chunk(0xAFFF, material);
        Join(coloredEditor, Object(coloredGeometry));
        accept(File(coloredEditor), mesh);
        Check(mesh.hasMaterialBaseColor && mesh.materialBaseColor[0] == 1 &&
              std::abs(mesh.materialBaseColor[1] - 128.f/255.f) < 1e-6f &&
              mesh.materialBaseColor[2] == 0, "material face colors");
        // Multiple meshes in a single named object use independent bases.
        Bytes object{'o',0}; AppendChunk(object, 0x4100, Geometry());
        AppendChunk(object, 0x4100, Geometry());
        accept(File(Chunk(0x4000, object)), mesh);
        Check(mesh.indices == std::vector<uint32_t>({0,1,2,3,4,5}), "multiple mesh offsets");
    }
    reject("index_equal_count", File(Object(Geometry(3))));
    reject("index_60000", File(Object(Geometry(60000))));
    Bytes crossed = Object(Geometry()); Join(crossed, Object(Geometry(3, true)));
    Join(crossed, Object(Geometry()));
    reject("cross_object_index", File(crossed));
    for (uint32_t length : {0u,1u,5u}) {
        reject("root_length_" + std::to_string(length), Header(0x4D4D, length));
        reject("child_length_" + std::to_string(length), File(Header(0x7777, length)));
        Bytes geometry = Geometry(); Join(geometry, Header(0x7777, length));
        reject("nested_length_" + std::to_string(length), File(Object(geometry)));
    }
    Bytes parentPayload = Chunk(0x3D3D, Header(0x4000, 7));
    AppendChunk(parentPayload, 0x7777, {0});
    reject("parent_boundary", Chunk(0x4D4D, parentPayload));
    reject("maximum_length", File(Header(0x7777, 0xffffffffu)));
    reject("root_header", {0x4d,0x4d});
    reject("root_id", Chunk(0x1234, {}));
    Bytes badObject{'o',0}; Join(badObject, Header(0x7777, 0));
    reject("object_nested", File(Chunk(0x4000, badObject)));
    Bytes badFaces = Faces(); Join(badFaces, Header(0x7777, 0));
    Bytes badGeometry = Chunk(0x4110, Vertices()); AppendChunk(badGeometry, 0x4120, badFaces);
    reject("face_nested", File(Object(badGeometry)));
    auto physical = File(Object(Geometry())); physical.pop_back();
    reject("physical_boundary", physical);
    physical = File(Object(Geometry())); physical.push_back(0);
    reject("root_shorter_than_file", physical);
    reject("nested_header", File(Object({0x10,0x41,0})));
    reject("editor_header", File({0}));
    reject("object_name", File(Chunk(0x4000, {'n','o'})));
    auto shortVertices = Vertices(); shortVertices.pop_back();
    Bytes geometry; AppendChunk(geometry, 0x4110, shortVertices);
    AppendChunk(geometry, 0x4120, Faces());
    reject("truncated_vertices", File(Object(geometry)));
    auto shortFaces = Faces(); shortFaces.pop_back();
    geometry = Chunk(0x4110, Vertices()); AppendChunk(geometry, 0x4120, shortFaces);
    reject("truncated_faces", File(Object(geometry)));
    for (uint16_t id : {0x4140,0x4160,0x4165}) {
        geometry = Geometry(); AppendChunk(geometry, id, {3,0});
        reject("truncated_fixed_" + std::to_string(id), File(Object(geometry)));
    }
    Bytes faces = Faces(); AppendChunk(faces, 0x4130, {'m',0,1,0});
    geometry = Chunk(0x4110, Vertices()); AppendChunk(geometry, 0x4120, faces);
    reject("truncated_material_faces", File(Object(geometry)));
    faces = Faces(); AppendChunk(faces, 0x4130, {'m',0,1,0,1,0});
    geometry = Chunk(0x4110, Vertices()); AppendChunk(geometry, 0x4120, faces);
    reject("invalid_material_face", File(Object(geometry)));
    for (uint16_t container : {0xA020,0xA200}) {
        Bytes editor = Chunk(0xAFFF, Chunk(container, Header(0x7777, 0)));
        Join(editor, Object(Geometry()));
        reject("material_nested_" + std::to_string(container), File(editor));
    }
    for (uint16_t id : {0x0010,0x0011}) {
        Bytes editor = Chunk(0xAFFF, Chunk(0xA020, Chunk(id, {0,0})));
        Join(editor, Object(Geometry()));
        reject("truncated_color_" + std::to_string(id), File(editor));
    }
    Bytes editor = Chunk(0xAFFF, Chunk(0xA200, Chunk(0xA300, {'x'})));
    Join(editor, Object(Geometry())); reject("texture_name", File(editor));
    if (argc == 1) {
        const auto glb = dir / "model.glb";
        Mesh glbMesh; std::string glbError;
        Check(Write(glb, Glb(2)), "valid GLB write failed");
        const bool glbLoaded = LoadGLB(glb.string(), glbMesh, &glbError);
        Check(glbLoaded, "valid GLB failed: " + glbError);
        Check(glbMesh.indices == std::vector<uint32_t>({0,1,2,3,4,5}), "GLB primitive offsets");
        for (uint16_t index : {3,60000}) {
            Check(Write(glb, Glb(index)), "GLB write failed");
            Check(!LoadGLB(glb.string(), glbMesh, &glbError) && !glbError.empty() &&
                  glbMesh.vertices.empty() && glbMesh.indices.empty(), "GLB invalid primitive indices");
        }
        // Direct callers and cached meshes can bypass loader validation.
        for (uint32_t index : {3u,60000u,std::numeric_limits<uint32_t>::max()}) {
            Mesh mesh; mesh.vertices = {0,0,0,1,0,0,0,1,0}; mesh.indices = {0,1,index};
            Check(!HasValidTriangleIndices(mesh), "shared validation accepted invalid index");
            EnsureOutwardWinding(mesh); ComputeNormals(mesh); BuildFlatShadingBuffers(mesh);
            Check(mesh.normals.empty() && mesh.flatVertices.empty() && !mesh.windingChecked,
                  "unsafe direct processing");
        }
        Mesh mesh; mesh.vertices = {0,0,0}; mesh.indices = {0};
        Check(!HasValidTriangleIndices(mesh), "incomplete triangle accepted");
        Check(!IsMeshTriangleIndexRangeValid(mesh, std::numeric_limits<size_t>::max()), "range overflow");
        const auto obj = dir / "invalid.obj";
        { std::ofstream out(obj); out << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 60000\n"; }
        std::string error;
        Check(!LoadOBJ(obj.string(), mesh, &error), "OBJ accepted invalid face index");
    }
    return failures ? 1 : 0;
}
