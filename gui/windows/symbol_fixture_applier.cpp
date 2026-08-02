#include "windows/symbol_fixture_applier.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <utility>
#include <vector>

#include <tinyxml2.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "fixtures/fixture_gdtf_resolution.h"
#include "filesystem_path_utils.h"
#include "guiconfigservices.h"
#include "gdtfdictionary.h"
#include "gdtf_mutation_audit.h"
#include "gdtf_canonicalizer.h"
#include "symbol_cache_manifest.h"
#include "windows/symbol_preview_exporter.h"

namespace fs = std::filesystem;

namespace symbol_preview {
namespace {

struct GdtfRewriteResult {
  bool success = false;
  fs::path finalArchivePath;
  bool atomicReplacementCompleted = false;
  std::set<std::string> normalizedGeneratedSvgPaths;
  bool requiredPathsConfirmed = false;
  std::string finalSemanticFingerprint;
  std::uintmax_t finalFileSize = 0;
  fs::file_time_type finalModificationTime{};
  bool externalVerificationRequired = false;
  std::string diagnostic;
};

struct SymbolPayload {
  std::string archivePath;
  std::string svg;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
};

std::string NormalizeArchivePath(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  while (value.rfind("./", 0) == 0)
    value.erase(0, 2);
  while (!value.empty() && value.front() == '/')
    value.erase(value.begin());
  return value;
}

std::string ToLowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

bool IsPerastageEditorValue(const char *editorValue) {
  if (!editorValue)
    return false;
  const std::string editor = editorValue;
  return editor == "Perastage" || editor.rfind("Perastage ", 0) == 0;
}

// Resolves the FixtureType element from a parsed GDTF description document.
tinyxml2::XMLElement *ResolveFixtureType(tinyxml2::XMLDocument &doc) {
  tinyxml2::XMLElement *fixtureType = doc.FirstChildElement("GDTF");
  if (fixtureType)
    fixtureType = fixtureType->FirstChildElement("FixtureType");
  if (!fixtureType)
    fixtureType = doc.FirstChildElement("FixtureType");
  return fixtureType;
}

// Resolves the preferred GDTF model using Main first and the first model as fallback.
tinyxml2::XMLElement *ResolvePreferredModel(tinyxml2::XMLElement *fixtureType) {
  if (!fixtureType)
    return nullptr;
  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (!models)
    return nullptr;
  tinyxml2::XMLElement *targetModel = nullptr;
  for (tinyxml2::XMLElement *model = models->FirstChildElement("Model"); model;
       model = model->NextSiblingElement("Model")) {
    const char *name = model->Attribute("Name");
    if (name && std::string(name) == "Main")
      return model;
    if (!targetModel)
      targetModel = model;
  }
  return targetModel;
}

// Resolves the model SVG basename from File, Name, or the standard main fallback.
std::string ResolveModelSvgBasenameFromFixtureType(tinyxml2::XMLElement *fixtureType,
                                                   std::string &errorMessage) {
  if (!fixtureType) {
    errorMessage = "Could not find FixtureType node in description.xml.";
    return {};
  }
  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (!models) {
    errorMessage = "Could not find Models node in description.xml.";
    return {};
  }
  tinyxml2::XMLElement *targetModel = ResolvePreferredModel(fixtureType);
  if (!targetModel) {
    errorMessage = "Could not find any Model node in description.xml.";
    return {};
  }
  const char *fileAttr = targetModel->Attribute("File");
  if (fileAttr && std::string(fileAttr).size() > 0)
    return std::string(fileAttr);
  const char *nameAttr = targetModel->Attribute("Name");
  if (nameAttr && std::string(nameAttr).size() > 0)
    return std::string(nameAttr);
  return "main";
}

// Builds the normalized SVG paths required for a complete Perastage symbol set.
std::unordered_set<std::string> BuildRequiredSymbolPaths(const std::string &modelSvgBase) {
  return {NormalizeArchivePath("models/svg/" + modelSvgBase + ".svg"),
          NormalizeArchivePath("models/svg/" + modelSvgBase + "_bottom.svg"),
          NormalizeArchivePath("models/svg_side/" + modelSvgBase + ".svg"),
          NormalizeArchivePath("models/svg_front/" + modelSvgBase + ".svg")};
}

// Returns whether every required SVG path is present in the scanned archive entries.
bool HasRequiredSymbolPaths(const std::unordered_set<std::string> &archiveEntries,
                            const std::unordered_set<std::string> &requiredPaths) {
  for (const std::string &path : requiredPaths) {
    if (archiveEntries.find(path) == archiveEntries.end())
      return false;
  }
  return true;
}

bool IsPerastageModifiedByValue(const char *modifiedByValue) {
  if (!modifiedByValue)
    return false;
  const std::string modifiedBy = modifiedByValue;
  return modifiedBy == "Perastage" || modifiedBy.rfind("Perastage ", 0) == 0;
}

bool HasPerastageRevisionModifiedBy(const tinyxml2::XMLElement *fixtureType) {
  if (!fixtureType)
    return false;
  const tinyxml2::XMLElement *revisions = fixtureType->FirstChildElement("Revisions");
  if (!revisions)
    return false;
  for (const tinyxml2::XMLElement *revision =
           revisions->FirstChildElement("Revision");
       revision; revision = revision->NextSiblingElement("Revision")) {
    if (IsPerastageModifiedByValue(revision->Attribute("ModifiedBy")))
      return true;
  }
  return false;
}

bool IsDescriptionXmlPath(const std::string &archivePath) {
  const std::string normalized = ToLowerCopy(NormalizeArchivePath(archivePath));
  return normalized == "description.xml" ||
         normalized.size() > sizeof("/description.xml") - 1 &&
             normalized.rfind("/description.xml") ==
                 normalized.size() - (sizeof("/description.xml") - 1);
}

std::string BuildDescriptionMissingMessage(
    const fs::path &gdtfPath,
    const std::vector<std::string> &sampleEntries,
    bool foundCaseInsensitiveVariant) {
  std::ostringstream message;
  message << "Could not locate description.xml in GDTF archive: "
          << gdtfPath.string() << ".";
  if (foundCaseInsensitiveVariant) {
    message
        << " A case-insensitive match was found, but this loader requires the canonical "
           "name/path (description.xml).";
  }
  if (!sampleEntries.empty()) {
    message << " Sample archive entries: ";
    for (size_t i = 0; i < sampleEntries.size(); ++i) {
      if (i > 0)
        message << ", ";
      message << sampleEntries[i];
    }
    message << ".";
  }
  return message.str();
}


bool ReadAllBytes(wxZipInputStream &zip, std::string &out) {
  out.clear();
  char buffer[4096];
  while (true) {
    zip.Read(buffer, sizeof(buffer));
    const size_t count = zip.LastRead();
    if (count == 0)
      break;
    out.append(buffer, count);
  }
  return true;
}

std::string NormalizePathSeparators(std::string value) {
  std::replace(value.begin(), value.end(), '\\', '/');
  return value;
}

bool IsPathWithinDirectory(const fs::path &path, const fs::path &directory) {
  if (directory.empty())
    return false;

  std::error_code ec;
  const fs::path canonicalPath = fs::weakly_canonical(path, ec);
  if (ec)
    return false;

  ec.clear();
  const fs::path canonicalDir = fs::weakly_canonical(directory, ec);
  if (ec)
    return false;

  auto pathIt = canonicalPath.begin();
  auto dirIt = canonicalDir.begin();
  for (; dirIt != canonicalDir.end(); ++dirIt, ++pathIt) {
    if (pathIt == canonicalPath.end() || *pathIt != *dirIt)
      return false;
  }
  return true;
}

// Copies a fixture GDTF into the project fixtures folder using derivative naming.
bool EnsureSceneLocalGdtfCopy(const fs::path &sourcePath,
                              const fs::path &sceneBasePath,
                              Fixture &fixture,
                              std::string &scenePathOut,
                              std::string &errorMessage) {
  if (sourcePath.empty() || sceneBasePath.empty()) {
    errorMessage = "Could not prepare a writable fixture GDTF copy in the scene folder.";
    return false;
  }

  std::error_code ec;
  const fs::path sceneFixturesDir = sceneBasePath / "fixtures";
  fs::create_directories(sceneFixturesDir, ec);
  if (ec) {
    errorMessage = "Could not create scene fixtures directory for writable GDTF copy.";
    return false;
  }

  const fs::path targetPath = sceneFixturesDir /
      GdtfDictionary::BuildPerastageCanonicalGdtfFileName(sourcePath.string());
  fs::copy_file(sourcePath, targetPath, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    errorMessage = "Could not copy fixture GDTF into the scene folder.";
    return false;
  }

  ec.clear();
  const fs::path relativePath = fs::relative(targetPath, sceneBasePath, ec);
  if (ec) {
    errorMessage = "Could not compute scene-relative fixture GDTF path.";
    return false;
  }

  const std::string relativeSpec =
      NormalizePathSeparators(relativePath.string());
  fixture.gdtfSpec = relativeSpec;
  scenePathOut = targetPath.string();
  return true;
}


std::string ResolveModelSvgBasename(const fs::path &gdtfPath,
                                    std::string &errorMessage) {
  wxFileInputStream input(gdtfPath.string());
  if (!input.IsOk()) {
    errorMessage = "Could not open fixture GDTF file for reading.";
    return {};
  }

  wxZipInputStream zipInput(input);
  std::unique_ptr<wxZipEntry> entry;
  std::string descriptionXml;
  std::vector<std::string> sampleEntries;
  bool foundCaseInsensitiveVariant = false;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const std::string entryName = entry->GetName().ToStdString();
    const std::string normalizedEntry = NormalizeArchivePath(entryName);
    if (sampleEntries.size() < 5)
      sampleEntries.push_back(normalizedEntry);

    if (ToLowerCopy(normalizedEntry) == "description.xml")
      foundCaseInsensitiveVariant = true;

    if (!IsDescriptionXmlPath(normalizedEntry))
      continue;
    if (!ReadAllBytes(zipInput, descriptionXml)) {
      errorMessage = "Could not read description.xml from the GDTF file.";
      return {};
    }
    break;
  }

