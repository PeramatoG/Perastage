#pragma once

#include <filesystem>
#include <string>

namespace tests::gdtf {

class FixtureBuilder {
public:
  static constexpr const char *kMinimalFixtureTypeId =
      "12345678-1234-4234-9234-123456789abc";
  FixtureBuilder();
  FixtureBuilder &WithDmxMode(std::string name, std::string geometry);
  FixtureBuilder &WithFixtureCategorySignals();
  FixtureBuilder &WithWheelMedia(std::string wheelName, std::string slotName);
  FixtureBuilder &WithModelResource(std::string modelName, std::string resourcePath);
  std::string BuildDescriptionXml() const;
  void WriteArchive(const std::filesystem::path &archivePath) const;
private:
  std::string modeName;
  std::string modeGeometry;
  bool categorySignals = false;
};
FixtureBuilder BuildMinimalValidFixture();
void WriteMissingMandatorySectionsArchive(const std::filesystem::path &archivePath);
void WriteInvalidGuidArchive(const std::filesystem::path &archivePath);
void WriteMalformedXmlArchive(const std::filesystem::path &archivePath);
} // namespace tests::gdtf
