#include "inspection/nested_gdtf_inspection.h"
#include "inspection/resource_inspection.h"
#include "support/archive_entry_test_utils.h"
#include "wx_path_utils.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

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

// Writes a streaming ZIP entry whose local header does not claim its final
// size.
void WriteStreamingPackage(const fs::path &path, const std::string &name,
                           const std::string &contents) {
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(path));
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  assert(zip.PutNextEntry(wxString::FromUTF8(name)));
  zip.Write(contents.data(), contents.size());
  zip.Close();
}

// Reads one complete local fixture into an owned byte vector.
std::vector<std::uint8_t> ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// Lowers central-directory size metadata while retaining the streamed payload.
void PatchCentralSize(const fs::path &path, std::uint32_t claimedSize) {
  std::vector<std::uint8_t> bytes = ReadBytes(path);
  const std::vector<std::uint8_t> signature{0x50, 0x4b, 0x01, 0x02};
  const auto found = std::search(bytes.begin(), bytes.end(), signature.begin(),
                                 signature.end());
  assert(found != bytes.end() && bytes.end() - found >= 46);
  for (int index = 0; index < 4; ++index)
    found[24 + index] =
        static_cast<std::uint8_t>((claimedSize >> (index * 8)) & 0xff);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  assert(output.good());
}

// Reverses central-directory records without changing local-file record order.
void ReverseCentralDirectoryRecords(const fs::path &path) {
  std::vector<std::uint8_t> bytes = ReadBytes(path);
  const std::vector<std::uint8_t> eocdSignature{0x50, 0x4b, 0x05, 0x06};
  const auto eocd = std::search(bytes.begin(), bytes.end(),
                                eocdSignature.begin(), eocdSignature.end());
  assert(eocd != bytes.end() && bytes.end() - eocd >= 22);
  const std::size_t eocdOffset = static_cast<std::size_t>(eocd - bytes.begin());
  const std::uint32_t centralOffset =
      tests::archive::ReadLe32(bytes, eocdOffset + 16);
  std::vector<std::vector<std::uint8_t>> records;
  std::size_t cursor = centralOffset;
  while (cursor < eocdOffset) {
    assert(tests::archive::ReadLe32(bytes, cursor) == 0x02014b50);
    const std::size_t size = 46 + tests::archive::ReadLe16(bytes, cursor + 28) +
                             tests::archive::ReadLe16(bytes, cursor + 30) +
                             tests::archive::ReadLe16(bytes, cursor + 32);
    records.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                         bytes.begin() +
                             static_cast<std::ptrdiff_t>(cursor + size));
    cursor += size;
  }
  assert(records.size() == 2 && cursor == eocdOffset);
  cursor = centralOffset;
  for (auto record = records.rbegin(); record != records.rend(); ++record) {
    std::copy(record->begin(), record->end(),
              bytes.begin() + static_cast<std::ptrdiff_t>(cursor));
    cursor += record->size();
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  assert(output.good());
}

// Reports whether a resource operation contains one stable diagnostic code.
bool HasCode(const Result &result, const std::string &code) {
  return std::any_of(
      result.diagnostics.begin(), result.diagnostics.end(),
      [&](const Diagnostic &diagnostic) { return diagnostic.code == code; });
}

// Reports whether one diagnostic preserves compatibility classification.
bool HasCompatibilityCode(const Result &result, const std::string &code) {
  return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                     [&](const Diagnostic &diagnostic) {
                       return diagnostic.code == code &&
                              diagnostic.classification ==
                                  DiagnosticClassification::Compatibility;
                     });
}

// Verifies returned diagnostics never expose an internal GDTF workspace path.
void AssertNoWorkspacePath(const GdtfInspectionResult &result) {
  auto assertLocation = [](const Diagnostic &diagnostic) {
    if (!diagnostic.location || !diagnostic.location->sourcePath)
      return;
    assert(diagnostic.location->sourcePath->string().find("gdtf-inspection-") ==
           std::string::npos);
  };
  for (const Diagnostic &diagnostic : result.inspection.diagnostics)
    assertLocation(diagnostic);
  for (const ValidationResult &validation : result.validation) {
    for (const Diagnostic &diagnostic : validation.diagnostics)
      assertLocation(diagnostic);
  }
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
  const auto repeated =
      DescribePackageResources(path, *package.inventory, 1024);
  assert(repeated.size() == descriptors.size());
  for (std::size_t index = 0; index < descriptors.size(); ++index) {
    assert(repeated[index].displayPath == descriptors[index].displayPath);
    assert(repeated[index].normalizedPath == descriptors[index].normalizedPath);
    assert(repeated[index].kind == descriptors[index].kind);
  }
}

