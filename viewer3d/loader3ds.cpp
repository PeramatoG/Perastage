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
#include "consolepanel.h"
#include <wx/wx.h>

constexpr bool kLog3dsMessages = false;

struct Chunk {
    uint16_t id;
    uint32_t length;
};

static bool readChunk(std::ifstream& file, Chunk& c)
{
    if(!file.read(reinterpret_cast<char*>(&c.id), 2)) return false;
    if(!file.read(reinterpret_cast<char*>(&c.length), 4)) return false;
    return true;
}

// Parses a single TRIANGULAR MESH (0x4100) chunk.
// vertexBase is the current number of vertices already stored in the mesh
// so we can offset face indices when concatenating multiple objects.
static void parseMesh(std::ifstream& file, long endPos, Mesh& mesh, size_t vertexBase)
{
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
                            parseMesh(file, me, outMesh, base);
                        } else {
                            file.seekg(me);
                        }
                    }
                } else {
                    file.seekg(se);
                }
            }
        } else {
            file.seekg(next);
        }
    }

    bool ok = !outMesh.vertices.empty() && !outMesh.indices.empty();
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
