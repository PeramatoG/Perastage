#include "gdtf_canonicalizer.h"

#include "gdtf_mutation_audit.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <functional>
#include <memory>
#include <sstream>
#include <unordered_set>

#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

namespace GdtfCanonicalizer {
namespace {

constexpr const char *kCanonicalizationRevisionText =
    "Canonicalized GDTF structure for Perastage export";
constexpr std::array<const char *, 9> kFixtureTypeChildOrder = {
    "AttributeDefinitions", "Wheels", "PhysicalDescriptions", "Models",
    "Geometries", "DMXModes", "Revisions", "FTPresets", "Protocols"};

struct ZipEntryData {
  std::string name;
  std::string bytes;
};

// Returns a lowercase copy for case-insensitive comparisons.
std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

// Normalizes ZIP entry separators and removes leading relative prefixes.
std::string NormalizeArchivePath(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  while (value.rfind("./", 0) == 0)
    value.erase(0, 2);
  while (!value.empty() && value.front() == '/')
    value.erase(value.begin());
  return value;
}

// Returns the filename portion of a normalized archive entry path.
std::string ArchiveFileName(const std::string &value) {
  const std::string normalized = NormalizeArchivePath(value);
  const size_t slash = normalized.find_last_of('/');
  return slash == std::string::npos ? normalized : normalized.substr(slash + 1);
}

// Reads the current ZIP entry into memory.
bool ReadCurrentZipEntry(wxZipInputStream &zip, std::string &out) {
  out.clear();
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t read = zip.LastRead();
    if (read == 0)
      break;
    out.append(buffer, read);
  }
  return true;
}

// Reads all non-directory entries from a ZIP archive.
bool ReadZipEntries(const fs::path &path, std::vector<ZipEntryData> &entries,
                    Result &result) {
  wxFileInputStream input(path.string());
  if (!input.IsOk()) {
    result.errors.push_back("Could not open GDTF archive: " + path.string());
    return false;
  }
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    ZipEntryData data;
    data.name = NormalizeArchivePath(entry->GetName().ToStdString());
    ReadCurrentZipEntry(zip, data.bytes);
    entries.push_back(std::move(data));
  }
  return true;
}

// Writes all entries to a ZIP archive in stable path order.
bool WriteZipEntries(const fs::path &path, std::vector<ZipEntryData> entries,
                     Result &result) {
  std::stable_sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
    if (a.name == "description.xml")
      return b.name != "description.xml";
    if (b.name == "description.xml")
      return false;
    return a.name < b.name;
  });

  wxFileOutputStream output(path.string());
  if (!output.IsOk()) {
    result.errors.push_back("Could not create canonical GDTF archive: " +
                            path.string());
    return false;
  }
  wxZipOutputStream zip(output);
  for (const ZipEntryData &data : entries) {
    auto *entry = new wxZipEntry(data.name);
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(entry);
    if (!data.bytes.empty())
      zip.Write(data.bytes.data(), data.bytes.size());
    zip.CloseEntry();
  }
  zip.Close();
  return true;
}

// Checks whether a string is a valid GUID token.
bool IsValidGuid(const std::string &value) {
  if (value.size() != 36)
    return false;
  for (size_t i = 0; i < value.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (value[i] != '-')
        return false;
    } else if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
      return false;
    }
  }
  return true;
}

// Appends a diagnostic with optional source context.
void AddError(Result &result, const Options &options, const std::string &message) {
  if (options.sourceLabel.empty())
    result.errors.push_back(message);
  else
    result.errors.push_back(options.sourceLabel + ": " + message);
}

// Builds a deterministic UUID-like value from stable FixtureType data.
std::string BuildStableFixtureTypeId(const tinyxml2::XMLElement *fixtureType,
                                     const Options &options) {
  std::ostringstream seed;
  if (!options.stableIdSeed.empty())
    seed << options.stableIdSeed << '|';
  if (fixtureType) {
    for (const char *attr : {"Manufacturer", "Name", "ShortName", "LongName", "Description"}) {
      if (const char *value = fixtureType->Attribute(attr))
        seed << attr << '=' << value << '|';
    }
  }
  const std::string text = seed.str().empty() ? "Perastage GDTF" : seed.str();
  const uint64_t h1 = std::hash<std::string>{}(text);
  const uint64_t h2 = std::hash<std::string>{}("perastage:" + text);
  char buffer[37];
  std::snprintf(buffer, sizeof(buffer), "%08x-%04x-%04x-%04x-%012llx",
                static_cast<unsigned>(h1 >> 32), static_cast<unsigned>((h1 >> 16) & 0xffff),
                static_cast<unsigned>((h1 & 0x0fff) | 0x5000),
                static_cast<unsigned>(((h2 >> 48) & 0x3fff) | 0x8000),
                static_cast<unsigned long long>(h2 & 0xffffffffffffULL));
  return std::string(buffer);
}

