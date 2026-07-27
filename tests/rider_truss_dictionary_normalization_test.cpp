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
#include <filesystem>
#include <string>
#include <vector>

#include <wx/init.h>

#include "configmanager.h"
#include "riderimporter.h"
#include "trussdictionary.h"
#include "trussloader.h"
#include "support/gdtf_test_fixture_builder.h"

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

// Verifies that a Rider truss resolves a real renderable geometry resource.
void AssertTrussSymbolResolved(const std::string &textVariant) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  assert(RiderImporter::ImportText(textVariant));

  const auto &scene = cfg.GetScene();
  assert(scene.trusses.size() == 1);
  const auto &truss = scene.trusses.begin()->second;
  assert(!truss.symbolFile.empty());
  assert(fs::exists(PathUtils::PathFromUtf8(truss.symbolFile)));
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
  assert(fs::exists(PathUtils::PathFromUtf8(truss.symbolFile)));
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempRoot =
      fs::temp_directory_path() / PathUtils::PathFromUtf8("perastage_truss_model_normalization");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot);

  const fs::path libraryRoot = tempRoot / PathUtils::PathFromUtf8("library");
  SetLibraryPathEnv(ToUtf8String(libraryRoot));

  const fs::path archivePath = tempRoot / PathUtils::PathFromUtf8("FK40Q.gdtf");
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("FK40Q", "Perastage",
                           tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WithModelResource("main")
      .WithArchiveEntry("models/gltf/main.glb", "glTF")
      .WithArchiveEntry("models/svg/main.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\"/>")
      .WriteArchive(archivePath);

  Truss geometryAndSvg;
  assert(LoadTrussGdtf(ToUtf8String(archivePath), geometryAndSvg));
  assert(geometryAndSvg.gdtfSpec == ToUtf8String(archivePath));
  assert(PathUtils::PathFromUtf8(geometryAndSvg.symbolFile).extension() == ".glb");
  assert(fs::exists(PathUtils::PathFromUtf8(geometryAndSvg.symbolFile)));

  const fs::path geometryOnlyPath = tempRoot / "geometry-only.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("main")
      .WithArchiveEntry("models/3ds/main.3ds", "3DS")
      .WriteArchive(geometryOnlyPath);
  Truss geometryOnly;
  assert(LoadTrussGdtf(ToUtf8String(geometryOnlyPath), geometryOnly));
  assert(PathUtils::PathFromUtf8(geometryOnly.symbolFile).extension() == ".3ds");

  const fs::path svgOnlyPath = tempRoot / "svg-only.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("main")
      .WithArchiveEntry("models/svg/main.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\"/>")
      .WriteArchive(svgOnlyPath);
  Truss svgOnly;
  assert(LoadTrussGdtf(ToUtf8String(svgOnlyPath), svgOnly));
  assert(svgOnly.gdtfSpec == ToUtf8String(svgOnlyPath));
  assert(svgOnly.symbolFile.empty());

  const fs::path metadataOnlyPath = tempRoot / "metadata-only.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("missing").WriteArchive(
      metadataOnlyPath);
  Truss metadataOnly;
  assert(LoadTrussGdtf(ToUtf8String(metadataOnlyPath), metadataOnly));
  assert(metadataOnly.symbolFile.empty());

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
  AssertTrussSymbolResolved("1 truss 40X40 PRO PLATA 3 m para lx1\n");

  TrussDictionary::Update("TRUSS BLACKBIRD 3M", ToUtf8String(archivePath));
  AssertTrussSymbolResolved("1 truss BLACKBIRD 3m para lx1\n");

  TrussDictionary::Save({});
  fs::remove_all(tempRoot, ec);
  return 0;
}
