#include "inspection/package_inspection.h"

#include "wx_path_utils.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;
using namespace perastage::inspection;

namespace {
struct EntryData {
  std::string name;
  std::string contents;
  bool directory = false;
};

// Converts a UTF-8 literal to std::string without execution-encoding
// dependence.
std::string Utf8(const char8_t *value) {
  const std::u8string text(value);
  return std::string(text.begin(), text.end());
}

// Writes an ordered synthetic ZIP package through the production archive
// dependency.
void WritePackage(const fs::path &path, const std::vector<EntryData> &entries) {
  wxFileOutputStream output(WxPathUtils::WxStringFromFilesystemPath(path));
  assert(output.IsOk());
  wxZipOutputStream zip(output);
  for (const EntryData &entry : entries) {
    assert(zip.PutNextEntry(wxString::FromUTF8(entry.name)));
    if (!entry.directory)
      zip.Write(entry.contents.data(), entry.contents.size());
    assert(zip.CloseEntry());
  }
  assert(zip.Close());
}

// Reads a file as bytes for source-package non-mutation checks.
std::vector<unsigned char> ReadBytes(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

// Replaces equal-length ZIP filename bytes in local and central records.
void ReplaceArchiveNameBytes(const fs::path &path, const std::string &from,
                             const std::string &to) {
  assert(from.size() == to.size());
  std::vector<unsigned char> bytes = ReadBytes(path);
  const std::vector<unsigned char> needle(from.begin(), from.end());
  std::size_t replacements = 0;
  for (auto found = std::search(bytes.begin(), bytes.end(), needle.begin(),
                                needle.end());
       found != bytes.end();
       found = std::search(found + to.size(), bytes.end(), needle.begin(),
                           needle.end())) {
    std::copy(to.begin(), to.end(), found);
    ++replacements;
  }
  assert(replacements == 2);
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
                      {"models/", {}, true},
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
}

// Verifies unsafe names remain visible but never become trusted identities.
void TestUnsafeEntryPaths(const fs::path &root) {
  const fs::path path = root / "unsafe.gdtf";
  const std::vector<std::string> unsafeNames = {
      "../escape.bin", "folder/../../escape.bin", "/absolute.bin",
      "C:/absolute.bin", "..\\escape.bin"};
  std::vector<EntryData> entries{{"description.xml", "<GDTF/>"}};
  for (const std::string &name : unsafeNames) {
    if (name == "/absolute.bin")
      continue;
    entries.push_back({name, "unsafe"});
  }
  entries.push_back({"xabsolute.bin", "unsafe"});
  WritePackage(path, entries);
  ReplaceArchiveNameBytes(path, "xabsolute.bin", "/absolute.bin");

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
  TestUnsafeEntryPaths(root);
  fs::remove_all(root, error);
  return 0;
}
