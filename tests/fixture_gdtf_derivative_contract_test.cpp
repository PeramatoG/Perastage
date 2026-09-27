#include "fixture_gdtf_derivative_contract.h"
#include "fixture_gdtf_derivative_publication.h"
#include "inspection/xml_schema_validation.h"

#include "gdtf_test_fixture_builder.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

// Reads an entire file for source-preservation assertions.
std::string ReadFileBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

// Reads description.xml from a synthetic GDTF archive.
std::string ReadDescriptionXml(const fs::path &path) {
  wxFileInputStream input(path.string());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->GetName() != "description.xml")
      continue;
    std::string bytes;
    char buffer[4096];
    while (true) {
      zip.Read(buffer, sizeof(buffer));
      const std::size_t count = zip.LastRead();
      if (count == 0)
        break;
      bytes.append(buffer, count);
    }
    return bytes;
  }
  return {};
}

// Reports whether a GDTF archive retains a requested resource entry.
bool ArchiveContainsEntry(const fs::path &path, const std::string &entryName) {
  wxFileInputStream input(path.string());
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->GetName().ToStdString() == entryName)
      return true;
  }
  return false;
}

// Counts canonicalization revisions in one description payload.
int CountCanonicalizationRevisions(const std::string &xml) {
  tinyxml2::XMLDocument document;
  assert(document.Parse(xml.c_str(), xml.size()) == tinyxml2::XML_SUCCESS);
  const auto *fixtureType =
      document.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
  const auto *revisions = fixtureType->FirstChildElement("Revisions");
  int count = 0;
  for (const auto *revision =
           revisions ? revisions->FirstChildElement("Revision") : nullptr;
       revision; revision = revision->NextSiblingElement("Revision")) {
    const char *text = revision->Attribute("Text");
    count += text && std::string(text) ==
                         "Canonicalized GDTF structure for Perastage export";
  }
  return count;
}

// Reads the FixtureTypeID from one description payload.
std::string ReadFixtureTypeId(const std::string &xml) {
  tinyxml2::XMLDocument document;
  assert(document.Parse(xml.c_str(), xml.size()) == tinyxml2::XML_SUCCESS);
  const auto *fixtureType =
      document.FirstChildElement("GDTF")->FirstChildElement("FixtureType");
  const char *id = fixtureType->Attribute("FixtureTypeID");
  return id ? id : "";
}

