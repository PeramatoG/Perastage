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
#include <cmath>
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

// Reports whether UTF-8 text and a native path identify the same filesystem object.
bool SameFilesystemIdentity(const std::string &left, const fs::path &right) {
  if (left.empty() || right.empty())
    return false;
  const fs::path leftPath = PathUtils::PathFromUtf8(left);
  std::error_code ec;
  if (fs::exists(leftPath, ec) && !ec && fs::exists(right, ec) && !ec &&
      fs::equivalent(leftPath, right, ec) && !ec)
    return true;
  return PathUtils::BuildFilesystemIdentityKey(leftPath) ==
         PathUtils::BuildFilesystemIdentityKey(right);
}

// Reports whether a path is a component-safe descendant of a directory.
bool IsInsideDirectory(const fs::path &path, const fs::path &directory) {
  const std::string pathKey = PathUtils::BuildFilesystemIdentityKey(path);
  std::string directoryKey = PathUtils::BuildFilesystemIdentityKey(directory);
  if (pathKey.empty() || directoryKey.empty())
    return false;
  if (!directoryKey.empty() && directoryKey.back() != '/')
    directoryKey.push_back('/');
  return pathKey.rfind(directoryKey, 0) == 0;
}

// Retrieves and validates the managed archive owned by a dictionary key.
fs::path GetManagedArchive(const std::string &key, const fs::path &sourceArchive,
                           const fs::path &managedTrussDirectory) {
  const auto stored = TrussDictionary::Get(key);
  assert(stored.has_value());
  const fs::path managedArchive = PathUtils::PathFromUtf8(*stored);
  assert(fs::exists(managedArchive));
  assert(managedArchive.extension() == ".gdtf");
  assert(IsInsideDirectory(managedArchive, managedTrussDirectory));
  assert(!SameFilesystemIdentity(*stored, sourceArchive));
  return managedArchive;
}

// Verifies dictionary-backed truss fields identify the managed archive only.
void AssertManagedArchiveIdentity(const Truss &truss,
                                  const fs::path &expectedManagedArchive,
                                  const fs::path &sourceArchive) {
  assert(SameFilesystemIdentity(truss.modelFile, expectedManagedArchive));
  assert(SameFilesystemIdentity(truss.gdtfSpec, expectedManagedArchive));
  assert(SameFilesystemIdentity(truss.modelFile,
                                PathUtils::PathFromUtf8(truss.gdtfSpec)));
  assert(fs::exists(PathUtils::PathFromUtf8(truss.modelFile)));
  assert(PathUtils::PathFromUtf8(truss.modelFile).extension() == ".gdtf");
  assert(!SameFilesystemIdentity(truss.modelFile, sourceArchive));
  assert(!SameFilesystemIdentity(truss.gdtfSpec, sourceArchive));
}

// Verifies Rider resolution of a supported 3D geometry resource path and metadata.
void AssertTrussSymbolResolved(const std::string &textVariant,
                               const fs::path &expectedManagedArchive,
                               const fs::path &sourceArchive) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  assert(RiderImporter::ImportText(textVariant));

  const auto &scene = cfg.GetScene();
  assert(scene.trusses.size() == 1);
  const auto &truss = scene.trusses.begin()->second;
  assert(!truss.symbolFile.empty());
  const fs::path symbolPath = PathUtils::PathFromUtf8(truss.symbolFile);
  assert(symbolPath.extension() == ".glb" || symbolPath.extension() == ".3ds");
  assert(fs::exists(symbolPath));
  AssertManagedArchiveIdentity(truss, expectedManagedArchive, sourceArchive);
  assert(truss.model == "FK40Q");
  assert(truss.manufacturer == "Perastage");
  assert(std::abs(truss.lengthMm - 3000.0f) < 0.001f);
  assert(std::abs(truss.widthMm - 400.0f) < 0.001f);
  assert(std::abs(truss.heightMm - 400.0f) < 0.001f);
}

// Verifies model-token lookup against the principal truss archive.
void AssertModelTokenDictionaryLookupWorks(const std::string &textVariant,
                                           const fs::path &expectedManagedArchive,
                                           const fs::path &sourceArchive) {
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
  AssertManagedArchiveIdentity(truss, expectedManagedArchive, sourceArchive);
  assert(std::abs(truss.lengthMm - 3000.0f) < 0.001f);
  assert(std::abs(truss.widthMm - 400.0f) < 0.001f);
  assert(std::abs(truss.heightMm - 400.0f) < 0.001f);
  assert(truss.manufacturer == "Perastage");
}

