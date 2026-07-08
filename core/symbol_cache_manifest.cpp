#include "symbol_cache_manifest.h"
#include "filesystem_path_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "json.hpp"

namespace symbol_cache {
namespace {

// Finds the manifest entry that belongs to the requested fixture key.
const FixtureSymbolCacheEntry *FindEntry(
    const std::vector<FixtureSymbolCacheEntry> &entries,
    const std::string &fixtureKey) {
  for (const auto &entry : entries) {
    if (entry.fixtureKey == fixtureKey)
      return &entry;
  }
  return nullptr;
}

// Finds the mutable manifest entry that belongs to the requested fixture key.
FixtureSymbolCacheEntry *FindMutableEntry(
    std::vector<FixtureSymbolCacheEntry> &entries,
    const std::string &fixtureKey) {
  for (auto &entry : entries) {
    if (entry.fixtureKey == fixtureKey)
      return &entry;
  }
  return nullptr;
}

// Reads the active ZIP entry into a UTF-8 string buffer.
bool ReadZipEntryText(wxZipInputStream &zip, std::string &out) {
  out.clear();
  std::array<char, 4096> buffer{};
  while (true) {
    zip.Read(buffer.data(), buffer.size());
    const size_t count = zip.LastRead();
    if (count == 0)
      break;
    out.append(buffer.data(), count);
  }
  return true;
}

// Converts a view list JSON array into a stable set of view names.
std::set<std::string> ParseViews(const nlohmann::json &value) {
  std::set<std::string> views;
  if (!value.is_array())
    return views;
  for (const auto &view : value) {
    if (view.is_string())
      views.insert(view.get<std::string>());
  }
  return views;
}

// Serializes a stable set of view names as a JSON array.
nlohmann::json SerializeViews(const std::set<std::string> &views) {
  nlohmann::json output = nlohmann::json::array();
  for (const std::string &view : views)
    output.push_back(view);
  return output;
}

} // namespace

// Clears all manifest metadata and returns the service to an empty state.
void SymbolCacheManifest::Clear() {
  manifestFormatVersion = kCurrentManifestFormatVersion;
  perastageSymbolFormatVersion = kCurrentPerastageSymbolFormatVersion;
  loadedManifest = false;
  entries.clear();
}

// Reports whether a manifest was loaded from the current project package.
bool SymbolCacheManifest::HasLoadedManifest() const { return loadedManifest; }

// Reports whether the loaded manifest format can be interpreted safely.
bool SymbolCacheManifest::IsManifestFormatKnown() const {
  return manifestFormatVersion == kCurrentManifestFormatVersion;
}

// Validates whether a fixture can safely skip deep GDTF symbol inspection.
ValidationResult SymbolCacheManifest::ValidateFixture(
    const ValidationRequest &request) const {
  if (request.bypassManifest)
    return {ValidationStatus::Bypassed, false,
            "manifest bypass requested for explicit symbol regeneration"};
  if (!loadedManifest)
    return {ValidationStatus::MissingManifest, false,
            "symbol cache manifest is missing"};
  if (!IsManifestFormatKnown())
    return {ValidationStatus::UnknownManifestVersion, false,
            "symbol cache manifest format is unknown"};

  const FixtureSymbolCacheEntry *entry = FindEntry(entries, request.fixtureKey);
  if (!entry)
    return {ValidationStatus::MissingEntry, false,
            "fixture type is not present in the symbol cache manifest"};
  if (perastageSymbolFormatVersion < kCurrentPerastageSymbolFormatVersion)
    return {ValidationStatus::OutdatedSymbolFormat, false,
            "manifest symbol format version is older than the supported format"};
  if (entry->gdtfSpec != request.gdtfSpec)
    return {ValidationStatus::GdtfSpecChanged, false,
            "fixture GDTF path/spec changed since the manifest entry was written"};
  if (entry->gdtfContentHash != request.gdtfContentHash)
    return {ValidationStatus::GdtfHashChanged, false,
            "fixture GDTF content hash changed since the manifest entry was written"};
  if (!entry->hasPerastageSymbols)
    return {ValidationStatus::SymbolsNotMarkedAvailable, false,
            "manifest entry does not mark Perastage symbols as available"};

  for (const std::string &view : request.requiredViews) {
    if (entry->availableViews.find(view) == entry->availableViews.end()) {
      return {ValidationStatus::MissingRequiredViews, false,
              "manifest entry does not list every required symbol view"};
    }
  }

  return {ValidationStatus::Valid, true,
          "manifest entry matches the current fixture GDTF and required views"};
}

// Records that a fixture type has valid Perastage-generated symbols in its GDTF.
void SymbolCacheManifest::MarkFixtureSymbolsValid(
    const ValidationRequest &request, std::string timestampUtc) {
  if (timestampUtc.empty())
    timestampUtc = CurrentUtcTimestamp();

  loadedManifest = true;
  manifestFormatVersion = kCurrentManifestFormatVersion;
  perastageSymbolFormatVersion = kCurrentPerastageSymbolFormatVersion;

  FixtureSymbolCacheEntry *entry = FindMutableEntry(entries, request.fixtureKey);
  if (!entry) {
    entries.push_back({});
    entry = &entries.back();
  }

  entry->fixtureKey = request.fixtureKey;
  entry->fixtureTypeName = request.fixtureTypeName;
  entry->gdtfSpec = request.gdtfSpec;
  entry->gdtfContentHash = request.gdtfContentHash;
  entry->hasPerastageSymbols = true;
  entry->availableViews = request.requiredViews;
  entry->lastGenerationTimestampUtc = std::move(timestampUtc);
}

// Loads manifest metadata from serialized JSON text without reading SVG data.
bool SymbolCacheManifest::LoadFromJsonText(const std::string &jsonText,
                                           std::string &errorMessage) {
  Clear();
  nlohmann::json root = nlohmann::json::parse(jsonText, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    errorMessage = "Symbol cache manifest JSON is invalid.";
    return false;
  }

  manifestFormatVersion = root.value("manifestFormatVersion", 0);
  perastageSymbolFormatVersion = root.value("perastageSymbolFormatVersion", 0);
  loadedManifest = true;

  const nlohmann::json entriesJson =
      root.value("fixtures", nlohmann::json::array());
  if (entriesJson.is_array()) {
    for (const auto &item : entriesJson) {
      if (!item.is_object())
        continue;
      FixtureSymbolCacheEntry entry;
      entry.fixtureKey = item.value("fixtureKey", std::string{});
      entry.fixtureTypeName = item.value("fixtureTypeName", std::string{});
      entry.gdtfSpec = item.value("gdtfSpec", std::string{});
      entry.gdtfContentHash = item.value("gdtfContentHash", std::string{});
      entry.hasPerastageSymbols = item.value("hasPerastageSymbols", false);
      entry.availableViews =
          ParseViews(item.value("availableViews", nlohmann::json::array()));
      entry.lastGenerationTimestampUtc =
          item.value("lastGenerationTimestampUtc", std::string{});
      if (!entry.fixtureKey.empty())
        entries.push_back(std::move(entry));
    }
  }

  return true;
}

// Serializes manifest metadata to JSON text suitable for project archive storage.
bool SymbolCacheManifest::SaveToJsonText(std::string &jsonText,
                                         std::string &errorMessage) const {
  nlohmann::json root = nlohmann::json::object();
  root["manifestFormatVersion"] = kCurrentManifestFormatVersion;
  root["perastageSymbolFormatVersion"] = kCurrentPerastageSymbolFormatVersion;
  root["fixtures"] = nlohmann::json::array();

  for (const auto &entry : entries) {
    if (entry.fixtureKey.empty())
      continue;
    root["fixtures"].push_back(nlohmann::json{
        {"fixtureKey", entry.fixtureKey},
        {"fixtureTypeName", entry.fixtureTypeName},
        {"gdtfSpec", entry.gdtfSpec},
        {"gdtfContentHash", entry.gdtfContentHash},
        {"hasPerastageSymbols", entry.hasPerastageSymbols},
        {"availableViews", SerializeViews(entry.availableViews)},
        {"lastGenerationTimestampUtc", entry.lastGenerationTimestampUtc}});
  }

  try {
    jsonText = root.dump(2);
  } catch (const std::exception &ex) {
    errorMessage = ex.what();
    return false;
  }
  return true;
}

// Converts a UTF-8 filesystem path into a wxString using the native platform representation.
wxString WxStringFromUtf8Path(const std::string &path) {
  const std::filesystem::path nativePath = PathUtils::PathFromUtf8(path);
#ifdef _WIN32
  return wxString(nativePath.wstring());
#else
  return wxString::FromUTF8(nativePath.string());
#endif
}

// Loads the project-level manifest entry from a .pstg ZIP archive when present.
bool SymbolCacheManifest::LoadFromProjectArchive(const std::string &projectPath,
                                                 std::string &errorMessage) {
  Clear();
  wxFileInputStream input(WxStringFromUtf8Path(projectPath));
  if (!input.IsOk()) {
    errorMessage = "Could not open project archive for symbol cache manifest.";
    return false;
  }

  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    if (entry->GetName().ToStdString() != kProjectArchiveEntryName)
      continue;
    std::string jsonText;
    if (!ReadZipEntryText(zip, jsonText)) {
      errorMessage = "Could not read symbol cache manifest from project archive.";
      return false;
    }
    return LoadFromJsonText(jsonText, errorMessage);
  }

