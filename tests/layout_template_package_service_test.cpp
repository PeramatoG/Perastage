#include "LayoutTemplatePackageService.h"
#include "LayoutImageResourceRegistry.h"
#include "runtime_storage.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
    entries[entry->GetName().ToStdString()] = data;
  }
  return entries;
}

// Builds a minimal layout definition for package roundtrip tests.
layouts::LayoutDefinition BuildLayout() {
  layouts::LayoutDefinition layout;
  layout.name = "Unicode Layout ø";
  return layout;
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
  fs::remove_all(dir);
  return 0;
}
