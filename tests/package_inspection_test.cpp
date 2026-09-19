#include "inspection/package_inspection.h"

#include "support/archive_entry_test_utils.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace perastage::inspection;

namespace {
// Converts a UTF-8 literal to std::string without execution-encoding
// dependence.
std::string Utf8(const char8_t *value) {
  const std::u8string text(value);
  return std::string(text.begin(), text.end());
}

// Writes one deterministic classic ZIP with byte-exact raw entry names.
void WritePackage(
    const fs::path &path,
    const std::vector<std::pair<std::string, std::string>> &entries) {
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path, entries, error));
  assert(error.empty());
}

// Reads a file as bytes for source-package non-mutation checks.
std::vector<unsigned char> ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// Truncates trailing ZIP directory bytes for deterministic malformed input.
void TruncateBytes(const fs::path &path, std::size_t count) {
  std::vector<unsigned char> bytes = ReadBytes(path);
  assert(bytes.size() > count);
  bytes.resize(bytes.size() - count);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  assert(output.good());
}

// Replaces the classic EOCD entry count with the ZIP64 sentinel value.
void PatchZip64EntryCountSentinel(const fs::path &path) {
  std::vector<unsigned char> bytes = ReadBytes(path);
  const std::vector<unsigned char> signature{0x50, 0x4b, 0x05, 0x06};
  const auto found = std::search(bytes.begin(), bytes.end(), signature.begin(),
                                 signature.end());
  assert(found != bytes.end() && bytes.end() - found >= 22);
  found[8] = found[9] = found[10] = found[11] = 0xff;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  assert(output.good());
}

// Marks the EOCD as a split archive without changing its bounded structure.
void PatchMultiDiskSentinel(const fs::path &path) {
  std::vector<unsigned char> bytes = ReadBytes(path);
  const std::vector<unsigned char> signature{0x50, 0x4b, 0x05, 0x06};
  const auto found = std::search(bytes.begin(), bytes.end(), signature.begin(),
                                 signature.end());
  assert(found != bytes.end() && bytes.end() - found >= 22);
  found[4] = 1;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  assert(output.good());
}

// Finds an inventory entry by its original display spelling.
const PackageEntry *FindEntry(const PackageInventory &inventory,
                              const std::string &displayPath) {
  const auto found =
      std::find_if(inventory.entries.begin(), inventory.entries.end(),
                   [&](const PackageEntry &entry) {
                     return entry.displayPath == displayPath;
                   });
  return found == inventory.entries.end() ? nullptr : &*found;
}

// Reports whether a package result contains one stable diagnostic code.
bool HasCode(const PackageInspectionResult &result, const std::string &code) {
  return std::any_of(
      result.inspection.diagnostics.begin(),
      result.inspection.diagnostics.end(),
      [&](const Diagnostic &diagnostic) { return diagnostic.code == code; });
}

// Verifies supported GDTF classification and neutral nested-resource metadata.
void TestGdtfInventory(const fs::path &root) {
  const fs::path path = root / "fixture.GDTF";
  WritePackage(path, {{"description.xml", "<GDTF/>"},
                      {"models/", {}},
                      {"models/body.glb", "model"}});
  const std::vector<unsigned char> before = ReadBytes(path);
  const PackageInspectionResult result = InspectPackage(path);
  assert(result.inspection.Success());
  assert(result.inventory && result.inventory->kind == PackageKind::Gdtf);
  assert(result.inventory->canonicalRootDocumentPresent);
  assert(result.inventory->entries.size() == 3);
  const PackageEntry *directory = FindEntry(*result.inventory, "models/");
  const PackageEntry *model = FindEntry(*result.inventory, "models/body.glb");
  assert(directory && directory->type == PackageEntryType::Directory);
  assert(model && model->type == PackageEntryType::File && model->pathSafe);
  assert(model->normalizedPath == "models/body.glb");
  assert(model->extension == ".glb" && model->sizeKnown &&
         model->uncompressedSize == 5);
  assert(ReadBytes(path) == before);
}