  return true;
}

// Creates an optional archive resource for persisting the manifest in a .pstg file.
std::optional<ManifestArchiveResource> SymbolCacheManifest::ToArchiveResource(
    std::string &errorMessage) const {
  if (entries.empty())
    return std::nullopt;

  std::string jsonText;
  if (!SaveToJsonText(jsonText, errorMessage))
    return std::nullopt;

  ManifestArchiveResource resource;
  resource.entryName = kProjectArchiveEntryName;
  resource.bytes.assign(jsonText.begin(), jsonText.end());
  return resource;
}

// Returns the immutable list of fixture entries currently held by the manifest.
const std::vector<FixtureSymbolCacheEntry> &SymbolCacheManifest::Entries() const {
  return entries;
}

// Normalizes a ZIP entry path for deterministic GDTF semantic fingerprinting.
static std::string NormalizeGdtfEntryPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (!path.empty() && path.front() == '/')
    path.erase(path.begin());
  std::string normalized;
  bool previousSlash = false;
  for (char ch : path) {
    if (ch == '/') {
      if (!previousSlash)
        normalized.push_back('/');
      previousSlash = true;
    } else {
      normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      previousSlash = false;
    }
  }
  return normalized;
}

// Reports whether a normalized GDTF entry affects generated Perastage symbols.
static bool IsSymbolRelevantGdtfEntry(const std::string &path) {
  if (path == "description.xml")
    return true;
  constexpr std::array<const char *, 10> prefixes = {
      "models/svg/",      "models/svg_front/", "models/svg_side/",
      "models/gltf_low/", "models/gltf/",      "models/gltf_high/",
      "models/glb/",      "models/3ds_low/",   "models/3ds/",
      "models/3ds_high/"};
  for (const char *prefix : prefixes) {
    if (path.rfind(prefix, 0) == 0)
      return true;
  }
  return false;
}

