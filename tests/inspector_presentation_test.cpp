#include "inspection/inspector_presentation.h"

#include "filesystem_path_utils.h"

#include <cassert>
#include <filesystem>
#include <string>

namespace {

// Converts a UTF-8 code-unit sequence without using the execution code page.
std::string Utf8String(const std::u8string &value) {
  return std::string(reinterpret_cast<const char *>(value.data()),
                     value.size());
}

// Builds one file resource descriptor for presentation mapping checks.
perastage::inspection::ResourceDescriptor
Resource(perastage::inspection::ResourceKind kind) {
  perastage::inspection::ResourceDescriptor resource;
  resource.entryType = perastage::inspection::PackageEntryType::File;
  resource.kind = kind;
  return resource;
}

// Verifies every structured location component and Unicode path is retained.
void CheckDiagnosticLocation() {
  const std::string unicodePath =
      Utf8String(u8"/shows/\u7167\u660E/escena.mvr");
  perastage::inspection::Diagnostic diagnostic;
  perastage::inspection::DiagnosticLocation location;
  location.sourcePath = PathUtils::PathFromUtf8(unicodePath);
  location.packageEntry = "nested/fixture.gdtf";
  location.xmlPath = "/GeneralSceneDescription/Scene/Layers";
  location.line = 42;
  location.column = 9;
  diagnostic.location = location;

  const std::string expected =
      "source_path=" + unicodePath + "; package_entry=nested/fixture.gdtf; " +
      "xml_path=/GeneralSceneDescription/Scene/Layers; line=42; column=9";
  assert(gui::inspection::DiagnosticLocationText(diagnostic) == expected);
}

// Verifies Core GDTF read states remain distinct at presentation time.
void CheckGdtfStatuses() {
  using perastage::inspection::GdtfReadStatus;
  assert(std::string(gui::inspection::GdtfReadStatusLabel(
             GdtfReadStatus::Canonical)) == "Canonical");
  assert(std::string(gui::inspection::GdtfReadStatusLabel(
             GdtfReadStatus::CompatibilityAccepted)) ==
         "Compatibility accepted");
  assert(std::string(gui::inspection::GdtfReadStatusLabel(
             GdtfReadStatus::Unusable)) == "Unusable");
}

// Verifies supplied Core resource classifications and directory presentation.
void CheckResourceKinds() {
  using perastage::inspection::ResourceKind;
  assert(std::string(gui::inspection::ResourceKindLabel(
             Resource(ResourceKind::XmlText))) == "XML text");
  assert(std::string(gui::inspection::ResourceKindLabel(
             Resource(ResourceKind::Text))) == "text");
  assert(std::string(gui::inspection::ResourceKindLabel(
             Resource(ResourceKind::Image))) == "image");
  assert(std::string(gui::inspection::ResourceKindLabel(
             Resource(ResourceKind::Model))) == "model");
  assert(std::string(gui::inspection::ResourceKindLabel(
             Resource(ResourceKind::NestedGdtf))) == "nested GDTF");

  auto directory = Resource(ResourceKind::Binary);
  directory.entryType = perastage::inspection::PackageEntryType::Directory;
  assert(std::string(gui::inspection::ResourceKindLabel(directory)).empty());
}

} // namespace

// Exercises the Inspector's GUI-independent presentation projection.
int main() {
  CheckDiagnosticLocation();
  CheckGdtfStatuses();
  CheckResourceKinds();
  return 0;
}
