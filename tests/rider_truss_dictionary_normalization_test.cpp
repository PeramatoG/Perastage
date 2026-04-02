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
#include <string>
#include <vector>

#include <wx/filename.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "riderimporter.h"
#include "trussdictionary.h"

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

void AssertTrussSymbolResolved(const std::string &textVariant) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  assert(RiderImporter::ImportText(textVariant));

  const auto &scene = cfg.GetScene();
  assert(scene.trusses.size() == 1);
  const auto &truss = scene.trusses.begin()->second;
  assert(!truss.symbolFile.empty());
  assert(fs::exists(fs::u8path(truss.symbolFile)));
}

void AssertModelTokenDictionaryLookupWorks(const std::string &textVariant) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  assert(RiderImporter::ImportText(textVariant));

  const auto &scene = cfg.GetScene();
  assert(scene.trusses.size() == 1);
  const auto &truss = scene.trusses.begin()->second;
  assert(!truss.model.empty());
  assert(truss.model == TrussDictionary::NormalizeModelKey("FK40Q"));
  assert(!truss.symbolFile.empty());
  assert(fs::exists(fs::u8path(truss.symbolFile)));
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempRoot =
      fs::temp_directory_path() / fs::u8path("perastage_truss_model_normalization");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  const fs::path libraryRoot = tempRoot / fs::u8path("library");
  SetLibraryPathEnv(ToUtf8String(libraryRoot));

  const fs::path archivePath = tempRoot / fs::u8path("FK40Q.gdtf");
  assert(WriteArchive(archivePath));

  TrussDictionary::Update("TRUSS 40X40 3M", ToUtf8String(archivePath));
  const auto canonical = TrussDictionary::Get("TRUSS 40X40 3M");
  assert(canonical.has_value());

  const std::vector<std::string> variants = {
      "TRUSS 40X40 3M",
      "truss 40x40 3m",
      "TRUSS   40X40   3M",
  };
  for (const std::string &variant : variants) {
    const auto resolved = TrussDictionary::Get(variant);
    assert(resolved.has_value());
    assert(*resolved == *canonical);
  }

  AssertTrussSymbolResolved("1 truss 40X40 3M\n");
  AssertTrussSymbolResolved("1 truss 40x40 3m\n");
  AssertTrussSymbolResolved("1 truss   40X40   3M\n");

  TrussDictionary::Update("FK40Q", ToUtf8String(archivePath));
  AssertModelTokenDictionaryLookupWorks("1 truss FK40Q 3 m para lx1\n");
  AssertModelTokenDictionaryLookupWorks("1 truss fk40q 3 m para lx1\n");

  TrussDictionary::Update("TRUSS 40X40 PRO 3M", ToUtf8String(archivePath));
  AssertTrussSymbolResolved("1 truss 40X40 PRO NEGRO 3m para lx1\n");
  AssertTrussSymbolResolved("1 truss 40X40 PRO BLACK 3m for lx1\n");

  TrussDictionary::Save({});
  fs::remove_all(tempRoot, ec);
  return 0;
}
