#include "mesh_3ds_loader.h"

#include "coordinate_mapper.h"

#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr float kMillimetersToMeters = 0.001F;

struct Chunk {
    uint16_t id = 0;
    uint32_t length = 0;
};

struct MeshData {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::string texture_path;
    std::array<float, 3> material_base_color = {1.0F, 1.0F, 1.0F};
    bool has_material_base_color = false;
};

struct FaceColorAccum {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double weight = 0.0;
};

struct MaterialInfo {
    std::array<float, 3> color = {1.0F, 1.0F, 1.0F};
    bool has_color = false;
};

bool read_chunk(std::ifstream &file, Chunk &chunk) {
    if (!file.read(reinterpret_cast<char *>(&chunk.id), sizeof(chunk.id))) {
        return false;
    }
    if (!file.read(reinterpret_cast<char *>(&chunk.length), sizeof(chunk.length))) {
        return false;
    }
    return true;
}

void ensure_godot_clockwise_winding(MeshData &mesh) {
    if (mesh.indices.size() < 3 || mesh.vertices.size() < 3) {
        return;
    }

    const size_t vertex_count = mesh.vertices.size() / 3;
    float cx = 0.0F;
    float cy = 0.0F;
    float cz = 0.0F;
    for (size_t i = 0; i < vertex_count; ++i) {
        cx += mesh.vertices[i * 3];
        cy += mesh.vertices[i * 3 + 1];
        cz += mesh.vertices[i * 3 + 2];
    }
    const float inv_vertex_count = 1.0F / static_cast<float>(vertex_count);
    cx *= inv_vertex_count;
    cy *= inv_vertex_count;
    cz *= inv_vertex_count;

    float orientation_score = 0.0F;
    float total_area = 0.0F;

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t i0 = mesh.indices[i];
        const uint32_t i1 = mesh.indices[i + 1];
        const uint32_t i2 = mesh.indices[i + 2];
        if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) {
            continue;
        }

        const float v0x = mesh.vertices[i0 * 3];
        const float v0y = mesh.vertices[i0 * 3 + 1];
        const float v0z = mesh.vertices[i0 * 3 + 2];

        const float v1x = mesh.vertices[i1 * 3];
        const float v1y = mesh.vertices[i1 * 3 + 1];
        const float v1z = mesh.vertices[i1 * 3 + 2];

        const float v2x = mesh.vertices[i2 * 3];
        const float v2y = mesh.vertices[i2 * 3 + 1];
        const float v2z = mesh.vertices[i2 * 3 + 2];

        const float ux = v1x - v0x;
        const float uy = v1y - v0y;
        const float uz = v1z - v0z;

        const float vx = v2x - v0x;
        const float vy = v2y - v0y;
        const float vz = v2z - v0z;

        const float nx = uy * vz - uz * vy;
        const float ny = uz * vx - ux * vz;
        const float nz = ux * vy - uy * vx;
        const float area = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (area <= 1e-8F) {
            continue;
        }

        const float tx = (v0x + v1x + v2x) / 3.0F;
        const float ty = (v0y + v1y + v2y) / 3.0F;
        const float tz = (v0z + v1z + v2z) / 3.0F;

        orientation_score += nx * (tx - cx) + ny * (ty - cy) + nz * (tz - cz);
        total_area += area;
    }

    if (total_area <= 1e-8F || std::abs(orientation_score) <= total_area * 1e-4F) {
        return;
    }

    // Godot's ArrayMesh expects clockwise triangles as front faces.
    // Positive score means a globally CCW-oriented closed mesh in this RH basis.
    // Flip those meshes so culling works consistently in Godot.
    if (orientation_score > 0.0F) {
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
        }
    }
}