// Verifies canonical derivatives require all four stored fixture-symbol views.
int main() {
  const fs::path root = fs::temp_directory_path() /
                        "perastage-fixture-derivative-contract-test";
  fs::remove_all(root);
  fs::create_directories(root);
  const fs::path incomplete = root / "Incomplete@Perastage.gdtf";
  const fs::path complete = root / "Complete@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("main")
      .WriteArchive(incomplete);
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("main")
      .WithPerastageGeneratedSymbols()
      .WriteArchive(complete);
  std::string error;
  assert(!fixture_gdtf::ValidatePublishedDerivative(incomplete.string(), error));
  assert(!error.empty());
  assert(fixture_gdtf::ValidatePublishedDerivative(complete.string(), error));
  assert(error.empty());

  const std::string validSvg =
      "<svg viewBox=\"0 0 10 10\"><polygon points=\"0,0 10,0 10,10\"/></svg>";
  const auto writeFourViews = [&](const fs::path &path,
                                  const std::string &frontSvg) {
    tests::gdtf::BuildMinimalValidFixture()
        .WithModelResource("main")
        .WithArchiveEntry("models/svg/main.svg", validSvg)
        .WithArchiveEntry("models/svg/main_bottom.svg", validSvg)
        .WithArchiveEntry("models/svg_front/main.svg", frontSvg)
        .WithArchiveEntry("models/svg_side/main.svg", validSvg)
        .WriteArchive(path);
  };
  const fs::path malformed = root / "Malformed.gdtf";
  const fs::path zeroViewBox = root / "ZeroViewBox.gdtf";
  const fs::path emptyGeometry = root / "EmptyGeometry.gdtf";
  writeFourViews(malformed, "<svg");
  writeFourViews(zeroViewBox,
                 "<svg viewBox=\"0 0 0 10\"><path d=\"M0 0 L1 1\"/></svg>");
  writeFourViews(emptyGeometry, "<svg viewBox=\"0 0 10 10\"/>");
  assert(!fixture_gdtf::ValidatePublishedDerivative(malformed.string(), error));
  assert(!fixture_gdtf::ValidatePublishedDerivative(zeroViewBox.string(), error));
  assert(!fixture_gdtf::ValidatePublishedDerivative(emptyGeometry.string(), error));

  const fs::path missingModel = root / "MissingModel.gdtf";
  tests::gdtf::WriteMissingMandatorySectionsArchive(missingModel);
  assert(!fixture_gdtf::ValidatePublishedDerivative(missingModel.string(), error));
  assert(error.find("Model") != std::string::npos);

  const fs::path project = root / "project";
  fs::create_directories(project / "fixtures");
  const fs::path published = project / "fixtures" / "Fixture@Perastage.gdtf";
  const std::string previousBytes = "previous-published-derivative";
  std::ofstream(published, std::ios::binary) << previousBytes;

  fixture_gdtf::PreparedDerivative failedPreparation;
  assert(fixture_gdtf::PrepareProjectDerivative(
      incomplete, project, published.filename(), failedPreparation, error));
  assert(fs::exists(failedPreparation.workingPath));
  assert(!fixture_gdtf::PublishPreparedDerivative(failedPreparation, error));
  assert(!error.empty());
  assert(!fs::exists(failedPreparation.workingPath));
  {
    std::ifstream previousInput(published, std::ios::binary);
    assert(std::string(std::istreambuf_iterator<char>(previousInput),
                       std::istreambuf_iterator<char>()) == previousBytes);
  }

  fixture_gdtf::PreparedDerivative successfulPreparation;
  assert(fixture_gdtf::PrepareProjectDerivative(
      complete, project, published.filename(), successfulPreparation, error));
  assert(successfulPreparation.publishedReference.find(".working") ==
         std::string::npos);
  assert(fixture_gdtf::PublishPreparedDerivative(successfulPreparation, error));
  assert(!fs::exists(successfulPreparation.workingPath));
  assert(fixture_gdtf::ValidatePublishedDerivative(published.string(), error));

  const fs::path legacySource = root / "LegacySource.gdtf";
  const fs::path canonicalDestination = root / "Legacy@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("main")
      .WithPerastageGeneratedSymbols()
      .WithEditor("PERASTAGE 1.5")
      .WithArchiveEntry("wheels/open.png", "wheel-resource")
      .WriteArchive(legacySource);
  const std::string originalLegacyBytes = ReadFileBytes(legacySource);
  assert(!fixture_gdtf::PublishCanonicalGdtfCopy(legacySource, legacySource,
                                                  error));
  assert(ReadFileBytes(legacySource) == originalLegacyBytes);
  assert(fixture_gdtf::PublishCanonicalGdtfCopy(
      legacySource, canonicalDestination, error));
  assert(ReadFileBytes(legacySource) == originalLegacyBytes);
  assert(legacySource != canonicalDestination);
  assert(ReadFileBytes(canonicalDestination) != originalLegacyBytes);
  std::string canonicalXml = ReadDescriptionXml(canonicalDestination);
  assert(ReadFixtureTypeId(canonicalXml) ==
         tests::gdtf::FixtureBuilder::kMinimalFixtureTypeId);
  assert(canonicalXml.find("Editor=") == std::string::npos);
  assert(canonicalXml.find("PerastageMutationAudit") == std::string::npos);
  assert(canonicalXml.find("<FTPresets") != std::string::npos);
  assert(canonicalXml.find("<Protocols") != std::string::npos);
  assert(ArchiveContainsEntry(canonicalDestination, "wheels/open.png"));
  assert(CountCanonicalizationRevisions(canonicalXml) == 1);
  const auto schemaValidation = perastage::inspection::ValidateXmlAgainstSchema(
      canonicalXml, perastage::inspection::Gdtf12Schema());
  assert(schemaValidation.schema.status ==
         perastage::inspection::ValidationStatus::Valid);
  assert(fixture_gdtf::PublishCanonicalGdtfCopy(
      canonicalDestination, root / "Repeated@Perastage.gdtf", error));
  assert(CountCanonicalizationRevisions(
             ReadDescriptionXml(root / "Repeated@Perastage.gdtf")) == 1);

  const fs::path placeholderSource = root / "PlaceholderSource.gdtf";
  const fs::path placeholderFirst = root / "PlaceholderFirst@Perastage.gdtf";
  const fs::path placeholderSecond = root / "PlaceholderSecond@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Placeholder Fixture", "Perastage",
                           "00000000-0000-0000-0000-000000000001")
      .WithModelResource("main")
      .WithPerastageGeneratedSymbols()
      .WriteArchive(placeholderSource);
  const std::string originalPlaceholderBytes = ReadFileBytes(placeholderSource);
  assert(fixture_gdtf::PublishCanonicalGdtfCopy(
      placeholderSource, placeholderFirst, error));
  assert(fixture_gdtf::PublishCanonicalGdtfCopy(
      placeholderSource, placeholderSecond, error));
  const std::string firstRepairedId =
      ReadFixtureTypeId(ReadDescriptionXml(placeholderFirst));
  const std::string secondRepairedId =
      ReadFixtureTypeId(ReadDescriptionXml(placeholderSecond));
  assert(firstRepairedId == secondRepairedId);
  assert(firstRepairedId != "00000000-0000-0000-0000-000000000001");
  assert(firstRepairedId.size() == 36);
  assert(ReadFileBytes(placeholderSource) == originalPlaceholderBytes);

  const fs::path invalidIdSource = root / "InvalidIdSource.gdtf";
  const fs::path invalidIdDestination = root / "InvalidId@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithFixtureIdentity("Invalid ID Fixture", "Perastage", "not-a-guid")
      .WithModelResource("main")
      .WithPerastageGeneratedSymbols()
      .WriteArchive(invalidIdSource);
  const std::string originalInvalidIdBytes = ReadFileBytes(invalidIdSource);
  assert(!fixture_gdtf::PublishCanonicalGdtfCopy(
      invalidIdSource, invalidIdDestination, error));
  assert(ReadFileBytes(invalidIdSource) == originalInvalidIdBytes);
  assert(!fs::exists(invalidIdDestination));

  const fs::path unknownSource = root / "UnknownSource.gdtf";
  const fs::path refusedDestination = root / "Unknown@Perastage.gdtf";
  tests::gdtf::BuildMinimalValidFixture()
      .WithModelResource("main")
      .WithPerastageGeneratedSymbols()
      .WithFixtureTypeExtensionAttribute("VendorData", "keep")
      .WriteArchive(unknownSource);
  const std::string originalUnknownBytes = ReadFileBytes(unknownSource);
  std::ofstream(refusedDestination, std::ios::binary) << "previous-destination";
  assert(!fixture_gdtf::PublishCanonicalGdtfCopy(
      unknownSource, refusedDestination, error));
  assert(!error.empty());
  assert(ReadFileBytes(unknownSource) == originalUnknownBytes);
  assert(ReadFileBytes(refusedDestination) == "previous-destination");
  fs::remove_all(root);
  return 0;
}
