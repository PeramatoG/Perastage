#include "mesh_3ds_loader.h"

#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct Chunk {
    uint16_t id = 0;
    uint32_t length = 0;
};

struct MeshData {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::vector<float> normals;
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

        const float nx = uy * vz - uz * vy;
        const float ny = uz * vx - ux * vz;
        const float nz = ux * vy - uy * vx;

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

void parse_mesh_chunk(std::ifstream &file, std::streampos mesh_end, MeshData &mesh,
                      size_t vertex_base) {
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
                            parse_mesh_chunk(file, mesh_end, mesh, vertex_base);
                        }
                        file.seekg(mesh_end);
                    }
                }
                file.seekg(sub_end);
            }
        }
        file.seekg(next);
    }

    if (mesh.vertices.empty() || mesh.indices.empty()) {
        return false;
    }

    compute_normals(mesh);
    return true;
}

} // namespace

namespace peraviz {

godot::Dictionary load_3ds_mesh_data(const godot::String &path) {
    godot::Dictionary result;
    result[godot::String("ok")] = false;

    MeshData mesh;
    const std::string utf8_path(path.utf8().get_data());
    if (!load_3ds(utf8_path, mesh)) {
        result[godot::String("error")] = godot::String("Failed to parse 3DS mesh");
        return result;
    }

    godot::PackedVector3Array vertices;
    vertices.resize(static_cast<int64_t>(mesh.vertices.size() / 3));
    for (int64_t i = 0; i < vertices.size(); ++i) {
        vertices.set(i, godot::Vector3(mesh.vertices[i * 3], mesh.vertices[i * 3 + 1],
                                       mesh.vertices[i * 3 + 2]));
    }

    godot::PackedVector3Array normals;
    normals.resize(static_cast<int64_t>(mesh.normals.size() / 3));
    for (int64_t i = 0; i < normals.size(); ++i) {
        normals.set(i, godot::Vector3(mesh.normals[i * 3], mesh.normals[i * 3 + 1],
                                      mesh.normals[i * 3 + 2]));
    }

    godot::PackedInt32Array indices;
    indices.resize(static_cast<int64_t>(mesh.indices.size()));
    for (int64_t i = 0; i < indices.size(); ++i) {
        indices.set(i, static_cast<int32_t>(mesh.indices[i]));
    }

    result[godot::String("ok")] = true;
    result[godot::String("vertices")] = vertices;
    result[godot::String("normals")] = normals;
    result[godot::String("indices")] = indices;
    return result;
}

} // namespace peraviz