  if (descriptionXml.empty()) {
    errorMessage = BuildDescriptionMissingMessage(gdtfPath, sampleEntries,
                                                  foundCaseInsensitiveVariant);
    return {};
  }

  tinyxml2::XMLDocument doc;
  if (doc.Parse(descriptionXml.c_str(), descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    errorMessage = "Could not parse description.xml from the GDTF file.";
    return {};
  }

  tinyxml2::XMLElement *fixtureType = ResolveFixtureType(doc);
  return ResolveModelSvgBasenameFromFixtureType(fixtureType, errorMessage);
}

const symbols::Symbol2D *FindSymbol(const std::vector<symbols::Symbol2D> &symbols,
                                    symbols::SymbolView view) {
  for (const auto &symbol : symbols) {
    if (symbol.view == view)
      return &symbol;
  }
  return nullptr;
}

bool BuildSymbolPayload(const std::vector<symbols::Symbol2D> &symbols,
                        symbols::SymbolView view,
                        const std::string &archivePath,
                        SymbolPayload &out,
                        std::string &errorMessage) {
  const symbols::Symbol2D *symbol = FindSymbol(symbols, view);
  if (!symbol || !symbol->bounds.valid)
    return false;

  out.archivePath = NormalizeArchivePath(archivePath);
  out.offsetX = -symbol->bounds.min.x;
  out.offsetY = -symbol->bounds.min.y;
  if (!symbol_preview::ExportSymbolToSvgString(*symbol, out.svg, errorMessage))
    return false;

  return true;
}

bool PatchDescriptionXml(const std::string &xml,
                         const std::unordered_map<std::string, SymbolPayload> &payloads,
                         const std::string &topPath,
                         const std::string &sidePath,
                         const std::string &frontPath,
                         std::string &updatedXml,
                         std::string &errorMessage) {
  tinyxml2::XMLDocument doc;
  if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
    errorMessage = "Could not parse description.xml from the GDTF file.";
    return false;
  }

