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
#include <cassert>
#include "filesystem_path_utils.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "projectutils.h"
#include "trussdictionary.h"
#include "trussloader.h"

namespace fs = std::filesystem;

namespace {

// Restores the exact prior last-project file state when the test scope ends.
class LastProjectFileGuard {
public:
  // Captures the existing file state for later restoration.
  explicit LastProjectFileGuard(const fs::path &path) : path_(path) {
    std::error_code ec;
    existed_ = fs::exists(path_, ec) && !ec;
    if (!existed_)
      return;
    std::ifstream input(path_, std::ios::binary);
    previousBytes_.assign(std::istreambuf_iterator<char>(input),
                          std::istreambuf_iterator<char>());
  }

  // Restores the previous bytes or removes the test-created file.
  ~LastProjectFileGuard() {
    if (existed_) {
      std::ofstream output(path_, std::ios::binary | std::ios::trunc);
      output.write(previousBytes_.data(),
                   static_cast<std::streamsize>(previousBytes_.size()));
      return;
    }
    std::error_code ec;
    fs::remove(path_, ec);
  }

private:
  fs::path path_;
  bool existed_ = false;
  std::string previousBytes_;
};

// Converts a filesystem path to UTF-8 for string-based application APIs.
std::string ToUtf8String(const fs::path &path) {
  return PathUtils::PathToUtf8(path);
}

// Sets the writable library override for the current process.
void SetLibraryPathEnv(const std::string &value) {
#ifdef _WIN32
  _putenv_s("PERASTAGE_LIBRARY_PATH", value.c_str());
#else
  setenv("PERASTAGE_LIBRARY_PATH", value.c_str(), 1);
#endif
}

// Writes a minimal truss archive with a supported GLB model resource.
bool WriteArchive(const fs::path &archivePath) {
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  if (!output.IsOk())
    return false;

  wxZipOutputStream zip(output);
  if (!zip.IsOk())
    return false;

  static const char *kDescriptionXml =
      "<GDTF><FixtureType Manufacturer=\"Perastage\" Name=\"FK40Q\">"
      "<Models><Model Name=\"main\" File=\"main\" Length=\"3.0\" Width=\"0.3\" Height=\"0.3\"/>"
      "</Models></FixtureType></GDTF>";
  static const char *kGlb = "glTF";

  if (!zip.PutNextEntry("description.xml"))
    return false;
  zip.Write(kDescriptionXml, std::strlen(kDescriptionXml));

  if (!zip.PutNextEntry("models/gltf/main.glb"))
    return false;
  zip.Write(kGlb, std::strlen(kGlb));

  zip.Close();
  return output.IsOk();
}

// Reports whether text is structurally valid UTF-8.
bool IsValidUtf8(const std::string &text) {
  size_t index = 0;
  while (index < text.size()) {
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80) {
      ++index;
      continue;
    }
    size_t continuationCount = 0;
    unsigned int codePoint = 0;
    if (lead >= 0xC2 && lead <= 0xDF) {
      continuationCount = 1;
      codePoint = lead & 0x1F;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      continuationCount = 2;
      codePoint = lead & 0x0F;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      continuationCount = 3;
      codePoint = lead & 0x07;
    } else {
      return false;
    }
    if (index + continuationCount >= text.size())
      return false;
    for (size_t offset = 1; offset <= continuationCount; ++offset) {
      const unsigned char continuation =
          static_cast<unsigned char>(text[index + offset]);
      if ((continuation & 0xC0) != 0x80)
        return false;
      codePoint = (codePoint << 6) | (continuation & 0x3F);
    }
    if ((continuationCount == 2 && codePoint < 0x800) ||
        (continuationCount == 3 && codePoint < 0x10000) ||
        codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
      return false;
    index += continuationCount + 1;
  }
  return true;
}

// Encodes each original UTF-8 byte as its corresponding Latin-1 code point.
std::string EncodeUtf8BytesAsLatin1Mojibake(const std::string &text) {
  std::string mojibake;
  mojibake.reserve(text.size() * 2);
  for (const unsigned char byte : text) {
    if (byte < 0x80) {
      mojibake.push_back(static_cast<char>(byte));
      continue;
    }
    mojibake.push_back(static_cast<char>(0xC0 | (byte >> 6)));
    mojibake.push_back(static_cast<char>(0x80 | (byte & 0x3F)));
  }
  return mojibake;
}

// Reads the exact stored last-project bytes for diagnostics.
std::string ReadStoredBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Validates and reports one last-project load stage.
bool CheckLastProjectStage(const char *stage, const std::string &expected,
                           const std::optional<std::string> &actual,
                           const fs::path &storagePath) {
  if (actual && *actual == expected)
    return true;
  const std::string stored = ReadStoredBytes(storagePath);
  std::cerr << "TrussPathEncodingRegression: last-project " << stage
            << " stage failed; returned=" << (actual ? "yes" : "no")
            << "; expected='" << expected << "'";
  if (actual)
    std::cerr << "; actual='" << *actual << "'";
  std::cerr << "; stored_bytes=" << stored.size()
            << "; stored_valid_utf8=" << (IsValidUtf8(stored) ? "yes" : "no")
            << '\n';
  return false;
}

// Verifies last-project paths survive Unicode and legacy mojibake storage.
bool RunLastProjectPathCase(const fs::path &tempRoot) {
  const fs::path projectPath =
      tempRoot / PathUtils::PathFromUtf8("Escenaro_Metal_ViñaRock_1.pstg");
  std::ofstream project(projectPath, std::ios::binary);
  project << "placeholder";
  project.close();

  const std::string expectedPath = ToUtf8String(projectPath);
  const fs::path lastProjectPath =
      PathUtils::PathFromUtf8(ProjectUtils::GetLastProjectPathFile());
  LastProjectFileGuard restoreLastProject(lastProjectPath);

  if (!ProjectUtils::SaveLastProjectPath(expectedPath)) {
    std::cerr << "TrussPathEncodingRegression: canonical last-project save failed\n";
    return false;
  }
  const auto canonicalResult = ProjectUtils::LoadLastProjectPath();
  if (!CheckLastProjectStage("canonical", expectedPath, canonicalResult,
                             lastProjectPath))
    return false;

  const std::string mojibake = EncodeUtf8BytesAsLatin1Mojibake(expectedPath);
  const std::string transformedEnye = "\xC3\x83\xC2\xB1";
  if (mojibake == expectedPath || !IsValidUtf8(mojibake) ||
      mojibake.find(transformedEnye) == std::string::npos ||
      !fs::exists(projectPath)) {
    std::cerr << "TrussPathEncodingRegression: legacy fixture validation failed\n";
    return false;
  }

  std::ofstream legacyOut(lastProjectPath, std::ios::binary | std::ios::trunc);
  legacyOut << mojibake;
  legacyOut.close();
  const auto legacyResult = ProjectUtils::LoadLastProjectPath();
  return CheckLastProjectStage("legacy", expectedPath, legacyResult,
                               lastProjectPath);
}

// Exercises dictionary import and model extraction for one archive path.
void RunPathCase(const fs::path &sourceDir, const std::string &modelName,
                 const std::string &fileName) {
  fs::create_directories(sourceDir);
  const fs::path archivePath = sourceDir / PathUtils::PathFromUtf8(fileName);
  assert(WriteArchive(archivePath));

  TrussDictionary::Update(modelName, ToUtf8String(archivePath));
  const auto storedPath = TrussDictionary::Get(modelName);
  assert(storedPath.has_value());

  Truss truss;
  assert(LoadTrussDefinition(*storedPath, truss));
  assert(!truss.symbolFile.empty());
  assert(fs::exists(PathUtils::PathFromUtf8(truss.symbolFile)));
  assert(fs::path(PathUtils::PathFromUtf8(truss.symbolFile)).filename() == "main.glb");
  assert(truss.symbolFile != *storedPath);
}

} // namespace

// Runs the complete Unicode truss and last-project regression sequence.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempRoot =
      fs::temp_directory_path() / PathUtils::PathFromUtf8("perastage_truss_diccionario_ñ");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  const fs::path libraryRoot = tempRoot / PathUtils::PathFromUtf8("librería prueba");
  SetLibraryPathEnv(ToUtf8String(libraryRoot));

  RunPathCase(tempRoot / PathUtils::PathFromUtf8("origen ñ"), "FK40Q-Unicode",
              "Tramo_áéíóú.gdtf");
  RunPathCase(tempRoot / PathUtils::PathFromUtf8("source with spaces"), "FK40Q-Spaces",
              "FK40Q H-300.gdtf");
  const bool lastProjectPassed = RunLastProjectPathCase(tempRoot);

  TrussDictionary::Save({});
  fs::remove_all(tempRoot, ec);
  return lastProjectPassed ? 0 : 1;
}
