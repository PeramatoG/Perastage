#include "inspection/nested_gdtf_inspection.h"
#include "inspection/resource_inspection.h"
#include "support/archive_entry_test_utils.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <wx/init.h>

namespace fs = std::filesystem;
using namespace perastage::inspection;

namespace {
// Writes one deterministic stored package used by resource tests.
void WritePackage(
    const fs::path &path,
    const std::vector<std::pair<std::string, std::string>> &entries) {
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path, entries, error));
  assert(error.empty());
}

// Reads one complete local fixture into an owned byte vector.
std::vector<std::uint8_t> ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// Reports whether a resource operation contains one stable diagnostic code.
bool HasCode(const Result &result, const std::string &code) {
  return std::any_of(
      result.diagnostics.begin(), result.diagnostics.end(),
      [&](const Diagnostic &diagnostic) { return diagnostic.code == code; });
}

// Returns standards-valid minimal GDTF XML for nested inspection parity.
std::string GdtfXml() {
  return "<GDTF DataVersion=\"1.2\"><FixtureType Name=\"Fixture\" "
         "Manufacturer=\"Perastage\" Description=\"Nested\" "
         "FixtureTypeID=\"12345678-1234-4234-9234-123456789abc\">"
         "<AttributeDefinitions><FeatureGroups/><Attributes/>"
         "</AttributeDefinitions><Geometries><Geometry Name=\"Root\"/>"
         "</Geometries><DMXModes><DMXMode Name=\"Mode\" Geometry=\"Root\">"
         "<DMXChannels/></DMXMode></DMXModes></FixtureType></GDTF>";
}

// Verifies descriptors preserve authoritative inventory facts and Unicode.
void TestDescriptors(const fs::path &root) {
  const fs::path path = root / "descriptors.mvr";
  const std::string unicode = "textures/ca\xC3\xB1\xC3\xB3n.png";
  WritePackage(path, {{"folder/", {}}, {unicode, "payload"}});
  const PackageInspectionResult package = InspectPackage(path);
  assert(package.inventory);
  const auto descriptors =
      DescribePackageResources(path, *package.inventory, 1024);
  assert(descriptors.size() == 2);
  assert(descriptors[0].entryType == PackageEntryType::Directory);
  assert(!descriptors[0].rawReadSupported);
  assert(descriptors[1].displayPath == unicode);
  assert(descriptors[1].normalizedPath == unicode);
  assert(descriptors[1].sizeKnown && descriptors[1].size == 7);
  assert(descriptors[1].pathSafe && descriptors[1].rawReadSupported);
}

// Verifies exact bounded reads, kind sniffing, preview, and diagnostics.
void TestReadsAndPreview(const fs::path &root) {
  const fs::path path = root / "resources.mvr";
  const std::string xml =
      "<?xml version=\"1.0\"?><name>Ca\xC3\xB1\xC3\xB3n</name>";
  const std::string png("\x89PNG\r\n\x1a\nbytes", 13);
  const std::string misleading("binary\0xml", 10);
  WritePackage(path, {{"data.xml", xml},
                      {"image.txt", png},
                      {"fake.xml", misleading},
                      {"large.bin", "123456789"},
                      {"Case.txt", "one"},
                      {"case.txt", "two"}});

  const ResourceReadResult exact =
      ReadPackageResource(path, PackageKind::Mvr, "data.xml", 1024);
  assert(exact.Success() && exact.kind == ResourceKind::XmlText);
  assert(exact.bytes == std::vector<std::uint8_t>(xml.begin(), xml.end()));
  const ResourceReadResult repeated =
      ReadPackageResource(path, PackageKind::Mvr, "data.xml", 1024);
  assert(repeated.bytes == exact.bytes);
  const TextPreviewResult preview =
      PreviewPackageText(path, PackageKind::Mvr, "data.xml", 1024);
  assert(preview.Success() && preview.text == xml);

  assert(ReadPackageResource(path, PackageKind::Mvr, "image.txt", 1024).kind ==
         ResourceKind::Image);
  const TextPreviewResult binary =
      PreviewPackageText(path, PackageKind::Mvr, "fake.xml", 1024);
  assert(HasCode(binary.inspection,
                 resource_diagnostic_codes::UnsupportedTextPreview));
  const ResourceReadResult oversized =
      ReadPackageResource(path, PackageKind::Mvr, "large.bin", 4);
  assert(HasCode(oversized.inspection, resource_diagnostic_codes::TooLarge));
  assert(HasCode(
      ReadPackageResource(path, PackageKind::Mvr, "missing", 10).inspection,
      resource_diagnostic_codes::NotFound));
  assert(HasCode(
      ReadPackageResource(path, PackageKind::Mvr, "../data.xml", 10).inspection,
      resource_diagnostic_codes::UnsafePath));
  assert(HasCode(
      ReadPackageResource(path, PackageKind::Mvr, "Case.txt", 10).inspection,
      resource_diagnostic_codes::Ambiguous));
  assert(HasCode(
      ReadPackageResource(path, PackageKind::Mvr, "data.xml", 0).inspection,
      resource_diagnostic_codes::InvalidReadLimit));
}