// Verifies that SVG-only Rider resources retain metadata and use dummy geometry.
void AssertSvgOnlyResourceUsesDummy(const fs::path &expectedManagedArchive,
                                    const fs::path &sourceArchive) {
  auto &cfg = ConfigManager::Get();
  cfg.Reset();
  assert(RiderImporter::ImportText("1 truss SVGONLY 3m para lx1\n"));

  const auto &scene = cfg.GetScene();
  assert(scene.trusses.size() == 1);
  const Truss &truss = scene.trusses.begin()->second;
  AssertManagedArchiveIdentity(truss, expectedManagedArchive, sourceArchive);
  assert(truss.symbolFile.empty());
  assert(truss.model == "SVGONLY");
  assert(truss.manufacturer == "Perastage");
  assert(std::abs(truss.lengthMm - 3000.0f) < 0.001f);
  assert(std::abs(truss.widthMm - 400.0f) < 0.001f);
  assert(std::abs(truss.heightMm - 400.0f) < 0.001f);
  assert(std::abs(truss.transform.o[0] - (-1500.0f)) < 0.001f);
}

} // namespace

int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());

  const fs::path tempRoot =
      fs::temp_directory_path() / PathUtils::PathFromUtf8("perastage_truss_model_normalization");
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  assert(!ec);
  ec.clear();
  const bool createdTempRoot = fs::create_directories(tempRoot, ec);
  assert(createdTempRoot && !ec);
  assert(fs::exists(tempRoot) && fs::is_directory(tempRoot));

  const fs::path libraryRoot = tempRoot / PathUtils::PathFromUtf8("library");
  ec.clear();
  const bool createdLibraryRoot = fs::create_directories(libraryRoot, ec);
  assert(createdLibraryRoot && !ec);
  assert(fs::exists(libraryRoot) && fs::is_directory(libraryRoot));
  SetLibraryPathEnv(ToUtf8String(libraryRoot));

  auto &cfg = ConfigManager::Get();
  cfg.RemoveKey("trusses_dictionary_active_path");
  const fs::path activeDictionaryPath = PathUtils::PathFromUtf8(
      TrussDictionary::GetActiveDictionaryFilePath());
  assert(!activeDictionaryPath.empty());
  assert(activeDictionaryPath.filename() == "truss_dictionary.json");
  const fs::path managedTrussDirectory = activeDictionaryPath.parent_path();
  assert(fs::exists(managedTrussDirectory));
  assert(fs::is_directory(managedTrussDirectory));
  assert(IsInsideDirectory(activeDictionaryPath, tempRoot));
  assert(IsInsideDirectory(managedTrussDirectory, tempRoot));
  assert(SameFilesystemIdentity(ToUtf8String(managedTrussDirectory),
                                libraryRoot / "trusses"));
  assert(IsInsideDirectory(managedTrussDirectory / "asset.gdtf",
                           managedTrussDirectory));
  assert(!IsInsideDirectory(managedTrussDirectory.parent_path() / "trusses-other" /
                                "asset.gdtf",
                            managedTrussDirectory));
  TrussDictionary::Save({});
  assert(fs::exists(activeDictionaryPath));
  assert(IsInsideDirectory(activeDictionaryPath, tempRoot));

  const fs::path archivePath = tempRoot / PathUtils::PathFromUtf8("FK40Q.gdtf");
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("FK40Q", "Perastage",
                           tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WithModelResource("main")
      .WithModelDimensionsMeters(3.0f, 0.4f, 0.4f)
      .WithArchiveEntry("models/gltf/main.glb", tests::gdtf::BuildMinimalValidGlb())
      .WithArchiveEntry("models/svg/main.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\"/>")
      .WriteArchive(archivePath);
  assert(!IsInsideDirectory(archivePath, managedTrussDirectory));

  Truss geometryAndSvg;
  assert(LoadTrussGdtf(ToUtf8String(archivePath), geometryAndSvg));
  assert(geometryAndSvg.gdtfSpec == ToUtf8String(archivePath));
  assert(PathUtils::PathFromUtf8(geometryAndSvg.symbolFile).extension() == ".glb");
  assert(fs::exists(PathUtils::PathFromUtf8(geometryAndSvg.symbolFile)));

  const fs::path geometryOnlyPath = tempRoot / "geometry-only.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("main")
      .WithModelDimensionsMeters(3.0f, 0.4f, 0.4f)
      .WithArchiveEntry("models/3ds/main.3ds", "3DS")
      .WriteArchive(geometryOnlyPath);
  assert(!IsInsideDirectory(geometryOnlyPath, managedTrussDirectory));
  Truss geometryOnly;
  assert(LoadTrussGdtf(ToUtf8String(geometryOnlyPath), geometryOnly));
  assert(PathUtils::PathFromUtf8(geometryOnly.symbolFile).extension() == ".3ds");

  const fs::path svgOnlyPath = tempRoot / "svg-only.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("SVGONLY", "Perastage",
                           tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId)
      .WithModelResource("main")
      .WithModelDimensionsMeters(3.0f, 0.4f, 0.4f)
      .WithArchiveEntry("models/svg/main.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\"/>")
      .WriteArchive(svgOnlyPath);
  assert(!IsInsideDirectory(svgOnlyPath, managedTrussDirectory));
  Truss svgOnly;
  assert(LoadTrussGdtf(ToUtf8String(svgOnlyPath), svgOnly));
  assert(svgOnly.gdtfSpec == ToUtf8String(svgOnlyPath));
  assert(svgOnly.symbolFile.empty());
  TrussDictionary::Update("SVGONLY", ToUtf8String(svgOnlyPath));
  const fs::path managedSvgOnly =
      GetManagedArchive("SVGONLY", svgOnlyPath, managedTrussDirectory);
  AssertSvgOnlyResourceUsesDummy(managedSvgOnly, svgOnlyPath);

  const fs::path metadataOnlyPath = tempRoot / "metadata-only.gdtf";
  tests::gdtf::BuildMinimalValidFixture().WithModelResource("missing").WriteArchive(
      metadataOnlyPath);
  assert(!IsInsideDirectory(metadataOnlyPath, managedTrussDirectory));
  Truss metadataOnly;
  assert(LoadTrussGdtf(ToUtf8String(metadataOnlyPath), metadataOnly));
  assert(metadataOnly.symbolFile.empty());

  TrussDictionary::Update("TRUSS 40X40 3M", ToUtf8String(archivePath));
  const auto canonical = TrussDictionary::Get("TRUSS 40X40 3M");
  assert(canonical.has_value());
  const fs::path managedCanonical =
      GetManagedArchive("TRUSS 40X40 3M", archivePath, managedTrussDirectory);

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

  AssertTrussSymbolResolved("1 truss 40X40 3M\n", managedCanonical, archivePath);
  AssertTrussSymbolResolved("1 truss 40x40 3m\n", managedCanonical, archivePath);
  AssertTrussSymbolResolved("1 truss   40X40   3M\n", managedCanonical, archivePath);

  TrussDictionary::Update("FK40Q", ToUtf8String(archivePath));
  const fs::path managedModelToken =
      GetManagedArchive("FK40Q", archivePath, managedTrussDirectory);
  AssertModelTokenDictionaryLookupWorks("1 truss FK40Q 3 m para lx1\n",
                                        managedModelToken, archivePath);
  AssertModelTokenDictionaryLookupWorks("1 truss fk40q 3 m para lx1\n",
                                        managedModelToken, archivePath);

  TrussDictionary::Update("TRUSS 40X40 PRO 3M", ToUtf8String(archivePath));
  const fs::path managedFinish =
      GetManagedArchive("TRUSS 40X40 PRO 3M", archivePath, managedTrussDirectory);
  AssertTrussSymbolResolved("1 truss 40X40 PRO NEGRO 3m para lx1\n",
                            managedFinish, archivePath);
  AssertTrussSymbolResolved("1 truss 40X40 PRO BLACK 3m for lx1\n",
                            managedFinish, archivePath);
  AssertTrussSymbolResolved("1 truss 40X40 PRO PLATA 3 m para lx1\n",
                            managedFinish, archivePath);

  TrussDictionary::Update("TRUSS BLACKBIRD 3M", ToUtf8String(archivePath));
  const fs::path managedSubstring =
      GetManagedArchive("TRUSS BLACKBIRD 3M", archivePath, managedTrussDirectory);
  AssertTrussSymbolResolved("1 truss BLACKBIRD 3m para lx1\n", managedSubstring,
                            archivePath);

  TrussDictionary::Save({});
  assert(IsInsideDirectory(activeDictionaryPath, tempRoot));
  ec.clear();
  fs::remove_all(tempRoot, ec);
  assert(!ec);
  assert(!fs::exists(tempRoot));
  return 0;
}
