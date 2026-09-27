#include "inspection/inspector_presentation.h"

#include <cassert>
#include <filesystem>
#include <string>

namespace {

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
  perastage::inspection::Diagnostic diagnostic;
  perastage::inspection::DiagnosticLocation location;
  location.sourcePath = std::filesystem::path(
      reinterpret_cast<const char8_t *>(u8"/shows/照明/escena.mvr"));
  location.packageEntry = "nested/fixture.gdtf";
  location.xmlPath = "/GeneralSceneDescription/Scene/Layers";
  location.line = 42;
  location.column = 9;
  diagnostic.location = location;

  assert(gui::inspection::DiagnosticLocationText(diagnostic) ==
         "source_path=/shows/照明/escena.mvr; "
         "package_entry=nested/fixture.gdtf; "
         "xml_path=/GeneralSceneDescription/Scene/Layers; line=42; column=9");
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