// Verifies embedded packages reach the same standalone GDTF implementation.
void TestNestedGdtf(const fs::path &root) {
  const fs::path gdtfPath = root / "fixture.gdtf";
  WritePackage(gdtfPath, {{"description.xml", GdtfXml()}});
  const std::vector<std::uint8_t> gdtfBytes = ReadBytes(gdtfPath);
  const std::string embedded(reinterpret_cast<const char *>(gdtfBytes.data()),
                             gdtfBytes.size());
  const fs::path mvrPath = root / "nested.mvr";
  WritePackage(mvrPath, {{"fixture.gdtf", embedded}});

  const GdtfInspectionResult standalone = InspectGdtf(gdtfPath);
  const NestedGdtfInspectionResult nested =
      InspectNestedGdtf(mvrPath, "fixture.gdtf", gdtfBytes.size());
  assert(standalone.Success() && nested.Success());
  assert(nested.gdtf->document->Description().fixtureTypeName ==
         standalone.document->Description().fixtureTypeName);
  assert(nested.gdtf->document->Modes() == standalone.document->Modes());
  assert(nested.gdtf->document->SourcePath() == mvrPath);

  const NestedGdtfInspectionResult tooSmall =
      InspectNestedGdtf(mvrPath, "fixture.gdtf", gdtfBytes.size() - 1);
  assert(!tooSmall.gdtf);
  assert(HasCode(tooSmall.resource.inspection,
                 resource_diagnostic_codes::TooLarge));

  const NestedGdtfInspectionResult missing =
      InspectNestedGdtf(mvrPath, "missing.gdtf", gdtfBytes.size());
  assert(!missing.gdtf);
  assert(HasCode(missing.resource.inspection,
                 resource_diagnostic_codes::NotFound));

  const fs::path ambiguousPath = root / "ambiguous.mvr";
  WritePackage(ambiguousPath,
               {{"fixture.gdtf", embedded}, {"Fixture.gdtf", embedded}});
  const NestedGdtfInspectionResult ambiguous =
      InspectNestedGdtf(ambiguousPath, "fixture.gdtf", gdtfBytes.size());
  assert(!ambiguous.gdtf);
  assert(HasCode(ambiguous.resource.inspection,
                 resource_diagnostic_codes::Ambiguous));
}
} // namespace

// Runs deterministic resource inspection without project state or network use.
int main() {
  wxInitializer initializer;
  assert(initializer.IsOk());
  const fs::path root =
      fs::temp_directory_path() / "perastage_resource_inspection_test";
  std::error_code error;
  fs::remove_all(root, error);
  fs::create_directories(root);
  TestDescriptors(root);
  TestReadsAndPreview(root);
  TestNestedGdtf(root);
  fs::remove_all(root, error);
  return 0;
}