  tinyxml2::XMLElement *fixtureType = GdtfMutationAudit::EnsureFixtureType(doc);

  tinyxml2::XMLElement *models = fixtureType->FirstChildElement("Models");
  if (!models) {
    errorMessage = "Could not find Models node in description.xml.";
    return false;
  }

  tinyxml2::XMLElement *targetModel = nullptr;
  for (tinyxml2::XMLElement *model = models->FirstChildElement("Model"); model;
       model = model->NextSiblingElement("Model")) {
    const char *name = model->Attribute("Name");
    if (name && std::string(name) == "Main") {
      targetModel = model;
      break;
    }
    if (!targetModel)
      targetModel = model;
  }

  if (!targetModel) {
    errorMessage = "Could not find any Model node in description.xml.";
    return false;
  }

  auto setOffsets = [&](const char *entryPath, const char *xAttr,
                        const char *yAttr) {
    auto it = payloads.find(entryPath);
    if (it == payloads.end())
      return;
    targetModel->SetAttribute(xAttr, it->second.offsetX);
    targetModel->SetAttribute(yAttr, it->second.offsetY);
  };

  setOffsets(topPath.c_str(), "SVGOffsetX", "SVGOffsetY");
  setOffsets(sidePath.c_str(), "SVGSideOffsetX", "SVGSideOffsetY");
  setOffsets(frontPath.c_str(), "SVGFrontOffsetX", "SVGFrontOffsetY");

  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  updatedXml = printer.CStr();
  return true;
}

std::string BuildSymbolRevisionAction(
    const std::unordered_map<std::string, SymbolPayload> &payloads,
    const std::string &topPath,
    const std::string &sidePath,
    const std::string &frontPath,
    const std::string &bottomPath) {
  std::vector<std::string> appliedViews;
  auto appendIfApplied = [&](const std::string &path, const char *label) {
    if (payloads.find(path) != payloads.end())
      appliedViews.emplace_back(label);
  };

  appendIfApplied(topPath, "top");
  appendIfApplied(sidePath, "side");
  appendIfApplied(frontPath, "front");
  appendIfApplied(bottomPath, "bottom");

  std::ostringstream action;
  action << "Applied fixture SVG symbol views (";
  for (size_t i = 0; i < appliedViews.size(); ++i) {
    if (i > 0)
      action << ", ";
    action << appliedViews[i];
  }
  action << ")";
  return action.str();
}

bool AppendMutationAuditMetadata(std::string &descriptionXml,
                                 const std::unordered_map<std::string, SymbolPayload> &payloads,
                                 const std::string &topPath,
                                 const std::string &sidePath,
                                 const std::string &frontPath,
                                 const std::string &bottomPath,
                                 std::string &errorMessage) {
  tinyxml2::XMLDocument doc;
  if (doc.Parse(descriptionXml.c_str(), descriptionXml.size()) !=
      tinyxml2::XML_SUCCESS) {
    errorMessage = "Could not parse description.xml from the GDTF file.";
    return false;
  }

  tinyxml2::XMLElement *fixtureType = GdtfMutationAudit::EnsureFixtureType(doc);
  GdtfMutationAudit::AppendRevision(
      fixtureType, doc,
      BuildSymbolRevisionAction(payloads, topPath, sidePath, frontPath, bottomPath),
      GdtfMutationAudit::BuildPerastageModifiedBy());

  GdtfCanonicalizer::Options canonicalOptions;
  canonicalOptions.allowFixtureTypeIdRepair = true;
  canonicalOptions.stableIdSeed = topPath + sidePath + frontPath + bottomPath;
  const GdtfCanonicalizer::Result canonicalResult =
      GdtfCanonicalizer::CanonicalizeDescription(doc, canonicalOptions);
  if (!canonicalResult.success) {
    errorMessage = canonicalResult.errors.empty()
                       ? "Could not canonicalize description.xml."
                       : canonicalResult.errors.front();
    return false;
  }

  tinyxml2::XMLPrinter printer;
  doc.Print(&printer);
  descriptionXml = printer.CStr();
  return true;
}

bool VerifyArchiveEntries(const fs::path &archivePath,
                          const std::unordered_map<std::string, SymbolPayload> &payloads) {
  wxFileInputStream input(archivePath.string());
  if (!input.IsOk())
    return false;

  wxZipInputStream zipInput(input);
  std::unordered_map<std::string, bool> found;
  for (const auto &[path, payload] : payloads) {
    (void)payload;
    found[NormalizeArchivePath(path)] = false;
  }

  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    const std::string name = NormalizeArchivePath(entry->GetName().ToStdString());
    auto it = found.find(name);
    if (it != found.end())
      it->second = true;
  }

