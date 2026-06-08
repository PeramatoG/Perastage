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
#include <map>
#include <string>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "trussloader.h"

namespace fs = std::filesystem;

namespace {

// Converts a filesystem path to a UTF-8 string for loader calls.
std::string ToUtf8String(const fs::path &path) {
  std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

// Writes a ZIP archive with the provided text entries.
bool WriteZipArchive(const fs::path &path, const std::map<std::string, std::string> &entries) {
  wxFileOutputStream output(path.string());
  if (!output.IsOk())
    return false;

  wxZipOutputStream zip(output);
  for (const auto &[name, content] : entries) {
    if (!zip.PutNextEntry(wxString::FromUTF8(name.c_str())))
      return false;
    zip.Write(content.data(), content.size());
  }
  return zip.Close();
}

// Returns a minimal GDTF description that references the main GLB model.
std::string MinimalGdtfDescription() {
  return R"xml(<?xml version="1.0" encoding="UTF-8"?>
<GDTF>
  <FixtureType Manufacturer="Perastage" Name="Test Truss">
    <Models>
      <Model Name="Main" File="main" Length="1.0" Width="0.5" Height="0.5"/>
    </Models>
  </FixtureType>
</GDTF>
)xml";
}

} // namespace

// Verifies truss loader extension validation and archive extraction safety.
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

  const fs::path validGdtfPath = tempRoot / "valid.gdtf";
  assert(WriteZipArchive(validGdtfPath,
                         {{"description.xml", MinimalGdtfDescription()},
                          {"models/gltf/main.glb", "glTF"}}));

  Truss validGdtf;
  assert(LoadTrussDefinition(ToUtf8String(validGdtfPath), validGdtf));
  assert(validGdtf.symbolFile.find("models/gltf/main.glb") != std::string::npos);

  const fs::path traversalGdtfPath = tempRoot / "traversal.gdtf";
  assert(WriteZipArchive(traversalGdtfPath,
                         {{"description.xml", MinimalGdtfDescription()},
                          {"../escaped.txt", "unsafe"},
                          {"models/gltf/main.glb", "glTF"}}));

  Truss traversalGdtf;
  assert(!LoadTrussDefinition(ToUtf8String(traversalGdtfPath), traversalGdtf));

  const fs::path absoluteGdtfPath = tempRoot / "absolute.gdtf";
  assert(WriteZipArchive(absoluteGdtfPath,
                         {{"description.xml", MinimalGdtfDescription()},
                          {"/tmp/perastage-unsafe-entry.txt", "unsafe"},
                          {"models/gltf/main.glb", "glTF"}}));

  Truss absoluteGdtf;
  assert(!LoadTrussDefinition(ToUtf8String(absoluteGdtfPath), absoluteGdtf));

  fs::remove_all(tempRoot, ec);
  return 0;
}