void compute_normals(MeshData &mesh) {
    const size_t vertex_count = mesh.vertices.size() / 3;
    mesh.normals.assign(vertex_count * 3, 0.0F);

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const uint32_t i0 = mesh.indices[i];
        const uint32_t i1 = mesh.indices[i + 1];
        const uint32_t i2 = mesh.indices[i + 2];
        if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) {
            continue;
        }

        const float v0x = mesh.vertices[i0 * 3];
        const float v0y = mesh.vertices[i0 * 3 + 1];
        const float v0z = mesh.vertices[i0 * 3 + 2];

        const float v1x = mesh.vertices[i1 * 3];
        const float v1y = mesh.vertices[i1 * 3 + 1];
        const float v1z = mesh.vertices[i1 * 3 + 2];

        const float v2x = mesh.vertices[i2 * 3];
        const float v2y = mesh.vertices[i2 * 3 + 1];
        const float v2z = mesh.vertices[i2 * 3 + 2];

        const float ux = v1x - v0x;
        const float uy = v1y - v0y;
        const float uz = v1z - v0z;

        const float vx = v2x - v0x;
        const float vy = v2y - v0y;
        const float vz = v2z - v0z;

        // 3DS content arrives with clockwise winding for Godot front faces.
        // Use the opposite cross-product orientation so accumulated normals
        // still point outward.
        const float nx = -(uy * vz - uz * vy);
        const float ny = -(uz * vx - ux * vz);
        const float nz = -(ux * vy - uy * vx);

        mesh.normals[i0 * 3] += nx;
        mesh.normals[i0 * 3 + 1] += ny;
        mesh.normals[i0 * 3 + 2] += nz;

        mesh.normals[i1 * 3] += nx;
        mesh.normals[i1 * 3 + 1] += ny;
        mesh.normals[i1 * 3 + 2] += nz;

        mesh.normals[i2 * 3] += nx;
        mesh.normals[i2 * 3 + 1] += ny;
        mesh.normals[i2 * 3 + 2] += nz;
    }

    for (size_t i = 0; i < vertex_count; ++i) {
        const float nx = mesh.normals[i * 3];
        const float ny = mesh.normals[i * 3 + 1];
        const float nz = mesh.normals[i * 3 + 2];
        const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 1e-8F) {
            mesh.normals[i * 3] = nx / len;
            mesh.normals[i * 3 + 1] = ny / len;
            mesh.normals[i * 3 + 2] = nz / len;
        } else {
            mesh.normals[i * 3] = 0.0F;
            mesh.normals[i * 3 + 1] = 1.0F;
            mesh.normals[i * 3 + 2] = 0.0F;
        }
    }
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string read_cstring(std::ifstream &file, std::streampos end_pos) {
    std::string out;
    char ch = '\0';
    while (file.tellg() < end_pos && file.read(&ch, 1)) {
        if (ch == '\0') {
            break;
        }
        out.push_back(ch);
    }
    return out;
}

bool filename_equals_insensitive(const std::filesystem::path &a,
                                 const std::filesystem::path &b) {
    return to_lower_ascii(a.filename().u8string()) ==
           to_lower_ascii(b.filename().u8string());
}