  for (const auto &[path, wasFound] : found) {
    (void)path;
    if (!wasFound)
      return false;
  }
  return true;
}

// Rewrites a fixture GDTF archive with generated SVG symbols and updated metadata.
GdtfRewriteResult RewriteGdtfWithProof(
    const fs::path &sourcePath,
    const std::unordered_map<std::string, SymbolPayload> &payloads,
    const std::string &topPath,
    const std::string &sidePath,
    const std::string &frontPath,
    const std::string &bottomPath,
    symbols::FixtureSymbolTimings *timings = nullptr) {
  symbols::ScopedFixtureSymbolPhase rewritePhase(
      timings, symbols::FixtureSymbolPhase::ArchiveRewrite);
  GdtfRewriteResult result;
  result.finalArchivePath = sourcePath;
  std::string errorMessage;
  std::vector<std::pair<std::string, std::string>> entries;
  std::vector<std::string> sampleEntries;
  bool foundCaseInsensitiveVariant = false;
  {
    wxFileInputStream input(sourcePath.string());
    if (!input.IsOk()) {
      errorMessage = "Could not open fixture GDTF file for reading.";
      result.diagnostic = errorMessage;
      return result;
    }

    wxZipInputStream zipInput(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zipInput.GetNextEntry())), entry) {
      const std::string name = entry->GetName().ToStdString();
      if (entry->IsDir())
        continue;

      const std::string normalizedName = NormalizeArchivePath(name);
      if (sampleEntries.size() < 5)
        sampleEntries.push_back(normalizedName);
      if (ToLowerCopy(normalizedName) == "description.xml")
        foundCaseInsensitiveVariant = true;

      std::string content;
      ReadAllBytes(zipInput, content);
      entries.emplace_back(name, std::move(content));
    }
  }

  auto descriptionIt = std::find_if(entries.begin(), entries.end(),
                                    [](const auto &entry) {
                                      return IsDescriptionXmlPath(entry.first);
                                    });
  if (descriptionIt == entries.end()) {
    errorMessage = BuildDescriptionMissingMessage(sourcePath, sampleEntries,
                                                  foundCaseInsensitiveVariant);
    result.diagnostic = errorMessage;
    return result;
  }

  std::string updatedDescription;
  if (!PatchDescriptionXml(descriptionIt->second, payloads, topPath, sidePath,
                           frontPath, updatedDescription, errorMessage)) {
    result.diagnostic = errorMessage;
    return result;
  }
  if (!AppendMutationAuditMetadata(updatedDescription, payloads, topPath, sidePath,
                                   frontPath, bottomPath, errorMessage)) {
    result.diagnostic = errorMessage;
    return result;
  }

  descriptionIt->second = std::move(updatedDescription);
  for (const auto &[path, payload] : payloads) {
    const std::string normalizedPath = NormalizeArchivePath(path);
    auto existing = std::find_if(entries.begin(), entries.end(),
                                 [&](const auto &entry) {
                                   return NormalizeArchivePath(entry.first) ==
                                          normalizedPath;
                                 });
    if (existing != entries.end())
      existing->second = payload.svg;
    else
      entries.emplace_back(normalizedPath, payload.svg);
  }

  std::unordered_set<std::string> finalEntrySet;
  std::vector<symbol_cache::GdtfSemanticFingerprintEntry> fingerprintEntries;
  for (const auto &[name, content] : entries) {
    const std::string normalizedName = NormalizeArchivePath(name);
    finalEntrySet.insert(normalizedName);
    symbol_cache::GdtfSemanticFingerprintEntry fingerprintEntry;
    fingerprintEntry.normalizedPath = normalizedName;
    fingerprintEntry.bytes.assign(content.begin(), content.end());
    fingerprintEntries.push_back(std::move(fingerprintEntry));
  }
  const std::unordered_set<std::string> requiredPaths = {
      NormalizeArchivePath(topPath), NormalizeArchivePath(sidePath),
      NormalizeArchivePath(frontPath), NormalizeArchivePath(bottomPath)};
  result.requiredPathsConfirmed = HasRequiredSymbolPaths(finalEntrySet, requiredPaths);
  for (const auto &[path, payload] : payloads) {
    (void)payload;
    result.normalizedGeneratedSvgPaths.insert(NormalizeArchivePath(path));
  }
  if (!result.requiredPathsConfirmed) {
    result.diagnostic = "Generated SVG views were not present in the final archive entry set.";
    return result;
  }
  result.finalSemanticFingerprint =
      symbol_cache::ComputeGdtfSemanticFingerprintFromEntries(fingerprintEntries, errorMessage);
  if (result.finalSemanticFingerprint.empty()) {
    result.diagnostic = errorMessage;
    return result;
  }

  const fs::path tempPath = sourcePath.string() + ".tmp";
  {
    wxFileOutputStream output(tempPath.string());
    if (!output.IsOk()) {
      errorMessage = "Could not open temporary GDTF file for writing.";
      result.diagnostic = errorMessage;
      return result;
    }

    wxZipOutputStream zipOutput(output);
    for (const auto &[name, content] : entries) {
      auto *zipEntry = new wxZipEntry(name);
      zipEntry->SetMethod(wxZIP_METHOD_DEFLATE);
      zipOutput.PutNextEntry(zipEntry);
      zipOutput.Write(content.data(), content.size());
      zipOutput.CloseEntry();
    }
    zipOutput.Close();
  }

  std::error_code ec;