// Verifies MVR classification, resources, Unicode spelling, and determinism.
void TestMvrInventoryAndUnicode(const fs::path &root) {
  const fs::path unicodeDirectory =
      root / fs::path(std::u8string(u8"escena-á"));
  fs::create_directories(unicodeDirectory);
  const fs::path path = unicodeDirectory / "scene.MVR";
  const std::string unicodeEntry = Utf8(u8"texturas/cañón.png");
  WritePackage(path,
               {{"GeneralSceneDescription.xml", "<GeneralSceneDescription/>"},
                {"fixture.gdtf", "package"},
                {unicodeEntry, "image"}});
  const PackageInspectionResult first = InspectPackage(path);
  const PackageInspectionResult second = InspectPackage(path);
  assert(first.inspection.Success() && first.inventory);
  assert(first.inventory->kind == PackageKind::Mvr);
  assert(first.inventory->canonicalRootDocumentPresent);
  assert(FindEntry(*first.inventory, "fixture.gdtf"));
  const PackageEntry *unicode = FindEntry(*first.inventory, unicodeEntry);
  assert(unicode && unicode->normalizedPath == unicodeEntry);
  assert(first.inventory->entries.size() == second.inventory->entries.size());
  for (std::size_t index = 0; index < first.inventory->entries.size();
       ++index) {
    const PackageEntry &left = first.inventory->entries[index];
    const PackageEntry &right = second.inventory->entries[index];
    assert(left.displayPath == right.displayPath);
    assert(left.normalizedPath == right.normalizedPath);
    assert(left.type == right.type && left.sizeKnown == right.sizeKnown);
    assert(left.uncompressedSize == right.uncompressedSize);
  }
  assert(first.inspection.diagnostics.size() ==
         second.inspection.diagnostics.size());
}

// Verifies unsupported input never falls through to ZIP or XML guessing.
void TestUnsupportedInputs(const fs::path &root) {
  for (const std::string &name : {"package.zip", "scene.xml", "notes.txt"}) {
    const fs::path path = root / name;
    std::ofstream(path) << "not inspected";
    const PackageInspectionResult result = InspectPackage(path);
    assert(!result.inventory);
    assert(HasCode(result, package_diagnostic_codes::UnsupportedFileType));
    assert(!HasCode(result, package_diagnostic_codes::MalformedArchive));
  }
  const PackageInspectionResult empty = InspectPackage(fs::path());
  assert(HasCode(empty, package_diagnostic_codes::EmptyInputPath));
}

// Verifies malformed supported packages produce structured container
// diagnostics.
void TestMalformedSupportedInput(const fs::path &root) {
  const fs::path path = root / "broken.mvr";
  std::ofstream(path, std::ios::binary) << "not a zip";
  const PackageInspectionResult result = InspectPackage(path);
  assert(!result.inventory);
  assert(HasCode(result, package_diagnostic_codes::MalformedArchive));
  assert(!HasCode(result, package_diagnostic_codes::UnsupportedFileType));

  const fs::path truncated = root / "truncated.gdtf";
  WritePackage(truncated, {{"description.xml", "<GDTF/>"}});
  TruncateBytes(truncated, 8);
  const PackageInspectionResult truncatedResult = InspectPackage(truncated);
  assert(!truncatedResult.inventory);
  assert(HasCode(truncatedResult, package_diagnostic_codes::MalformedArchive));
  assert(
      !HasCode(truncatedResult, package_diagnostic_codes::UnsupportedFileType));

  const fs::path zip64 = root / "zip64.mvr";
  WritePackage(zip64,
               {{"GeneralSceneDescription.xml", "<GeneralSceneDescription/>"}});
  PatchZip64EntryCountSentinel(zip64);
  const PackageInspectionResult zip64Result = InspectPackage(zip64);
  assert(!zip64Result.inventory);
  assert(
      HasCode(zip64Result, package_diagnostic_codes::UnsupportedZipStructure));
  assert(!HasCode(zip64Result, package_diagnostic_codes::MalformedArchive));

  const fs::path multiDisk = root / "multi-disk.gdtf";
  WritePackage(multiDisk, {{"description.xml", "<GDTF/>"}});
  PatchMultiDiskSentinel(multiDisk);
  const PackageInspectionResult multiDiskResult = InspectPackage(multiDisk);
  assert(!multiDiskResult.inventory);
  assert(HasCode(multiDiskResult,
                 package_diagnostic_codes::UnsupportedZipStructure));
  assert(!HasCode(multiDiskResult, package_diagnostic_codes::MalformedArchive));
}

