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

#include "loader_obj.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Converts OBJ index tokens into absolute zero-based indexes while supporting negative OBJ indexes.
bool ParseObjIndex(const std::string &token, size_t valueCount, int &outIndex) {
  if (token.empty())
    return false;
  int rawIndex = 0;
  try {
    rawIndex = std::stoi(token);
  } catch (...) {
    return false;
  }

  if (rawIndex > 0)
    outIndex = rawIndex - 1;
  else if (rawIndex < 0)
    outIndex = static_cast<int>(valueCount) + rawIndex;
  else
    return false;

  return outIndex >= 0 && static_cast<size_t>(outIndex) < valueCount;
}

} // namespace

// Loads OBJ geometry, preserves provided normals/UVs when valid, and computes fallback normals when missing.
bool LoadOBJ(const std::string &path, Mesh &outMesh, std::string *errorMessage) {
  auto setError = [&](const std::string &message) {
    if (errorMessage)
      *errorMessage = message;
    return false;
  };

  std::ifstream file(path);
  if (!file.is_open())
    return setError("Cannot open OBJ file");

  outMesh = Mesh{};
  outMesh.hasMaterialBaseColor = true;
  outMesh.materialBaseColor = {0.85f, 0.85f, 0.85f};

  std::vector<float> normals;
  std::vector<float> texcoords;

  std::string line;
  size_t lineNumber = 0;
  while (std::getline(file, line)) {
    ++lineNumber;
    if (line.empty() || line[0] == '#')
      continue;

    std::istringstream ss(line);
    std::string keyword;
    ss >> keyword;
    if (keyword == "v") {
      float x = 0.0f, y = 0.0f, z = 0.0f;
      if (!(ss >> x >> y >> z))
        return setError("Invalid vertex at line " + std::to_string(lineNumber));
      outMesh.vertices.push_back(x * 1000.0f);
      outMesh.vertices.push_back(y * 1000.0f);
      outMesh.vertices.push_back(z * 1000.0f);
    } else if (keyword == "vn") {
      float nx = 0.0f, ny = 0.0f, nz = 0.0f;
      if (!(ss >> nx >> ny >> nz))
        return setError("Invalid normal at line " + std::to_string(lineNumber));
      normals.push_back(nx);
      normals.push_back(ny);
      normals.push_back(nz);
    } else if (keyword == "vt") {
      float u = 0.0f, v = 0.0f;
      if (!(ss >> u >> v))
        return setError("Invalid UV at line " + std::to_string(lineNumber));
      texcoords.push_back(u);
      texcoords.push_back(v);
    } else if (keyword == "f") {
      std::vector<uint32_t> polygon;
      std::string vertexToken;
      while (ss >> vertexToken) {
        const size_t firstSlash = vertexToken.find('/');
        const std::string vertexIndexToken = vertexToken.substr(0, firstSlash);
        int vertexIndex = -1;
        if (!ParseObjIndex(vertexIndexToken, outMesh.vertices.size() / 3, vertexIndex))
          return setError("Invalid face vertex index at line " + std::to_string(lineNumber));
        polygon.push_back(static_cast<uint32_t>(vertexIndex));
      }

      if (polygon.size() < 3)
        return setError("Face with less than 3 vertices at line " + std::to_string(lineNumber));

      for (size_t i = 1; i + 1 < polygon.size(); ++i) {
        outMesh.indices.push_back(polygon[0]);
        outMesh.indices.push_back(polygon[i]);
        outMesh.indices.push_back(polygon[i + 1]);
      }
    }
  }

  if (outMesh.vertices.empty() || outMesh.indices.empty())
    return setError("OBJ file has no usable mesh geometry");

  if (!normals.empty() && normals.size() == outMesh.vertices.size())
    outMesh.normals = normals;
  else
    ComputeNormals(outMesh);

  if (!texcoords.empty() && texcoords.size() == (outMesh.vertices.size() / 3) * 2)
    outMesh.texcoords = texcoords;
  else
    outMesh.texcoords.clear();

  return true;
}

// Returns true because OBJ loading is compiled into this build.
bool IsObjLoaderAvailable() { return true; }
