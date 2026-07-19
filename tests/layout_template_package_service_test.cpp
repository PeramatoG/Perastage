#include "LayoutTemplatePackageService.h"
#include "LayoutTemplateSerializer.h"
#include "LayoutImageResourceRegistry.h"
#include "runtime_storage.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <wx/filename.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

namespace {

// Converts a filesystem path to UTF-8 text.
std::string ToUtf8(const fs::path &path) {
  const auto text = path.u8string();
  return std::string(text.begin(), text.end());
}

// Writes bytes to a test-owned file path.
void WriteFile(const fs::path &path, const std::string &bytes) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << bytes;
}

// Reads a package into an entry map for package assertions.
std::map<std::string, std::string> ReadZipEntries(const fs::path &path) {
  wxFFileInputStream input(path.string());
  wxZipInputStream zip(input);
  std::map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    std::string data;
    char buffer[4096];
    while (true) {
      zip.Read(buffer, sizeof(buffer));
      const size_t bytes = zip.LastRead();
      if (bytes == 0)
        break;
      data.append(buffer, bytes);
    }
    const wxScopedCharBuffer utf8 = entry->GetName(wxPATH_UNIX).utf8_str();
    entries[utf8.data() == nullptr ? std::string() : std::string(utf8.data())] = data;
  }
  return entries;
}

// Builds a minimal layout definition for package roundtrip tests.
layouts::LayoutDefinition BuildLayout() {
  layouts::LayoutDefinition layout;
  layout.name = "Unicode Layout ø";
  return layout;
}


// Writes one ZIP entry using explicit Unix path semantics.
void WriteZipEntry(wxZipOutputStream &zip, const std::string &name,
                   const std::string &content, bool directory = false) {
  auto *entry = new wxZipEntry();
  entry->SetName(wxString::FromUTF8(name.c_str()), wxPATH_UNIX);
  assert(zip.PutNextEntry(entry));
  if (!directory)
    zip.Write(content.data(), content.size());
  assert(zip.CloseEntry());
}

// Writes a handcrafted package for archive validation tests.
void WritePackageZip(const fs::path &path,
                     const std::vector<std::pair<std::string, std::string>> &entries) {
  wxFFileOutputStream output(ToUtf8(path));
  wxZipOutputStream zip(output);
  for (const auto &[name, content] : entries)
    WriteZipEntry(zip, name, content, !name.empty() && name.back() == '/');
  assert(zip.Close());
}

// Builds a minimal valid package entry set for archive validation tests.
std::vector<std::pair<std::string, std::string>> MinimalPackageEntries() {
  layouts::LayoutDefinition layout = BuildLayout();
  const std::string manifest = R"({
  "format": "perastage-layout-package",
  "packageVersion": 1,
  "layoutSchemaVersion": 1,
  "entryPoint": "layout.json",
  "createdWith": "Perastage"
})";
  return {{"manifest.json", manifest},
          {"layout.json", layouts::ToTemplateDocument({layout}).dump(2)}};
}

// Verifies packages without images can be round-tripped.
void TestRoundTripWithoutImages(const fs::path &dir) {
  layouts::LayoutDefinition layout = BuildLayout();
  const fs::path package = dir / "no_images.pslayout";
  std::string error;
  assert(layouts::LayoutTemplatePackageService::ExportPackage(
      layout, ToUtf8(package), &error));
  layouts::LayoutTemplateImportResult imported;
  assert(layouts::LayoutTemplatePackageService::ImportFile(
      ToUtf8(package), imported, &error));
  assert(imported.layout.name == layout.name);
  assert(imported.layout.imageViews.empty());
}

