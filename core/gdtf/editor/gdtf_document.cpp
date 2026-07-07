#include "gdtf_document.h"

#include <filesystem>

namespace gdtf {
namespace {

// Builds future-safe summaries for repeated GDTF families already exposed by
// the read model.
std::vector<GdtfRepeatedFamilySummary>
BuildRepeatedFamilies(const GdtfDescriptionSnapshot &description) {
  std::vector<GdtfRepeatedFamilySummary> families;
  GdtfRepeatedFamilySummary wheels;
  wheels.familyKind = "wheel";
  for (const auto &wheel : description.wheels)
    wheels.names.push_back(wheel.name);
  families.push_back(std::move(wheels));
  return families;
}

} // namespace

// Creates a document from shared archive and description read-service results.
GdtfDocument::GdtfDocument(ArchiveReadResult archive,
                           GdtfDescriptionSnapshot description)
    : archive_(std::move(archive)), description_(std::move(description)),
      repeatedFamilies_(BuildRepeatedFamilies(description_)) {}

// Returns the path used to load this document.
const std::filesystem::path &GdtfDocument::SourcePath() const {
  return archive_.sourcePath;
}

// Returns archive-level read data and diagnostics.
const ArchiveReadResult &GdtfDocument::Archive() const { return archive_; }

// Returns the serialization-neutral description snapshot.
const GdtfDescriptionSnapshot &GdtfDocument::Description() const {
  return description_;
}

// Returns the available DMX mode names from the shared read model.
const std::vector<std::string> &GdtfDocument::Modes() const {
  return description_.dmxModeNames;
}

// Returns repeated-family summaries without collapsing them into singleton
// fields.
const std::vector<GdtfRepeatedFamilySummary> &
GdtfDocument::RepeatedFamilies() const {
  return repeatedFamilies_;
}

// Reports whether the source file exists on disk.
bool GdtfDocument::SourceFilePresent() const {
  return !archive_.sourcePath.empty() &&
         std::filesystem::exists(archive_.sourcePath);
}

// Reports whether both archive and description reads succeeded.
bool GdtfDocument::Valid() const {
  return archive_.Success() && description_.Success();
}

// Loads a GDTF document through the shared read-only services.
GdtfDocument LoadGdtfDocument(const std::filesystem::path &sourcePath) {
  auto archive = ReadGdtfArchive(sourcePath);
  auto description = ReadGdtfDescription(archive.descriptionXml, [&archive] {
    std::vector<std::string> paths;
    for (const auto &entry : archive.entries)
      paths.push_back(entry.path);
    return paths;
  }());
  return GdtfDocument(std::move(archive), std::move(description));
}

} // namespace gdtf