bool find_texture_by_filename(const std::filesystem::path &root,
                              const std::filesystem::path &target_name,
                              std::filesystem::path &out_path) {
    if (root.empty() || !std::filesystem::exists(root) ||
        !std::filesystem::is_directory(root)) {
        return false;
    }

    constexpr size_t k_max_scanned_entries = 5000;
    constexpr int k_max_search_depth = 4;

    std::error_code ec;
    size_t scanned_entries = 0;
    for (std::filesystem::recursive_directory_iterator it(
             root, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (it.depth() > k_max_search_depth) {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_symlink(ec)) {
            continue;
        }
        if (ec) {
            ec.clear();
            continue;
        }
        if (!it->is_regular_file(ec)) {
            continue;
        }
        if (ec) {
            ec.clear();
            continue;
        }
        if (filename_equals_insensitive(it->path(), target_name)) {
            out_path = it->path();
            return true;
        }

        ++scanned_entries;
        if (scanned_entries >= k_max_scanned_entries) {
            break;
        }
    }
    return false;
}

bool resolve_texture_path(const std::filesystem::path &model_dir,
                          const std::string &texture_reference,
                          std::filesystem::path &out_path) {
    if (texture_reference.empty()) {
        return false;
    }

    std::filesystem::path texture_path = std::filesystem::u8path(texture_reference);
    if (texture_path.is_absolute() && std::filesystem::exists(texture_path)) {
        out_path = texture_path;
        return true;
    }

    const std::filesystem::path direct_candidate = model_dir / texture_path;
    if (std::filesystem::exists(direct_candidate)) {
        out_path = direct_candidate;
        return true;
    }

    std::filesystem::path current_dir = model_dir;
    while (!current_dir.empty()) {
        const std::filesystem::path ancestor_candidate = current_dir / texture_path;
        if (std::filesystem::exists(ancestor_candidate)) {
            out_path = ancestor_candidate;
            return true;
        }
        if (current_dir == current_dir.parent_path()) {
            break;
        }
        current_dir = current_dir.parent_path();
    }

    return find_texture_by_filename(model_dir, texture_path, out_path);
}

void parse_material_block(std::ifstream &file, std::streampos material_end,
                          std::unordered_map<std::string, MaterialInfo> &materials,
                          std::string &texture_filename) {
    std::string material_name;
    MaterialInfo material_info;

    while (file.tellg() < material_end) {
        Chunk chunk;
        if (!read_chunk(file, chunk)) {
            break;
        }
        const std::streampos data_start = file.tellg();
        const std::streampos next = data_start + static_cast<std::streamoff>(chunk.length - 6U);
        if (chunk.id == 0xA000) {
            material_name = read_cstring(file, next);
        } else if (chunk.id == 0xA020) {
            while (file.tellg() < next) {
                Chunk color_chunk;
                if (!read_chunk(file, color_chunk)) {
                    break;
                }
                const std::streampos color_data_start = file.tellg();
                const std::streampos color_next =
                    color_data_start + static_cast<std::streamoff>(color_chunk.length - 6U);
                if (color_chunk.id == 0x0010 || color_chunk.id == 0x0013) {
                    file.read(reinterpret_cast<char *>(&material_info.color[0]), sizeof(float));
                    file.read(reinterpret_cast<char *>(&material_info.color[1]), sizeof(float));
                    file.read(reinterpret_cast<char *>(&material_info.color[2]), sizeof(float));
                    material_info.has_color = true;
                } else if (color_chunk.id == 0x0011 || color_chunk.id == 0x0012) {
                    unsigned char rgb[3] = {255, 255, 255};
                    file.read(reinterpret_cast<char *>(rgb), 3);
                    material_info.color = {rgb[0] / 255.0F, rgb[1] / 255.0F, rgb[2] / 255.0F};
                    material_info.has_color = true;
                }
                file.seekg(color_next);
            }
        } else if (chunk.id == 0xA200) {
            while (file.tellg() < next) {
                Chunk tex_chunk;
                if (!read_chunk(file, tex_chunk)) {
                    break;
                }
                const std::streampos tex_data_start = file.tellg();
                const std::streampos tex_next =
                    tex_data_start + static_cast<std::streamoff>(tex_chunk.length - 6U);
                if (tex_chunk.id == 0xA300) {
                    const std::string candidate = read_cstring(file, tex_next);
                    if (!candidate.empty()) {
                        texture_filename = candidate;
                    }
                }
                file.seekg(tex_next);
            }
        }
        file.seekg(next);
    }

    if (!material_name.empty()) {
        materials[material_name] = material_info;
    }
}

void parse_mesh_chunk(std::ifstream &file, std::streampos mesh_end, MeshData &mesh,
                      size_t vertex_base,
                      const std::unordered_map<std::string, MaterialInfo> &materials,
                      FaceColorAccum &color_accum,
                      std::string &texture_filename) {
    size_t object_vertex_start = vertex_base;
    size_t object_vertex_count = 0;
    while (file.good() && file.tellg() < mesh_end) {
        Chunk chunk;
        if (!read_chunk(file, chunk)) {
            return;
        }

        const std::streampos data_start = file.tellg();
        const std::streampos next = data_start + static_cast<std::streamoff>(chunk.length - 6U);

        if (chunk.id == 0x4110) {
            uint16_t count = 0;
            file.read(reinterpret_cast<char *>(&count), sizeof(count));

            const size_t start = mesh.vertices.size();
            mesh.vertices.resize(start + static_cast<size_t>(count) * 3U);
            file.read(reinterpret_cast<char *>(mesh.vertices.data() + start),
                      static_cast<std::streamsize>(count) * 3 * sizeof(float));
            object_vertex_start = start / 3U;
            object_vertex_count = static_cast<size_t>(count);
        } else if (chunk.id == 0x4120) {
            uint16_t count = 0;
            file.read(reinterpret_cast<char *>(&count), sizeof(count));

            const size_t start = mesh.indices.size();
            mesh.indices.resize(start + static_cast<size_t>(count) * 3U);

            for (uint16_t i = 0; i < count; ++i) {
                uint16_t a = 0;
                uint16_t b = 0;
                uint16_t c = 0;
                uint16_t flag = 0;
                file.read(reinterpret_cast<char *>(&a), sizeof(a));
                file.read(reinterpret_cast<char *>(&b), sizeof(b));
                file.read(reinterpret_cast<char *>(&c), sizeof(c));
                file.read(reinterpret_cast<char *>(&flag), sizeof(flag));
                (void)flag;

                const size_t idx = start + static_cast<size_t>(i) * 3U;
                mesh.indices[idx] = static_cast<uint32_t>(a) + static_cast<uint32_t>(vertex_base);
                mesh.indices[idx + 1] =
                    static_cast<uint32_t>(b) + static_cast<uint32_t>(vertex_base);
                mesh.indices[idx + 2] =
                    static_cast<uint32_t>(c) + static_cast<uint32_t>(vertex_base);
            }
            while (file.tellg() < next) {
                Chunk sub_chunk;
                if (!read_chunk(file, sub_chunk)) {
                    break;
                }
                const std::streampos sub_data_start = file.tellg();
                const std::streampos sub_next =
                    sub_data_start + static_cast<std::streamoff>(sub_chunk.length - 6U);
                if (sub_chunk.id == 0x4130) {
                    const std::string material_name = read_cstring(file, sub_next);
                    uint16_t face_count = 0;
                    file.read(reinterpret_cast<char *>(&face_count), sizeof(face_count));
                    auto material_it = materials.find(material_name);
                    if (material_it != materials.end() && material_it->second.has_color) {
                        const std::array<float, 3> &c = material_it->second.color;
                        color_accum.r += static_cast<double>(c[0]) * face_count;
                        color_accum.g += static_cast<double>(c[1]) * face_count;
                        color_accum.b += static_cast<double>(c[2]) * face_count;
                        color_accum.weight += face_count;
                    }
                }
                file.seekg(sub_next);
            }
        } else if (chunk.id == 0x4140) {
            uint16_t count = 0;
            file.read(reinterpret_cast<char *>(&count), sizeof(count));
            const size_t uv_count = static_cast<size_t>(count);
            if (object_vertex_count > 0 && uv_count == object_vertex_count) {
                const size_t required_size = (object_vertex_start + object_vertex_count) * 2U;
                if (mesh.texcoords.size() < required_size) {
                    mesh.texcoords.resize(required_size, 0.0F);
                }
                for (size_t i = 0; i < uv_count; ++i) {
                    float u = 0.0F;
                    float v = 0.0F;
                    file.read(reinterpret_cast<char *>(&u), sizeof(float));
                    file.read(reinterpret_cast<char *>(&v), sizeof(float));
                    mesh.texcoords[(object_vertex_start + i) * 2U] = u;
                    mesh.texcoords[(object_vertex_start + i) * 2U + 1U] = 1.0F - v;
                }
            }
        }

        file.seekg(next);
    }
}

bool load_3ds(const std::string &path, MeshData &mesh) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    Chunk root;
    if (!read_chunk(file, root) || root.id != 0x4D4D) {
        return false;
    }

    std::string texture_filename;
    FaceColorAccum color_accum;
    std::unordered_map<std::string, MaterialInfo> materials;
    const std::filesystem::path model_path = std::filesystem::u8path(path);
    const std::filesystem::path model_dir =
        model_path.has_parent_path() ? model_path.parent_path() : std::filesystem::path();

    const std::streampos root_end = static_cast<std::streamoff>(root.length);
    while (file.good() && file.tellg() < root_end) {
        Chunk chunk;
        if (!read_chunk(file, chunk)) {
            break;
        }
        const std::streampos data_start = file.tellg();
        const std::streampos next = data_start + static_cast<std::streamoff>(chunk.length - 6U);

        if (chunk.id == 0x3D3D) {
            while (file.good() && file.tellg() < next) {
                Chunk sub;
                if (!read_chunk(file, sub)) {
                    break;
                }
                const std::streampos sub_data_start = file.tellg();
                const std::streampos sub_end =
                    sub_data_start + static_cast<std::streamoff>(sub.length - 6U);

                if (sub.id == 0x4000) {
                    char c = '\0';
                    do {
                        file.read(&c, 1);
                    } while (file.good() && c != '\0' && file.tellg() < sub_end);

                    while (file.good() && file.tellg() < sub_end) {
                        Chunk mesh_chunk;
                        if (!read_chunk(file, mesh_chunk)) {
                            break;
                        }
                        const std::streampos mesh_data_start = file.tellg();
                        const std::streampos mesh_end =
                            mesh_data_start + static_cast<std::streamoff>(mesh_chunk.length - 6U);

                        if (mesh_chunk.id == 0x4100) {
                            const size_t vertex_base = mesh.vertices.size() / 3;
                            parse_mesh_chunk(file, mesh_end, mesh, vertex_base,
                                             materials, color_accum,
                                             texture_filename);
                        }
                        file.seekg(mesh_end);
                    }
                } else if (sub.id == 0xAFFF) {
                    parse_material_block(file, sub_end, materials, texture_filename);
                }
                file.seekg(sub_end);
            }
        }
        file.seekg(next);
    }

    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return false;
    }

    const size_t vertex_count = mesh.vertices.size() / 3U;
    if (!mesh.texcoords.empty()) {
        const size_t expected_size = vertex_count * 2U;
        if (mesh.texcoords.size() < expected_size) {
            mesh.texcoords.resize(expected_size, 0.0F);
        } else if (mesh.texcoords.size() > expected_size) {
            mesh.texcoords.resize(expected_size);
        }
    }

    if (!texture_filename.empty()) {
        std::filesystem::path resolved_texture_path;
        if (resolve_texture_path(model_dir, texture_filename, resolved_texture_path)) {
            mesh.texture_path = resolved_texture_path.u8string();
        }
    }

    if (color_accum.weight > 0.0) {
        mesh.material_base_color = {
            static_cast<float>(color_accum.r / color_accum.weight),
            static_cast<float>(color_accum.g / color_accum.weight),
            static_cast<float>(color_accum.b / color_accum.weight),
        };
        mesh.has_material_base_color = true;
    }

    ensure_godot_clockwise_winding(mesh);
    compute_normals(mesh);
    return true;
}

} // namespace

