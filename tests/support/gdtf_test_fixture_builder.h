#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace tests::gdtf {

class FixtureBuilder {
public:
  static constexpr const char *kMinimalFixtureTypeId =
      "12345678-1234-4234-9234-123456789abc";
  FixtureBuilder();
  FixtureBuilder &WithFixtureIdentity(std::string name, std::string manufacturer,
                                      std::string fixtureTypeId);
  FixtureBuilder &WithDmxMode(std::string name, std::string geometry);
  FixtureBuilder &WithModelResource(std::string fileBase);
  FixtureBuilder &WithModelDimensionsMeters(float length, float width, float height);
  FixtureBuilder &WithFixtureCategorySignals();
  FixtureBuilder &WithPerastageGeneratedSymbols();
  FixtureBuilder &WithArchiveEntry(std::string path, std::string bytes);
  std::string BuildDescriptionXml() const;
  void WriteArchive(const std::filesystem::path &archivePath) const;
private:
  std::string modeName;
  std::string modeGeometry;
  std::string fixtureName;
  std::string manufacturer;
  std::string fixtureTypeId;
  std::string modelFileBase;
  float modelLengthMeters = 0.1f;
  float modelWidthMeters = 0.1f;
  float modelHeightMeters = 0.1f;
  bool categorySignals = false;
  bool perastageGeneratedSymbols = false;
  std::vector<std::pair<std::string, std::string>> archiveEntries;
};
FixtureBuilder BuildMinimalValidFixture();
std::string BuildMinimalValidGlb();
void WriteMissingMandatorySectionsArchive(const std::filesystem::path &archivePath);
void WriteInvalidGuidArchive(const std::filesystem::path &archivePath);
void WriteMalformedXmlArchive(const std::filesystem::path &archivePath);
bool IsPortableArchiveEntryPathForTesting(const std::string &path);
void WriteArchiveEntryForTesting(const std::filesystem::path &archivePath,
                                 const std::string &entryPath);
} // namespace tests::gdtf