// Updates an FNV-1a hash with raw bytes.
static void UpdateFnv1a(std::uint64_t &hash, const void *data, size_t size) {
  const auto *bytes = static_cast<const unsigned char *>(data);
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ull;
  }
}

// Computes a versioned semantic fingerprint over symbol-relevant uncompressed GDTF entries.
std::string ComputeGdtfSemanticFingerprint(const std::string &path,
                                           std::string &errorMessage) {
  wxFileInputStream input(WxStringFromUtf8Path(path));
  if (!input.IsOk()) {
    errorMessage = "Could not open GDTF archive for semantic fingerprinting.";
    return {};
  }

  wxZipInputStream zip(input);
  if (!zip.IsOk()) {
    errorMessage = "Could not read GDTF archive for semantic fingerprinting.";
    return {};
  }

  std::map<std::string, std::vector<unsigned char>> entries;
  while (std::unique_ptr<wxZipEntry> entry(zip.GetNextEntry())) {
    if (entry->IsDir())
      continue;
    const std::string normalized = NormalizeGdtfEntryPath(entry->GetName().ToStdString());
    if (!IsSymbolRelevantGdtfEntry(normalized))
      continue;

    std::vector<unsigned char> bytes;
    std::array<char, 8192> buffer{};
    while (true) {
      zip.Read(buffer.data(), buffer.size());
      const size_t count = zip.LastRead();
      if (count == 0)
        break;
      const auto *begin = reinterpret_cast<const unsigned char *>(buffer.data());
      bytes.insert(bytes.end(), begin, begin + count);
    }
    entries[normalized] = std::move(bytes);
  }

  if (entries.find("description.xml") == entries.end()) {
    errorMessage = "GDTF semantic fingerprint requires description.xml.";
    return {};
  }

  std::uint64_t hash = 1469598103934665603ull;
  std::uint64_t payloadSize = 0;
  constexpr char version[] = "gdtf-symbol-fnv1a64-v1\n";
  UpdateFnv1a(hash, version, sizeof(version) - 1);
  for (const auto &[name, bytes] : entries) {
    UpdateFnv1a(hash, name.data(), name.size());
    const char separator = '\0';
    UpdateFnv1a(hash, &separator, 1);
    if (!bytes.empty())
      UpdateFnv1a(hash, bytes.data(), bytes.size());
    UpdateFnv1a(hash, &separator, 1);
    payloadSize += static_cast<std::uint64_t>(name.size() + bytes.size());
  }

  std::ostringstream out;
  out << "gdtfsymfnv1a64v1:" << std::hex << std::setw(16) << std::setfill('0')
      << hash << ":" << std::dec << entries.size() << ":" << payloadSize;
  return out.str();
}