namespace peraviz {

bool load_3ds_mesh_data(const godot::String &path,
                        godot::PackedVector3Array &out_vertices,
                        godot::PackedVector3Array &out_normals,
                        godot::PackedVector2Array &out_texcoords,
                        godot::PackedInt32Array &out_indices,
                        godot::String &out_texture_path,
                        bool &out_has_material_base_color,
                        godot::Vector3 &out_material_base_color,
                        godot::String &out_error) {
    MeshData mesh;
    const std::string utf8_path(path.utf8().get_data());
    if (!load_3ds(utf8_path, mesh)) {
        out_error = godot::String("Failed to parse 3DS mesh");
        return false;
    }

    out_vertices.resize(static_cast<int64_t>(mesh.vertices.size() / 3));
    for (int64_t i = 0; i < out_vertices.size(); ++i) {
        const std::array<float, 3> source_vertex = {
            mesh.vertices[i * 3] * kMillimetersToMeters,
            mesh.vertices[i * 3 + 1] * kMillimetersToMeters,
            mesh.vertices[i * 3 + 2] * kMillimetersToMeters,
        };
        const auto mapped = coordinate_mapper::map_source_vector_to_godot(source_vertex);
        out_vertices.set(i, godot::Vector3(mapped[0], mapped[1], mapped[2]));
    }

    out_normals.resize(static_cast<int64_t>(mesh.normals.size() / 3));
    for (int64_t i = 0; i < out_normals.size(); ++i) {
        const std::array<float, 3> source_normal = {
            mesh.normals[i * 3],
            mesh.normals[i * 3 + 1],
            mesh.normals[i * 3 + 2],
        };
        const auto mapped = coordinate_mapper::map_source_vector_to_godot(source_normal);
        out_normals.set(i, godot::Vector3(mapped[0], mapped[1], mapped[2]));
    }

    out_indices.resize(static_cast<int64_t>(mesh.indices.size()));
    for (int64_t i = 0; i < out_indices.size(); ++i) {
        out_indices.set(i, static_cast<int32_t>(mesh.indices[i]));
    }

    out_texcoords.resize(static_cast<int64_t>(mesh.texcoords.size() / 2U));
    for (int64_t i = 0; i < out_texcoords.size(); ++i) {
        out_texcoords.set(i, godot::Vector2(mesh.texcoords[i * 2], mesh.texcoords[i * 2 + 1]));
    }

    out_texture_path = godot::String(mesh.texture_path.c_str());
    out_has_material_base_color = mesh.has_material_base_color;
    out_material_base_color = godot::Vector3(mesh.material_base_color[0],
                                             mesh.material_base_color[1],
                                             mesh.material_base_color[2]);

    return true;
}

} // namespace peraviz
