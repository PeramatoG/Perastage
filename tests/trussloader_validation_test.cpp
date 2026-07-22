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
#include "wx_path_utils.h"
#include "filesystem_path_utils.h"
#include "trussloader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

namespace {

// Converts a filesystem path to a UTF-8 string for loader calls.
std::string ToUtf8String(const fs::path &path) {
  std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

// Reports a failed condition without invoking a modal CRT assertion dialog.
bool Check(bool condition, const std::string &message) {
  if (condition)
    return true;
  std::cerr << "TrussLoaderValidation: " << message << '\n';
  return false;
}

// Writes a ZIP archive with the provided text entries.
bool WriteZipArchive(const fs::path &path, const std::map<std::string, std::string> &entries) {
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(path));
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

// Normalizes path separators for portable suffix checks.
std::string NormalizedSeparators(std::string value) {
  for (char &ch : value) {
    if (ch == '\\')
      ch = '/';
  }
  return value;
}

// Reports whether a path ends with the expected normalized relative suffix.
bool HasNormalizedSuffix(const std::string &path, const std::string &suffix) {
  const std::string normalizedPath = NormalizedSeparators(path);
  const std::string normalizedSuffix = NormalizedSeparators(suffix);
  if (normalizedPath.size() < normalizedSuffix.size())
    return false;
  const std::size_t start = normalizedPath.size() - normalizedSuffix.size();
  return normalizedPath.compare(start, normalizedSuffix.size(), normalizedSuffix) == 0 &&
         (start == 0 || normalizedPath[start - 1] == '/');
}

// Verifies raw archive-entry path safety independent of writer-side sanitization.
bool CheckArchiveEntrySecurity(const fs::path &tempRoot) {
  bool ok = true;
  const fs::path destination = tempRoot / "extract-root";
  std::error_code ec;
  fs::create_directories(destination, ec);
  const std::vector<std::pair<std::string, bool>> cases = {
      {"../escaped.txt", false},
      {"/absolute-posix.txt", false},
      {"C:/absolute-windows.txt", false},
      {"C:\\absolute-windows.txt", false},
      {"\\\\server\\share\\file.txt", false},
      {"models/gltf/main.glb", true},
  };
  for (const auto &[entry, expectedSafe] : cases) {
    const bool actualSafe = IsTrussArchiveEntryTargetSafeForTesting(entry, destination);
    ok &= Check(actualSafe == expectedSafe,
                "archive entry safety mismatch for '" + entry + "'.");
  }
  ok &= Check(!fs::exists(tempRoot / "escaped.txt"),
              "unsafe traversal fixture wrote outside the extraction root.");
  ok &= Check(!fs::exists(fs::path("/tmp/perastage-unsafe-entry.txt")),
              "unsafe absolute fixture wrote outside the extraction root.");
  return ok;
}

} // namespace

// Verifies truss loader extension validation and archive extraction safety.
int main() {
  bool ok = true;
  const fs::path tempRoot = fs::temp_directory_path() /
                            PathUtils::PathFromUtf8("perastage_trussloader_validation_test");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  const fs::path glbPath = tempRoot / "direct_model.GLB";
  {
    std::ofstream glb(glbPath, std::ios::binary);
    glb << "glTF";
  }

  Truss truss;
  ok &= Check(IsSupportedTrussDefinitionExtension(ToUtf8String(glbPath)),
              "GLB extension was not recognized.");
  ok &= Check(LoadTrussDefinition(ToUtf8String(glbPath), truss),
              "direct GLB truss definition did not load.");
  ok &= Check(truss.symbolFile == ToUtf8String(glbPath),
              "direct GLB symbol path changed unexpectedly.");
  ok &= Check(truss.modelFile == ToUtf8String(glbPath),
              "direct GLB model path changed unexpectedly.");
  ok &= Check(truss.lengthMm > 0.0f && truss.widthMm > 0.0f && truss.heightMm > 0.0f,
              "direct GLB dimensions were not populated.");

  Truss missingModel;
  ok &= Check(!LoadTrussDefinition(ToUtf8String(tempRoot / "missing.glb"), missingModel),
              "missing GLB unexpectedly loaded.");

  Truss unsupportedModel;
  ok &= Check(!IsSupportedTrussDefinitionExtension(ToUtf8String(tempRoot / "mesh.obj")),
              "OBJ extension was unexpectedly accepted.");
  ok &= Check(!LoadTrussDefinition(ToUtf8String(tempRoot / "mesh.obj"), unsupportedModel),
              "unsupported OBJ unexpectedly loaded.");

  ok &= Check(GetTrussDefinitionFileDialogWildcard().find("*.gdtf;*.gtruss;*.glb;*.3ds") != std::string::npos,
              "file dialog wildcard is missing supported truss extensions.");

  const fs::path validGdtfPath = tempRoot / "valid.gdtf";
  ok &= Check(WriteZipArchive(validGdtfPath,
                              {{"description.xml", MinimalGdtfDescription()},
                               {"models/gltf/main.glb", "glTF"}}),
              "valid GDTF test archive could not be written.");

  Truss validGdtf;
  ok &= Check(LoadTrussDefinition(ToUtf8String(validGdtfPath), validGdtf),
              "valid GDTF truss definition did not load.");
  ok &= Check(HasNormalizedSuffix(validGdtf.symbolFile, "models/gltf/main.glb"),
              "valid GDTF symbol path did not end with models/gltf/main.glb: " +
                  validGdtf.symbolFile);

  const fs::path traversalGdtfPath = tempRoot / "traversal.gdtf";
  ok &= Check(WriteZipArchive(traversalGdtfPath,
                              {{"description.xml", MinimalGdtfDescription()},
                               {"../escaped.txt", "unsafe"},
                               {"models/gltf/main.glb", "glTF"}}),
              "traversal GDTF test archive could not be written.");

  Truss traversalGdtf;
  ok &= Check(!LoadTrussDefinition(ToUtf8String(traversalGdtfPath), traversalGdtf),
              "traversal GDTF archive unexpectedly loaded.");
  ok &= Check(CheckArchiveEntrySecurity(tempRoot),
              "raw archive-entry security checks failed.");

  fs::remove_all(tempRoot, ec);
  return ok ? 0 : 1;
}