// Computes the current semantic GDTF content fingerprint used by symbol cache validation.
std::string ComputeFileContentHash(const std::string &path,
                                   std::string &errorMessage) {
  return ComputeGdtfSemanticFingerprint(path, errorMessage);
}

// Returns the complete set of views required before manifest validation can skip inspection.
std::set<std::string> RequiredPerastageSymbolViews() {
  return {"top", "bottom", "front", "side"};
}

// Returns the current UTC timestamp in a compact ISO-8601 format.
std::string CurrentUtcTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &nowTime);
#else
  gmtime_r(&nowTime, &utc);
#endif
  std::ostringstream out;
  out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

// Returns a stable string name for a manifest validation status.
const char *ValidationStatusName(ValidationStatus status) {
  switch (status) {
  case ValidationStatus::Valid:
    return "valid";
  case ValidationStatus::Bypassed:
    return "bypassed";
  case ValidationStatus::MissingManifest:
    return "missing_manifest";
  case ValidationStatus::UnknownManifestVersion:
    return "unknown_manifest_version";
  case ValidationStatus::MissingEntry:
    return "missing_entry";
  case ValidationStatus::OutdatedSymbolFormat:
    return "outdated_symbol_format";
  case ValidationStatus::GdtfSpecChanged:
    return "gdtf_spec_changed";
  case ValidationStatus::GdtfHashChanged:
    return "gdtf_hash_changed";
  case ValidationStatus::MissingRequiredViews:
    return "missing_required_views";
  case ValidationStatus::SymbolsNotMarkedAvailable:
    return "symbols_not_marked_available";
  }
  return "unknown";
}

} // namespace symbol_cache
