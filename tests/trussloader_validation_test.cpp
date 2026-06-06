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
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "trussloader.h"

namespace fs = std::filesystem;

namespace {

// Converts a filesystem path to a UTF-8 string for loader calls.
std::string ToUtf8String(const fs::path &path) {
  std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

} // namespace

// Verifies truss loader extension validation and direct model defaults.
int main() {
  const fs::path tempRoot = fs::temp_directory_path() /
                            fs::u8path("perastage_trussloader_validation_test");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  const fs::path glbPath = tempRoot / "direct_model.GLB";
  {
    std::ofstream glb(glbPath, std::ios::binary);
    glb << "glTF";
  }

  Truss truss;
  assert(IsSupportedTrussDefinitionExtension(ToUtf8String(glbPath)));
  assert(LoadTrussDefinition(ToUtf8String(glbPath), truss));
  assert(truss.symbolFile == ToUtf8String(glbPath));
  assert(truss.modelFile == ToUtf8String(glbPath));
  assert(truss.lengthMm > 0.0f);
  assert(truss.widthMm > 0.0f);
  assert(truss.heightMm > 0.0f);

  Truss missingModel;
  assert(!LoadTrussDefinition(ToUtf8String(tempRoot / "missing.glb"),
                              missingModel));

  Truss unsupportedModel;
  assert(!IsSupportedTrussDefinitionExtension(
      ToUtf8String(tempRoot / "mesh.obj")));
  assert(!LoadTrussDefinition(ToUtf8String(tempRoot / "mesh.obj"),
                              unsupportedModel));

  assert(GetTrussDefinitionFileDialogWildcard().find(
             "*.gdtf;*.gtruss;*.glb;*.3ds") != std::string::npos);

  fs::remove_all(tempRoot, ec);
  return 0;
}