// Returns true when a FixtureType child name is standard.
bool IsStandardFixtureTypeChild(const char *name) {
  return std::find(kFixtureTypeChildOrder.begin(), kFixtureTypeChildOrder.end(),
                   std::string(name ? name : "")) != kFixtureTypeChildOrder.end();
}

// Returns the official order index for a FixtureType child.
int OrderIndex(const char *name) {
  for (size_t i = 0; i < kFixtureTypeChildOrder.size(); ++i) {
    if (std::string(name ? name : "") == kFixtureTypeChildOrder[i])
      return static_cast<int>(i);
  }
  return -1;
}

// Serializes XML to a string for change detection.
std::string PrintDocument(tinyxml2::XMLDocument &doc) {
  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  return printer.CStr();
}

// Removes non-standard FixtureType children and reports the mutation.
bool RemoveUnknownFixtureTypeChildren(tinyxml2::XMLElement *fixtureType) {
  bool changed = false;
  for (tinyxml2::XMLNode *node = fixtureType->FirstChild(); node;) {
    tinyxml2::XMLNode *next = node->NextSibling();
    tinyxml2::XMLElement *element = node->ToElement();
    if (element && !IsStandardFixtureTypeChild(element->Name())) {
      fixtureType->DeleteChild(node);
      changed = true;
    }
    node = next;
  }
  return changed;
}

// Reorders FixtureType children to the official GDTF order.
bool ReorderFixtureTypeChildren(tinyxml2::XMLElement *fixtureType) {
  std::vector<tinyxml2::XMLNode *> ordered;
  int lastIndex = -1;
  bool changed = false;
  for (tinyxml2::XMLElement *child = fixtureType->FirstChildElement(); child;
       child = child->NextSiblingElement()) {
    const int index = OrderIndex(child->Name());
    if (index < lastIndex)
      changed = true;
    lastIndex = std::max(lastIndex, index);
  }
  if (!changed)
    return false;
  for (const char *name : kFixtureTypeChildOrder) {
    for (tinyxml2::XMLElement *child = fixtureType->FirstChildElement(name); child;
         child = child->NextSiblingElement(name))
      ordered.push_back(child);
  }
  tinyxml2::XMLDocument *doc = fixtureType->GetDocument();
  std::vector<tinyxml2::XMLNode *> cloned;
  cloned.reserve(ordered.size());
  for (tinyxml2::XMLNode *node : ordered)
    cloned.push_back(node->DeepClone(doc));

  for (tinyxml2::XMLNode *node = fixtureType->FirstChild(); node;) {
    tinyxml2::XMLNode *next = node->NextSibling();
    if (node->ToElement())
      fixtureType->DeleteChild(node);
    node = next;
  }

  for (tinyxml2::XMLNode *node : cloned)
    fixtureType->InsertEndChild(node);
  return true;
}

// Finds the standard FixtureType node in a GDTF document.
tinyxml2::XMLElement *FindFixtureType(tinyxml2::XMLDocument &doc) {
  tinyxml2::XMLElement *root = doc.FirstChildElement("GDTF");
  return root ? root->FirstChildElement("FixtureType") : nullptr;
}

// Validates root and FixtureType structure common to all export paths.
Result ValidateDocumentStructure(const tinyxml2::XMLDocument &doc,
                                 const Options &options) {
  Result result;
  result.success = true;
  const tinyxml2::XMLElement *root = doc.FirstChildElement("GDTF");
  if (!root) {
    AddError(result, options, "missing GDTF root node");
  } else if (!root->Attribute("DataVersion")) {
    AddError(result, options, "GDTF root is missing DataVersion");
  }
  const tinyxml2::XMLElement *fixtureType = root ? root->FirstChildElement("FixtureType") : nullptr;
  if (!fixtureType) {
    AddError(result, options, "missing FixtureType node");
  } else {
    const char *id = fixtureType->Attribute("FixtureTypeID");
    if (!id || !IsValidGuid(id) || IsPlaceholderFixtureTypeId(id))
      AddError(result, options, "FixtureTypeID is missing, invalid, or a placeholder");

    int last = -1;
    for (const tinyxml2::XMLElement *child = fixtureType->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
      const int index = OrderIndex(child->Name());
      if (index < 0)
        AddError(result, options, std::string("unknown FixtureType child node: ") + child->Name());
      else if (index < last)
        AddError(result, options, "FixtureType children are not in official GDTF order");
      last = std::max(last, index);
    }
    if (!fixtureType->FirstChildElement("AttributeDefinitions"))
      AddError(result, options, "missing required FixtureType/AttributeDefinitions section");
    if (!fixtureType->FirstChildElement("Geometries"))
      AddError(result, options, "missing required FixtureType/Geometries section");
    if (!fixtureType->FirstChildElement("DMXModes"))
      AddError(result, options, "missing required FixtureType/DMXModes section");
  }
  result.success = result.errors.empty();
  return result;
}

} // namespace

