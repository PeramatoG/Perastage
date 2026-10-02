#include "gdtf_filename_policy.h"

#include "gdtf_archive_reader.h"
#include "gdtf_description_reader.h"

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace gdtf_filename_policy {
namespace {

// Trims ASCII whitespace without applying locale-dependent transformations.
std::string TrimAsciiWhitespace(const std::string &text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return {};
  const size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(start, end - start + 1);
}

// Normalizes the optional comment segment for stable ownership recognition.
std::string NormalizeAsciiKey(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  value.erase(std::remove_if(value.begin(), value.end(),
                             [](unsigned char character) {
                               return std::isspace(character) != 0 ||
                                      character == '_' || character == '-';
                             }),
              value.end());
  return value;
}

} // namespace

// Builds the canonical Perastage filename from explicit fixture identity.
std::string BuildCanonicalFileName(const std::string &manufacturer,
                                   const std::string &model,
                                   const std::string &fallbackStem) {
  std::string manufacturerName = TrimAsciiWhitespace(manufacturer);
  std::string fixtureTypeName = TrimAsciiWhitespace(model);
  if (manufacturerName.empty())
    manufacturerName = "Unknown";
  if (fixtureTypeName.empty())
    fixtureTypeName = fallbackStem;
  std::replace(manufacturerName.begin(), manufacturerName.end(), '@', '_');
  std::replace(manufacturerName.begin(), manufacturerName.end(), ' ', '_');
  std::replace(fixtureTypeName.begin(), fixtureTypeName.end(), '@', '_');
  std::replace(fixtureTypeName.begin(), fixtureTypeName.end(), ' ', '_');
  manufacturerName = TrimAsciiWhitespace(manufacturerName);
  fixtureTypeName = TrimAsciiWhitespace(fixtureTypeName);
  if (manufacturerName.empty())
    manufacturerName = "Unknown";
  if (fixtureTypeName.empty())
    fixtureTypeName = "Fixture";
  return manufacturerName + "@" + fixtureTypeName + "@Perastage.gdtf";
}

// Builds the canonical Perastage filename from a GDTF source archive.
std::string BuildCanonicalFileName(const std::filesystem::path &sourcePath) {
  const gdtf::ArchiveReadResult archive = gdtf::ReadGdtfArchive(sourcePath);
  const gdtf::GdtfDescriptionSnapshot description =
      gdtf::ReadGdtfDescription(archive.descriptionXml);
  return BuildCanonicalFileName(description.manufacturer,
                                description.fixtureTypeName,
                                sourcePath.stem().string());
}

// Reports whether a filename carries the canonical Perastage comment segment.
bool IsPerastageNamedFile(const std::filesystem::path &path) {
  const std::string stem = path.stem().string();
  const size_t firstAt = stem.find('@');
  if (firstAt == std::string::npos)
    return false;
  const size_t secondAt = stem.find('@', firstAt + 1);
  if (secondAt == std::string::npos)
    return false;
  return NormalizeAsciiKey(stem.substr(secondAt + 1)) == "perastage";
}

} // namespace gdtf_filename_policy