#ifdef _WIN32
  const std::wstring tempWide = tempPath.wstring();
  const std::wstring sourceWide = sourcePath.wstring();
  const BOOL moved = MoveFileExW(tempWide.c_str(), sourceWide.c_str(),
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
  if (!moved)
    ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
  fs::rename(tempPath, sourcePath, ec);
#endif
  if (ec) {
    fs::remove(tempPath);
    errorMessage = "Could not replace the original GDTF file.";
    result.diagnostic = errorMessage;
    return result;
  }

  result.atomicReplacementCompleted = true;
  rewritePhase.Finish();
  symbols::ScopedFixtureSymbolPhase validationPhase(
      timings, symbols::FixtureSymbolPhase::Validation);
  if (!VerifyArchiveEntries(sourcePath, payloads)) {
    result.diagnostic =
        "The replaced GDTF archive did not pass generated SVG post-validation.";
    return result;
  }
  symbol_cache::InvalidateGdtfSemanticFingerprintCache(sourcePath.string());
  std::string validationError;
  const std::string validatedFingerprint =
      symbol_cache::ComputeGdtfSemanticFingerprint(sourcePath.string(),
                                                   validationError);
  if (validatedFingerprint.empty() ||
      validatedFingerprint != result.finalSemanticFingerprint) {
    result.diagnostic = validationError.empty()
                            ? "The replaced GDTF archive fingerprint did not match the validated mutation."
                            : validationError;
    return result;
  }
  std::error_code metadataError;
  result.finalFileSize = fs::file_size(sourcePath, metadataError);
  metadataError.clear();
  result.finalModificationTime = fs::last_write_time(sourcePath, metadataError);
  symbol_cache::PublishGdtfSemanticFingerprintCache(
      sourcePath.string(), result.finalSemanticFingerprint);

  result.success = true;
  return result;
}