// Verifies image bytes are packaged once and local paths are removed.
void TestImagePortabilityAndDeduplication(const fs::path &dir) {
  const fs::path imagePath = dir / "source image.png";
  WriteFile(imagePath, "same-image-bytes");
  layouts::LayoutDefinition layout = BuildLayout();
  layouts::LayoutImageDefinition image;
  image.id = 1;
  image.imagePath = ToUtf8(imagePath);
  image.originalImagePath = ToUtf8(imagePath);
  layout.imageViews.push_back(image);
  image.id = 2;
  layout.imageViews.push_back(image);

  const fs::path package = dir / "with_images.pslayout";
  std::string error;
  assert(layouts::LayoutTemplatePackageService::ExportPackage(
      layout, ToUtf8(package), &error));
  auto entries = ReadZipEntries(package);
  int imageEntryCount = 0;
  std::string allText;
  for (const auto &[name, content] : entries) {
    allText += name + "\n" + content + "\n";
    if (name.rfind("resources/layout_images/", 0) == 0)
      ++imageEntryCount;
  }
  assert(imageEntryCount == 1);

  bool foundCanonicalImage = false;
  for (const auto &[name, content] : entries) {
    (void)content;
    assert(name.find('\\') == std::string::npos);
    if (name.rfind("resources/layout_images/", 0) == 0) {
      assert(name.find('/') != std::string::npos);
      foundCanonicalImage = true;
    }
  }
  assert(foundCanonicalImage);
  assert(layouts::LayoutTemplatePackageService::ValidatePortablePackage(
      ToUtf8(package), &error));
  assert(allText.find(ToUtf8(imagePath)) == std::string::npos);
  assert(allText.find("originalPath") == std::string::npos);

  fs::remove(imagePath);
  layouts::LayoutTemplateImportResult imported;
  assert(layouts::LayoutTemplatePackageService::ImportFile(
      ToUtf8(package), imported, &error));
  assert(imported.layout.imageViews.size() == 2);
  for (const auto &importedImage : imported.layout.imageViews) {
    assert(fs::exists(fs::path(importedImage.imagePath)));
    assert(importedImage.projectResourcePath.rfind("resources/layout_images/", 0) == 0);
  }
}

// Verifies export can use registry bytes after a source file disappears.
void TestRegistryFallback(const fs::path &dir) {
  std::vector<std::uint8_t> bytes = {'r', 'e', 'g'};
  layouts::LayoutImageResourceRegistry::Get().RegisterResourceBytes(
      "resources/layout_images/registered.png", {}, bytes);
  layouts::LayoutDefinition layout = BuildLayout();
  layouts::LayoutImageDefinition image;
  image.id = 1;
  image.imagePath = ToUtf8(dir / "missing.png");
  image.projectResourcePath = "resources/layout_images/registered.png";
  layout.imageViews.push_back(image);
  std::string error;
  assert(layouts::LayoutTemplatePackageService::ExportPackage(
      layout, ToUtf8(dir / "registry.pslayout"), &error));
}

// Verifies unresolved image bytes fail without replacing an existing destination.
void TestFailedExportPreservesDestination(const fs::path &dir) {
  const fs::path package = dir / "preserve.pslayout";
  WriteFile(package, "existing");
  layouts::LayoutDefinition layout = BuildLayout();
  layouts::LayoutImageDefinition image;
  image.id = 1;
  image.imagePath = ToUtf8(dir / "missing.png");
  layout.imageViews.push_back(image);
  std::string error;
  assert(!layouts::LayoutTemplatePackageService::ExportPackage(
      layout, ToUtf8(package), &error));
  std::ifstream in(package, std::ios::binary);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  assert(content == "existing");
  assert(!fs::exists(package.string() + ".tmp"));
}

