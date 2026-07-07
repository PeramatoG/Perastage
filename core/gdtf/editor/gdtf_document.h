#pragma once

#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"

#include <filesystem>
#include <string>
#include <vector>

namespace gdtf {

struct GdtfRepeatedFamilySummary {
  std::string familyKind;
  std::vector<std::string> names;
};

class GdtfDocument {
public:
  GdtfDocument() = default;
  GdtfDocument(ArchiveReadResult archive, GdtfDescriptionSnapshot description);

  const std::filesystem::path &SourcePath() const;
  const ArchiveReadResult &Archive() const;
  const GdtfDescriptionSnapshot &Description() const;
  const std::vector<std::string> &Modes() const;
  const std::vector<GdtfRepeatedFamilySummary> &RepeatedFamilies() const;
  bool SourceFilePresent() const;
  bool Valid() const;

private:
  ArchiveReadResult archive_;
  GdtfDescriptionSnapshot description_;
  std::vector<GdtfRepeatedFamilySummary> repeatedFamilies_;
};

GdtfDocument LoadGdtfDocument(const std::filesystem::path &sourcePath);

} // namespace gdtf