// Verifies large resources are classified from a strictly bounded prefix.
void TestBoundedDescriptorSniff(const fs::path &root) {
  const fs::path path = root / "large-sniff.mvr";
  std::string largePng("\x89PNG\r\n\x1a\n", 8);
  largePng.append(4096, 'x');
  std::string misleadingText("text-prefix");
  misleadingText.append(64, '\0');
  WritePackage(path, {{"large.png", largePng},
                      {"fake.png", "not-image"},
                      {"misleading.txt", misleadingText}});
  const PackageInspectionResult package = InspectPackage(path);
  assert(package.inventory);
  const auto filesystem = DescribePackageResources(path, *package.inventory, 8);
  const std::vector<std::uint8_t> bytes = ReadBytes(path);
  const auto owned =
      DescribePackageResources(bytes, *package.inventory, 8, Request{path});
  assert(filesystem.size() == 3 && owned.size() == filesystem.size());
  assert(filesystem[0].size > 8 && filesystem[0].kind == ResourceKind::Image);
  assert(owned[0].kind == ResourceKind::Image);
  assert(filesystem[1].kind == ResourceKind::Binary);
  assert(filesystem[2].kind == ResourceKind::Binary);
}

// Verifies central-directory order cannot redirect a selected resource payload.
void TestReorderedCentralDirectory(const fs::path &root) {
  const fs::path path = root / "reordered.mvr";
  const std::string xml = "<?xml version=\"1.0\"?><entry>A</entry>";
  const std::string png("\x89PNG\r\n\x1a\nimage-b", 15);
  WritePackage(path, {{"first.xml", xml}, {"second.png", png}});
  ReverseCentralDirectoryRecords(path);
  const std::vector<std::uint8_t> bytes = ReadBytes(path);

  for (const std::string &requested : {"first.xml", "second.png"}) {
    const std::vector<std::uint8_t> expected =
        requested == "first.xml"
            ? std::vector<std::uint8_t>(xml.begin(), xml.end())
            : std::vector<std::uint8_t>(png.begin(), png.end());
    const ResourceKind expectedKind =
        requested == "first.xml" ? ResourceKind::XmlText : ResourceKind::Image;
    const ResourceReadResult filesystem =
        ReadPackageResource(path, PackageKind::Mvr, requested, 1024);
    const ResourceReadResult repeated =
        ReadPackageResource(path, PackageKind::Mvr, requested, 1024);
    const ResourceReadResult owned = ReadPackageResource(
        bytes, PackageKind::Mvr, requested, 1024, Request{path});
    assert(filesystem.Success() && owned.Success());
    assert(filesystem.requestedPath == requested &&
           filesystem.resolvedPath == requested);
    assert(owned.requestedPath == requested && owned.resolvedPath == requested);
    assert(filesystem.bytes == expected && owned.bytes == expected);
    assert(filesystem.kind == expectedKind && owned.kind == expectedKind);
    assert(repeated.bytes == filesystem.bytes &&
           repeated.kind == filesystem.kind);
  }

  const fs::path malformed = root / "mismatched-local-name.mvr";
  WritePackage(malformed, {{"first.xml", xml}, {"second.png", png}});
  std::string error;
  assert(tests::archive::ReplaceRawLocalHeaderName(malformed.string(), 0,
                                                   "other.xml", error));
  const ResourceReadResult rejected =
      ReadPackageResource(malformed, PackageKind::Mvr, "first.xml", 1024);
  assert(!rejected.Success());
  assert(HasCode(rejected.inspection, resource_diagnostic_codes::ReadFailed));
}