// Verifies safe directory entries are accepted and malicious entries are rejected.
void TestArchiveEntryValidation(const fs::path &dir) {
  std::string error;
  auto entries = MinimalPackageEntries();
  entries.insert(entries.begin(), {"resources/", ""});
  entries.insert(entries.begin() + 1, {"resources/layout_images/", ""});
  const fs::path directories = dir / "directories.pslayout";
  WritePackageZip(directories, entries);
  assert(layouts::LayoutTemplatePackageService::ValidatePortablePackage(
      ToUtf8(directories), &error));

  const fs::path traversal = dir / "traversal.pslayout";
  entries = MinimalPackageEntries();
  entries.push_back({"resources/../image.png", "bad"});
  WritePackageZip(traversal, entries);
  assert(!layouts::LayoutTemplatePackageService::ValidatePortablePackage(
      ToUtf8(traversal), &error));
  assert(error.find("unsafe archive entry") != std::string::npos);

  const fs::path absolute = dir / "absolute.pslayout";
  entries = MinimalPackageEntries();
  entries.push_back({"/absolute.png", "bad"});
  WritePackageZip(absolute, entries);
  assert(!layouts::LayoutTemplatePackageService::ValidatePortablePackage(
      ToUtf8(absolute), &error));
  assert(error.find("unsafe archive entry") != std::string::npos);

  const fs::path backslash = dir / "backslash.pslayout";
  entries = MinimalPackageEntries();
  entries.push_back({"resources\\layout_images\\bad.png", "bad"});
  WritePackageZip(backslash, entries);
  assert(!layouts::LayoutTemplatePackageService::ValidatePortablePackage(
      ToUtf8(backslash), &error));
  assert(error.find("unsafe archive entry") != std::string::npos);

  const fs::path duplicateManifest = dir / "duplicate_manifest.pslayout";
  entries = MinimalPackageEntries();
  entries.push_back({"manifest.json", entries.front().second});
  WritePackageZip(duplicateManifest, entries);
  assert(!layouts::LayoutTemplatePackageService::ValidatePortablePackage(
      ToUtf8(duplicateManifest), &error));
  assert(error.find("duplicate archive entry") != std::string::npos);

  const fs::path duplicateImage = dir / "duplicate_image.pslayout";
  entries = MinimalPackageEntries();
  entries.push_back({"resources/layout_images/a.png", "image"});
  entries.push_back({"resources/layout_images/a.png", "image"});
  WritePackageZip(duplicateImage, entries);
  assert(!layouts::LayoutTemplatePackageService::ValidatePortablePackage(
      ToUtf8(duplicateImage), &error));
  assert(error.find("duplicate archive entry") != std::string::npos);
}

// Verifies export validation does not import package resources into the registry.
void TestExportValidationHasNoImportSideEffects(const fs::path &dir) {
  layouts::LayoutImageResourceRegistry::Get().Clear();
  const fs::path imagePath = dir / "side_effect.png";
  WriteFile(imagePath, "side-effect-bytes");
  layouts::LayoutDefinition layout = BuildLayout();
  layouts::LayoutImageDefinition image;
  image.id = 1;
  image.imagePath = ToUtf8(imagePath);
  layout.imageViews.push_back(image);
  std::string error;
  const fs::path package = dir / "side_effect.pslayout";
  assert(layouts::LayoutTemplatePackageService::ExportPackage(
      layout, ToUtf8(package), &error));
  auto entries = ReadZipEntries(package);
  for (const auto &[name, content] : entries) {
    (void)content;
    if (name.rfind("resources/layout_images/", 0) == 0)
      assert(!layouts::LayoutImageResourceRegistry::Get().HasResourceBytes(name));
  }
}

// Verifies legacy JSON parser and image-resolution warnings are preserved together.
void TestLegacyJsonWarningsArePreserved(const fs::path &dir) {
  const fs::path legacy = dir / "legacy.json";
  const std::string json = R"({
    "schemaVersion": 1,
    "layouts": [{
      "name": "Legacy",
      "unknown": true,
      "imageViews": [{
        "id": 1,
        "frame": {"x": 0, "y": 0, "width": 10, "height": 10},
        "path": "missing.png"
      }]
    }]
  })";
  WriteFile(legacy, json);
  layouts::LayoutTemplateImportResult result;
  std::string error;
  assert(layouts::LayoutTemplatePackageService::ImportLegacyJson(
      ToUtf8(legacy), result, &error));
  bool hasImageWarning = false;
  for (const auto &warning : result.warnings)
    hasImageWarning = hasImageWarning ||
                      warning.find("Legacy JSON image could not be resolved") != std::string::npos;
  assert(hasImageWarning);
}

} // namespace

// Runs focused portable layout package service coverage.
int main() {
  const fs::path dir = fs::temp_directory_path() / "perastage_layout_package_test";
  fs::remove_all(dir);
  fs::create_directories(dir);
  runtime_storage::SetRuntimeRootOverrideForTests(dir / "runtime");
  TestRoundTripWithoutImages(dir);
  TestImagePortabilityAndDeduplication(dir);
  TestRegistryFallback(dir);
  TestFailedExportPreservesDestination(dir);
  TestArchiveEntryValidation(dir);
  TestExportValidationHasNoImportSideEffects(dir);
  TestLegacyJsonWarningsArePreserved(dir);
  fs::remove_all(dir);
  return 0;
}