// Rewrites a fixture GDTF archive and returns a compatibility boolean result.
bool RewriteGdtf(const fs::path &sourcePath,
                 const std::unordered_map<std::string, SymbolPayload> &payloads,
                 const std::string &topPath,
                 const std::string &sidePath,
                 const std::string &frontPath,
                 const std::string &bottomPath,
                 std::string &errorMessage) {
  GdtfRewriteResult result = RewriteGdtfWithProof(sourcePath, payloads, topPath,
                                                  sidePath, frontPath, bottomPath);
  errorMessage = result.diagnostic;
  return result.success;
}

} // namespace

// Applies generated SVG symbol views and reports scene and library ownership separately.
ApplySymbolsResult ApplySymbolsToFixtureGdtfWithResult(
    const std::vector<symbols::Symbol2D> &symbols,
    const std::string &fixtureUuid,
    const ApplySymbolsOptions &options) {
  ApplySymbolsResult result;
  std::string errorMessage;
  if (!options.updateSceneCopy && !options.updateLibraryCopy) {
    result.diagnostic = "No fixture GDTF persistence target was requested.";
    return result;
  }
  if (fixtureUuid.empty()) {
    result.diagnostic = "No fixture was selected for this symbol preview.";
    return result;
  }

  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto &scene = cfg.GetScene();
  auto &fixtures = scene.fixtures;
  auto fixtureIt = fixtures.find(fixtureUuid);
  if (fixtureIt == fixtures.end()) {
    errorMessage = "Could not resolve the selected fixture in the scene.";
    result.diagnostic = "Could not resolve the selected fixture in the scene.";
    return result;
  }

  gui::fixtures::FixtureGdtfResolution resolution;
  if (!gui::fixtures::ResolveFixtureGdtfDeterministic(fixtureIt->second, scene,
                                                      resolution, errorMessage,
                                                      "apply")) {
    result.diagnostic = errorMessage;
    return result;
  }
  const std::string scenePath = resolution.scenePath;
  const std::string libraryPath = resolution.libraryPath;

  const std::string inspectPath = resolution.selectedPath;
  std::string modelSvgBase = ResolveModelSvgBasename(inspectPath, errorMessage);
  if (modelSvgBase.empty()) {
    result.diagnostic = errorMessage;
    return result;
  }

  const std::string topSvgPath = "models/svg/" + modelSvgBase + ".svg";
  const std::string sideSvgPath = "models/svg_side/" + modelSvgBase + ".svg";
  const std::string frontSvgPath = "models/svg_front/" + modelSvgBase + ".svg";
  const std::string bottomSvgPath = "models/svg/" + modelSvgBase + "_bottom.svg";

  std::unordered_map<std::string, SymbolPayload> payloads;
  SymbolPayload topPayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Top, topSvgPath,
                         topPayload, errorMessage)) {
    payloads[topPayload.archivePath] = std::move(topPayload);
  }

  SymbolPayload sidePayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Left, sideSvgPath,
                         sidePayload, errorMessage)) {
    payloads[sidePayload.archivePath] = std::move(sidePayload);
  }

  SymbolPayload frontPayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Front, frontSvgPath,
                         frontPayload, errorMessage)) {
    payloads[frontPayload.archivePath] = std::move(frontPayload);
  }

  SymbolPayload bottomPayload;
  if (BuildSymbolPayload(symbols, symbols::SymbolView::Bottom, bottomSvgPath,
                         bottomPayload, errorMessage)) {
    payloads[bottomPayload.archivePath] = std::move(bottomPayload);
  }

  if (payloads.empty()) {
    errorMessage =
        "No valid symbol views were available to apply to the fixture.";
    result.diagnostic = errorMessage;
    return result;
  }

  std::string writableScenePath = scenePath;
  bool sceneUpdated = false;
  if (options.updateSceneCopy && !scene.basePath.empty() &&
      !writableScenePath.empty() &&
      !IsPathWithinDirectory(fs::path(writableScenePath), fs::path(scene.basePath))) {
    if (!EnsureSceneLocalGdtfCopy(fs::path(writableScenePath), fs::path(scene.basePath),
                                  fixtureIt->second, writableScenePath,
                                  errorMessage)) {
      result.diagnostic = errorMessage;
      return result;
    }
  }

  if (options.updateSceneCopy && writableScenePath.empty() && !scene.basePath.empty()) {
    const std::string copySourcePath = !inspectPath.empty() ? inspectPath : libraryPath;
    if (!copySourcePath.empty()) {
      if (!EnsureSceneLocalGdtfCopy(fs::path(copySourcePath), fs::path(scene.basePath),
                                    fixtureIt->second, writableScenePath,
                                    errorMessage)) {
        result.diagnostic = errorMessage;
        return result;
      }
    }
  }

  if (options.updateSceneCopy && !writableScenePath.empty()) {
    if (!scene.basePath.empty() &&
        !GdtfDictionary::IsPerastageNamedGdtfFile(writableScenePath)) {
      if (!EnsureSceneLocalGdtfCopy(fs::path(writableScenePath), fs::path(scene.basePath),
                                    fixtureIt->second, writableScenePath,
                                    errorMessage)) {
        result.diagnostic = errorMessage;
        return result;
      }
    }
    const GdtfRewriteResult rewrite = RewriteGdtfWithProof(
        writableScenePath, payloads, topSvgPath, sideSvgPath, frontSvgPath,
        bottomSvgPath, options.timings);
    if (!rewrite.success) {
      result.diagnostic = rewrite.diagnostic;
      return result;
    }
    sceneUpdated = true;
    result.sceneUpdated = true;
    result.finalScenePath = rewrite.finalArchivePath.string();
    result.finalSceneFingerprint = rewrite.finalSemanticFingerprint;
  }

  if (options.updateSceneCopy && !sceneUpdated) {
    errorMessage = "Could not resolve a project-owned fixture GDTF copy to update.";
    result.diagnostic = "Could not resolve a project-owned fixture GDTF copy to update.";
    return result;
  }

  if (options.updateLibraryCopy) {
    const std::string librarySource = result.sceneUpdated ? result.finalScenePath : inspectPath;
    auto derivative = GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
        fixtureIt->second.typeName, librarySource, fixtureIt->second.gdtfMode,
        fixtureIt->second.category);
    if (!derivative || derivative->path.empty()) {
      const std::string warning =
          "Could not synchronize the Perastage library fixture derivative.";
      if (options.updateSceneCopy)
        result.warnings.push_back(warning);
      else
        result.diagnostic = warning;
    } else {
      result.finalLibraryPath = derivative->path;
      std::error_code pathError;
      if (PathUtils::AreFilesystemPathsEquivalent(
              fs::path(result.finalScenePath), fs::path(derivative->path),
              pathError)) {
        result.libraryUpdated = result.sceneUpdated;
      } else if (result.sceneUpdated) {
        // The derivative was copied from the already validated project archive.
        symbol_cache::PublishGdtfSemanticFingerprintCache(
            derivative->path, result.finalSceneFingerprint);
        result.libraryUpdated = true;
      } else if (RewriteGdtf(derivative->path, payloads, topSvgPath, sideSvgPath,
                             frontSvgPath, bottomSvgPath, errorMessage)) {
        result.libraryUpdated = true;
      } else {
        result.diagnostic = errorMessage;
      }
    }
  }

  result.success = options.updateSceneCopy ? result.sceneUpdated : result.libraryUpdated;
  if (!result.success && result.diagnostic.empty())
    result.diagnostic = "The requested fixture GDTF persistence operation failed.";
  return result;
}