// Verifies exact bounded reads, kind sniffing, preview, and diagnostics.
void TestReadsAndPreview(const fs::path &root) {
  const fs::path path = root / "resources.mvr";
  const std::string xml =
      "<?xml version=\"1.0\"?><name>Ca\xC3\xB1\xC3\xB3n</name>";
  const std::string png("\x89PNG\r\n\x1a\nbytes", 13);
  const std::string misleading("binary\0xml", 10);
  const std::string unicodePath = "notes/ca\xC3\xB1\xC3\xB3n.txt";
  const std::string unicodeText = "Texto ca\xC3\xB1\xC3\xB3n";
  WritePackage(path, {{"data.xml", xml},
                      {"image.txt", png},
                      {"fake.xml", misleading},
                      {"fake.png", "not an image"},
                      {"model.glb", "glTFpayload"},
                      {"unknown.bin", "unknown"},
                      {"empty.txt", ""},
                      {unicodePath, unicodeText},
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
  const std::vector<std::uint8_t> packageBytes = ReadBytes(path);
  const ResourceReadResult owned = ReadPackageResource(
      packageBytes, PackageKind::Mvr, "data.xml", 1024, Request{path});
  assert(owned.Success() && owned.bytes == exact.bytes &&
         owned.kind == exact.kind);
  const TextPreviewResult preview =
      PreviewPackageText(path, PackageKind::Mvr, "data.xml", 1024);
  assert(preview.Success() && preview.text == xml);

  assert(ReadPackageResource(path, PackageKind::Mvr, "image.txt", 1024).kind ==
         ResourceKind::Image);
  assert(ReadPackageResource(path, PackageKind::Mvr, "fake.png", 1024).kind ==
         ResourceKind::Binary);
  assert(ReadPackageResource(path, PackageKind::Mvr, "model.glb", 1024).kind ==
         ResourceKind::Model);
  assert(
      ReadPackageResource(path, PackageKind::Mvr, "unknown.bin", 1024).kind ==
      ResourceKind::Binary);
  const TextPreviewResult binary =
      PreviewPackageText(path, PackageKind::Mvr, "fake.xml", 1024);
  assert(ReadPackageResource(path, PackageKind::Mvr, "fake.xml", 1024).kind ==
         ResourceKind::Binary);
  assert(HasCode(binary.inspection,
                 resource_diagnostic_codes::UnsupportedTextPreview));
  const TextPreviewResult empty =
      PreviewPackageText(path, PackageKind::Mvr, "empty.txt", 1024);
  assert(empty.Success() && empty.text.empty());
  const ResourceReadResult unicodeRead =
      ReadPackageResource(path, PackageKind::Mvr, unicodePath, 1024);
  assert(unicodeRead.Success());
  const TextPreviewResult unicodePreview =
      PreviewPackageText(path, PackageKind::Mvr, unicodePath, 1024);
  assert(unicodePreview.Success() && unicodePreview.text == unicodeText);
  const ResourceReadResult oversized =
      ReadPackageResource(path, PackageKind::Mvr, "large.bin", 4);
  assert(HasCode(oversized.inspection, resource_diagnostic_codes::TooLarge));
  const TextPreviewResult oversizedPreview =
      PreviewPackageText(path, PackageKind::Mvr, "data.xml", 4);
  assert(!oversizedPreview.Success());
  assert(HasCode(oversizedPreview.inspection,
                 resource_diagnostic_codes::TooLarge));
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

  const fs::path inconsistent = root / "inconsistent.mvr";
  WriteStreamingPackage(inconsistent, "streamed.bin", "123456789");
  PatchCentralSize(inconsistent, 1);
  const ResourceReadResult streamed =
      ReadPackageResource(inconsistent, PackageKind::Mvr, "streamed.bin", 4);
  assert(!streamed.Success());
  assert(HasCode(streamed.inspection, resource_diagnostic_codes::TooLarge));
}

// Verifies filesystem and owned GDTF reads retain established lookup semantics.
void TestGdtfResourceLookup(const fs::path &root) {
  const std::string png("\x89PNG\r\n\x1a\nmedia", 13);
  const fs::path path = root / "resources.gdtf";
  WritePackage(path, {{"description.xml", GdtfXml()},
                      {"wheels/gobos/Blue.png", png},
                      {"large.bin", "123456789"}});
  const ResourceReadResult exact = ReadPackageResource(
      path, PackageKind::Gdtf, "wheels/gobos/Blue.png", 1024);
  assert(exact.Success() && exact.kind == ResourceKind::Image);

  const ResourceReadResult fallback =
      ReadPackageResource(path, PackageKind::Gdtf, "blue", 1024);
  assert(fallback.Success());
  assert(HasCompatibilityCode(
      fallback.inspection, resource_diagnostic_codes::CompatibilityFallback));
  assert(!HasCode(fallback.inspection, resource_diagnostic_codes::ReadFailed));

  const std::vector<std::uint8_t> bytes = ReadBytes(path);
  const ResourceReadResult owned = ReadPackageResource(
      bytes, PackageKind::Gdtf, "blue", 1024, Request{path});
  assert(owned.Success() && owned.bytes == fallback.bytes);
  assert(HasCompatibilityCode(
      owned.inspection, resource_diagnostic_codes::CompatibilityFallback));

  const ResourceReadResult tooLarge =
      ReadPackageResource(path, PackageKind::Gdtf, "large.bin", 4);
  assert(!tooLarge.Success());
  assert(HasCode(tooLarge.inspection, resource_diagnostic_codes::TooLarge));

  const fs::path ambiguousPath = root / "ambiguous-resource.gdtf";
  WritePackage(ambiguousPath, {{"description.xml", GdtfXml()},
                               {"wheels/gobos/Blue.png", png},
                               {"graphics/blue.png", png}});
  const ResourceReadResult ambiguous =
      ReadPackageResource(ambiguousPath, PackageKind::Gdtf, "blue", 1024);
  assert(!ambiguous.Success());
  assert(HasCode(ambiguous.inspection, resource_diagnostic_codes::Ambiguous));
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
  assert(nested.gdtf->document->SourcePath().empty());
  assert(!nested.gdtf->document->SourceFilePresent());
  assert(nested.gdtf->inspection.request.sourcePath == mvrPath);
  assert(nested.resource.resolvedPath == "fixture.gdtf");
  AssertNoWorkspacePath(*nested.gdtf);

  const std::vector<std::uint8_t> mvrBytes = ReadBytes(mvrPath);
  const NestedGdtfInspectionResult ownedNested = InspectNestedGdtf(
      mvrBytes, "fixture.gdtf", gdtfBytes.size(), Request{mvrPath});
  assert(ownedNested.Success());
  assert(ownedNested.gdtf->document->Description().fixtureTypeName ==
         standalone.document->Description().fixtureTypeName);
  assert(ownedNested.gdtf->document->SourcePath().empty());
  assert(!ownedNested.gdtf->document->SourceFilePresent());
  assert(ownedNested.gdtf->inspection.request.sourcePath == mvrPath);
  assert(ownedNested.resource.resolvedPath == "fixture.gdtf");
  AssertNoWorkspacePath(*ownedNested.gdtf);

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

  const fs::path invalidPath = root / "invalid-nested.mvr";
  WritePackage(invalidPath, {{"invalid.gdtf", "PK\x03\x04not-a-gdtf"}});
  const NestedGdtfInspectionResult invalid =
      InspectNestedGdtf(invalidPath, "invalid.gdtf", 1024);
  assert(!invalid.Success());

  const fs::path malformedGdtfPath = root / "malformed-inner.gdtf";
  WritePackage(malformedGdtfPath, {{"description.xml", "<GDTF>"}});
  const std::vector<std::uint8_t> malformedGdtf = ReadBytes(malformedGdtfPath);
  const std::string malformedPayload(
      reinterpret_cast<const char *>(malformedGdtf.data()),
      malformedGdtf.size());
  const fs::path diagnosticMvr = root / "diagnostic-identity.mvr";
  WritePackage(diagnosticMvr, {{"broken.gdtf", malformedPayload}});
  const NestedGdtfInspectionResult diagnosed =
      InspectNestedGdtf(diagnosticMvr, "broken.gdtf", malformedGdtf.size());
  assert(!diagnosed.Success() && diagnosed.gdtf);
  assert(diagnosed.resource.resolvedPath == "broken.gdtf");
  assert(std::any_of(diagnosed.gdtf->inspection.diagnostics.begin(),
                     diagnosed.gdtf->inspection.diagnostics.end(),
                     [&](const Diagnostic &diagnostic) {
                       return diagnostic.location &&
                              diagnostic.location->sourcePath ==
                                  std::optional<fs::path>(diagnosticMvr) &&
                              diagnostic.location->packageEntry ==
                                  std::optional<std::string>("description.xml");
                     }));
  AssertNoWorkspacePath(*diagnosed.gdtf);
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
  TestBoundedDescriptorSniff(root);
  TestReorderedCentralDirectory(root);
  TestReadsAndPreview(root);
  TestGdtfResourceLookup(root);
  TestNestedGdtf(root);
  fs::remove_all(root, error);
  return 0;
}
