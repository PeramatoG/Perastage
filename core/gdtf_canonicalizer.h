#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <tinyxml2.h>

namespace GdtfCanonicalizer {

struct Result {
  bool success = false;
  bool changed = false;
  std::vector<std::string> warnings;
  std::vector<std::string> errors;
};

struct Options {
  bool allowFixtureTypeIdRepair = false;
  std::string stableIdSeed;
  std::string sourceLabel;
};

using DescriptionMutator =
    std::function<bool(tinyxml2::XMLDocument &document)>;

// Canonicalizes a parsed GDTF description.xml document in memory.
Result CanonicalizeDescription(tinyxml2::XMLDocument &doc,
                               const Options &options = {});

// Validates a parsed GDTF description.xml document against Perastage export rules.
Result ValidateDescription(const tinyxml2::XMLDocument &doc,
                           const Options &options = {});

// Canonicalizes a GDTF archive into another archive path.
Result CanonicalizeArchive(const std::filesystem::path &sourcePath,
                           const std::filesystem::path &destinationPath,
                           const Options &options = {});

// Rewrites an archive after applying a caller-owned description mutation.
Result RewriteArchiveDescription(const std::filesystem::path &sourcePath,
                                 const std::filesystem::path &destinationPath,
                                 const DescriptionMutator &mutator);

// Validates a GDTF archive against Perastage export rules.
Result ValidateArchive(const std::filesystem::path &sourcePath,
                       const Options &options = {});

// Returns true when the value is a placeholder FixtureTypeID rejected for export.
bool IsPlaceholderFixtureTypeId(const std::string &value);

} // namespace GdtfCanonicalizer