// Verifies invalid raw UTF-8 is diagnosed without trusting the damaged name.
void TestInvalidFilenameEncoding(const fs::path &root) {
  const fs::path path = root / "invalid-name.gdtf";
  const std::string invalidName("bad-\xc3.bin", 9);
  WritePackage(path, {{"description.xml", "<GDTF/>"},
                      {invalidName, "invalid"},
                      {"models/body.glb", "safe"}});

  const PackageInspectionResult first = InspectPackage(path);
  const PackageInspectionResult second = InspectPackage(path);
  assert(first.inventory && second.inventory);
  assert(HasCode(first, package_diagnostic_codes::FilenameDecodeFailed));
  assert(HasCode(second, package_diagnostic_codes::FilenameDecodeFailed));
  assert(FindEntry(*first.inventory, "description.xml"));
  assert(FindEntry(*first.inventory, "models/body.glb"));
  assert(std::none_of(first.inventory->entries.begin(),
                      first.inventory->entries.end(),
                      [&](const PackageEntry &entry) {
                        return entry.normalizedPath == invalidName;
                      }));
  assert(first.inventory->entries.size() == second.inventory->entries.size());
  assert(first.inspection.diagnostics.size() ==
         second.inspection.diagnostics.size());
}

// Verifies unsafe names remain visible but never become trusted identities.
void TestUnsafeEntryPaths(const fs::path &root) {
  const fs::path path = root / "unsafe.gdtf";
  const std::vector<std::string> unsafeNames = {
      "../escape.bin", "folder/../../escape.bin", "/absolute.bin",
      "C:/absolute.bin", "..\\escape.bin"};
  std::vector<std::pair<std::string, std::string>> entries{
      {"description.xml", "<GDTF/>"}};
  for (const std::string &name : unsafeNames)
    entries.push_back({name, "unsafe"});
  WritePackage(path, entries);

  const PackageInspectionResult result = InspectPackage(path);
  assert(result.inventory);
  for (const std::string &name : unsafeNames) {
    const PackageEntry *entry = FindEntry(*result.inventory, name);
    assert(entry && !entry->pathSafe && !entry->normalizedPath);
  }
  assert(std::count_if(result.inspection.diagnostics.begin(),
                       result.inspection.diagnostics.end(),
                       [](const Diagnostic &diagnostic) {
                         return diagnostic.code ==
                                package_diagnostic_codes::UnsafeEntryPath;
                       }) == static_cast<std::ptrdiff_t>(unsafeNames.size()));
}
} // namespace

// Runs focused production-path package inventory coverage without application
// state.
int main() {
  const fs::path root =
      fs::temp_directory_path() / "perastage_inspection_package_test";
  std::error_code error;
  fs::remove_all(root, error);
  fs::create_directories(root);
  TestGdtfInventory(root);
  TestMvrInventoryAndUnicode(root);
  TestUnsupportedInputs(root);
  TestMalformedSupportedInput(root);
  TestInvalidFilenameEncoding(root);
  TestUnsafeEntryPaths(root);
  fs::remove_all(root, error);
  return 0;
}
