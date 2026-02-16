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
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include <wx/filename.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "trussdictionary.h"
#include "trussloader.h"

namespace fs = std::filesystem;

namespace {

std::string ToUtf8String(const fs::path &path) {
  std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

void SetLibraryPathEnv(const std::string &value) {
#ifdef _WIN32
  _putenv_s("PERASTAGE_LIBRARY_PATH", value.c_str());
#else
  setenv("PERASTAGE_LIBRARY_PATH", value.c_str(), 1);
#endif
}

bool WriteArchive(const fs::path &archivePath) {
  wxFileName::Mkdir(archivePath.parent_path().string(), wxS_DIR_DEFAULT,
                    wxPATH_MKDIR_FULL);

  wxFileOutputStream output(archivePath.string());
  if (!output.IsOk())
    return false;

  wxZipOutputStream zip(output);
  if (!zip.IsOk())
    return false;

  static const char *kDescriptionXml =
      "<GDTF><FixtureType Manufacturer=\"Perastage\" Name=\"FK40Q\">"
      "<Models><Model Name=\"main\" File=\"main\" Length=\"3.0\" Width=\"0.3\" Height=\"0.3\"/>"
      "</Models></FixtureType></GDTF>";
  static const char *kSvg =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"10\"></svg>";

  if (!zip.PutNextEntry("description.xml"))
    return false;
  zip.Write(kDescriptionXml, std::strlen(kDescriptionXml));

  if (!zip.PutNextEntry("models/svg/main.svg"))
    return false;
  zip.Write(kSvg, std::strlen(kSvg));

  zip.Close();
  return output.IsOk();
}

void RunPathCase(const fs::path &sourceDir, const std::string &modelName,
                 const std::string &fileName) {
  fs::create_directories(sourceDir);
  const fs::path archivePath = sourceDir / fs::u8path(fileName);
  assert(WriteArchive(archivePath));

  TrussDictionary::Update(modelName, ToUtf8String(archivePath));
  const auto storedPath = TrussDictionary::Get(modelName);
  assert(storedPath.has_value());

  Truss truss;
  assert(LoadTrussDefinition(*storedPath, truss));
  assert(!truss.symbolFile.empty());
  assert(fs::exists(fs::u8path(truss.symbolFile)));
  assert(fs::path(fs::u8path(truss.symbolFile)).filename() == "main.svg");
  assert(truss.symbolFile != *storedPath);
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempRoot =
      fs::temp_directory_path() / fs::u8path("perastage_truss_diccionario_ñ");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  const fs::path libraryRoot = tempRoot / fs::u8path("librería prueba");
  SetLibraryPathEnv(ToUtf8String(libraryRoot));

  RunPathCase(tempRoot / fs::u8path("origen ñ"), "FK40Q-Unicode",
              "Tramo_áéíóú.gdtf");
  RunPathCase(tempRoot / fs::u8path("source with spaces"), "FK40Q-Spaces",
              "FK40Q H-300.gdtf");

  TrussDictionary::Save({});
  fs::remove_all(tempRoot, ec);
  return 0;
}