// Returns true when the FixtureTypeID is a known non-unique placeholder.
bool IsPlaceholderFixtureTypeId(const std::string &value) {
  const std::string lower = Lower(value);
  return lower.empty() || lower == "00000000-0000-0000-0000-000000000000" ||
         lower == "00000000-0000-0000-0000-000000000001";
}

// Canonicalizes a parsed GDTF description.xml document in memory.
Result CanonicalizeDescription(tinyxml2::XMLDocument &doc, const Options &options) {
  Result result;
  const std::string before = PrintDocument(doc);
  tinyxml2::XMLElement *root = doc.FirstChildElement("GDTF");
  if (!root) {
    AddError(result, options, "missing GDTF root node");
    return result;
  }
  if (!root->Attribute("DataVersion")) {
    root->SetAttribute("DataVersion", "1.2");
    result.changed = true;
  }
  tinyxml2::XMLElement *fixtureType = FindFixtureType(doc);
  if (!fixtureType) {
    AddError(result, options, "missing FixtureType node");
    return result;
  }

  const char *id = fixtureType->Attribute("FixtureTypeID");
  if (!id || !IsValidGuid(id) || IsPlaceholderFixtureTypeId(id)) {
    if (!options.allowFixtureTypeIdRepair) {
      AddError(result, options, "FixtureTypeID is missing, invalid, or a placeholder");
      return result;
    }
    fixtureType->SetAttribute("FixtureTypeID", BuildStableFixtureTypeId(fixtureType, options).c_str());
    result.changed = true;
  }

  result.changed = RemoveUnknownFixtureTypeChildren(fixtureType) || result.changed;
  result.changed = ReorderFixtureTypeChildren(fixtureType) || result.changed;

  const std::string afterStructure = PrintDocument(doc);
  if (result.changed || before != afterStructure) {
    GdtfMutationAudit::AppendRevision(fixtureType, doc, kCanonicalizationRevisionText,
                                      GdtfMutationAudit::BuildPerastageModifiedBy());
    ReorderFixtureTypeChildren(fixtureType);
  }
  result.changed = result.changed || before != PrintDocument(doc);

  Result validation = ValidateDocumentStructure(doc, options);
  result.errors.insert(result.errors.end(), validation.errors.begin(), validation.errors.end());
  result.warnings.insert(result.warnings.end(), validation.warnings.begin(), validation.warnings.end());
  result.success = result.errors.empty();
  return result;
}

// Validates a parsed GDTF description.xml document against export rules.
Result ValidateDescription(const tinyxml2::XMLDocument &doc, const Options &options) {
  return ValidateDocumentStructure(doc, options);
}

// Canonicalizes a GDTF archive into another archive path.
Result CanonicalizeArchive(const fs::path &sourcePath, const fs::path &destinationPath,
                           const Options &options) {
  Result result;
  std::vector<ZipEntryData> entries;
  if (!ReadZipEntries(sourcePath, entries, result))
    return result;
  auto descIt = std::find_if(entries.begin(), entries.end(), [](const ZipEntryData &entry) {
    return Lower(ArchiveFileName(entry.name)) == "description.xml";
  });
  if (descIt == entries.end()) {
    AddError(result, options, "missing description.xml in GDTF archive");
    return result;
  }
  descIt->name = "description.xml";
  tinyxml2::XMLDocument doc;
  if (doc.Parse(descIt->bytes.c_str(), descIt->bytes.size()) != tinyxml2::XML_SUCCESS) {
    AddError(result, options, "unreadable description.xml");
    return result;
  }
  Result xmlResult = CanonicalizeDescription(doc, options);
  result.changed = xmlResult.changed;
  result.warnings = xmlResult.warnings;
  result.errors = xmlResult.errors;
  if (!xmlResult.success)
    return result;
  descIt->bytes = PrintDocument(doc);
  if (!WriteZipEntries(destinationPath, entries, result))
    return result;
  result.success = result.errors.empty();
  return result;
}

// Validates a GDTF archive against export rules.
Result ValidateArchive(const fs::path &sourcePath, const Options &options) {
  Result result;
  std::vector<ZipEntryData> entries;
  if (!ReadZipEntries(sourcePath, entries, result))
    return result;
  auto descIt = std::find_if(entries.begin(), entries.end(), [](const ZipEntryData &entry) {
    return Lower(ArchiveFileName(entry.name)) == "description.xml";
  });
  if (descIt == entries.end()) {
    AddError(result, options, "missing description.xml in GDTF archive");
    return result;
  }
  tinyxml2::XMLDocument doc;
  if (doc.Parse(descIt->bytes.c_str(), descIt->bytes.size()) != tinyxml2::XML_SUCCESS) {
    AddError(result, options, "unreadable description.xml");
    return result;
  }
  return ValidateDescription(doc, options);
}

} // namespace GdtfCanonicalizer