// Applies generated SVG symbol views through the compatibility boolean contract.
bool ApplySymbolsToFixtureGdtf(const std::vector<symbols::Symbol2D> &symbols,
                               const std::string &fixtureUuid,
                               std::string &errorMessage,
                               const ApplySymbolsOptions &options) {
  const ApplySymbolsResult result =
      ApplySymbolsToFixtureGdtfWithResult(symbols, fixtureUuid, options);
  errorMessage = result.diagnostic;
  return result.success;
}


// Inspects a fixture GDTF to determine whether Perastage SVG symbols must be generated.
bool InspectFixtureSymbolState(const Fixture &fixture,
                               const MvrScene &scene,
                               FixtureSymbolInspectionResult &result,
                               std::string &errorMessage) {
  result = {};

  gui::fixtures::FixtureGdtfResolution resolution;
  if (!gui::fixtures::ResolveFixtureGdtfDeterministic(fixture, scene, resolution,
                                                      errorMessage, "inspect")) {
    result.scenePath = resolution.scenePath;
    result.libraryPath = resolution.libraryPath;
    return false;
  }
  result.scenePath = resolution.scenePath;
  result.libraryPath = resolution.libraryPath;
  std::string inspectPath = resolution.selectedPath;

  result.hasResolvableGdtf = true;

  wxFileInputStream input(inspectPath);
  if (!input.IsOk()) {
    errorMessage = "Could not open fixture GDTF file for inspection.";
    return false;
  }

  wxZipInputStream zipInput(input);
  std::unique_ptr<wxZipEntry> entry;
  std::string descriptionXml;
  std::unordered_set<std::string> archiveEntries;
  while ((entry.reset(zipInput.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    const std::string entryName = NormalizeArchivePath(entry->GetName().ToStdString());
    archiveEntries.insert(entryName);
    if (!IsDescriptionXmlPath(entryName))
      continue;
    if (!ReadAllBytes(zipInput, descriptionXml)) {
      errorMessage = "Could not read description.xml from the GDTF file.";
      return false;
    }
  }

  if (descriptionXml.empty())
    return true;

  tinyxml2::XMLDocument doc;
  if (doc.Parse(descriptionXml.c_str(), descriptionXml.size()) != tinyxml2::XML_SUCCESS)
    return true;

  tinyxml2::XMLElement *fixtureType = ResolveFixtureType(doc);
  if (!fixtureType)
    return true;

  const bool revisionModifiedByPerastage =
      HasPerastageRevisionModifiedBy(fixtureType);
  const bool editorIsPerastage =
      IsPerastageEditorValue(fixtureType->Attribute("Editor"));
  const auto compatibility = GdtfMutationAudit::InspectCompatibility(fixtureType);
  result.warningMessage = compatibility.warning;
  result.editorIsPerastage =
      compatibility.mode == GdtfMutationAudit::CompatibilityMode::KnownPerastageVersion
          ? true
          : (compatibility.mode ==
                     GdtfMutationAudit::CompatibilityMode::LegacyFallback
                 ? (revisionModifiedByPerastage || editorIsPerastage)
                 : false);

  std::string modelSvgBase =
      ResolveModelSvgBasenameFromFixtureType(fixtureType, errorMessage);
  if (modelSvgBase.empty()) {
    errorMessage.clear();
    return true;
  }

  const std::unordered_set<std::string> requiredPaths =
      BuildRequiredSymbolPaths(modelSvgBase);
  result.hasValidSvgSymbolSet =
      result.editorIsPerastage && HasRequiredSymbolPaths(archiveEntries, requiredPaths);
  result.requiresSymbolGeneration = !result.hasValidSvgSymbolSet;
  return true;
}

// Synchronizes only library-owned fixture changes back to a stable @Perastage derivative.
bool SyncFixtureGdtfToLibrary(const Fixture &fixture,
                              const MvrScene &scene,
                              std::string &errorMessage) {
  gui::fixtures::FixtureGdtfResolution resolution;
  if (!gui::fixtures::ResolveFixtureGdtfDeterministic(fixture, scene, resolution,
                                                      errorMessage, "sync")) {
    return false;
  }
  const std::string scenePath = resolution.scenePath;
  const std::string libraryPath = resolution.libraryPath;
  if (scenePath.empty() || libraryPath.empty() || scenePath == libraryPath)
    return true;

  if (resolution.libraryPath.empty())
    return true;

  auto derivative = GdtfDictionary::CreateOrUpdatePerastageLibraryDerivative(
      fixture.typeName, scenePath, fixture.gdtfMode, fixture.category);
  if (!derivative) {
    errorMessage = "Could not update the Perastage library fixture derivative.";
    return false;
  }
  return true;
}

} // namespace symbol_preview
