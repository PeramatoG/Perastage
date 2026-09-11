/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "mvr_import_package.h"

#include "filesystem_path_utils.h"
#include "logger.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_set>

#include <wx/filename.h>
#include <wx/zipstrm.h>

namespace fs = std::filesystem;

namespace {

// Converts a UTF-8 filesystem string without changing its byte sequence.
std::string ToString(const std::u8string &value) {
  return std::string(value.begin(), value.end());
}

// Removes surrounding ASCII whitespace from an archive path.
std::string Trim(const std::string &value) {
  const char *whitespace = " \t\r\n";
  const size_t start = value.find_first_not_of(whitespace);
  if (start == std::string::npos)
    return {};
  const size_t end = value.find_last_not_of(whitespace);
  return value.substr(start, end - start + 1);
}

// Converts archive separators to their portable representation.
std::string NormalizeSlashes(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

// Lowercases ASCII text used for portable archive comparisons.
std::string ToLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

// Reports whether a destination path risks the legacy Windows path limit.
bool IsPathLikelyTooLong(const fs::path &path) {
#ifdef _WIN32
  constexpr size_t kLegacyMaxPathSafetyLimit = 245;
  return ToString(path.u8string()).size() >= kLegacyMaxPathSafetyLimit;
#else
  (void)path;
  return false;
#endif
}

// Folds only ASCII uppercase characters for platform-neutral archive identity.
std::string FoldArchiveIdentityAscii(std::string text) {
  for (char &ch : text) {
    if (ch >= 'A' && ch <= 'Z')
      ch = static_cast<char>(ch - 'A' + 'a');
  }
  return text;
}

// Escapes control characters in archive identities used by diagnostics.
std::string EscapeArchiveIdentity(const std::string &identity) {
  std::ostringstream escaped;
  for (unsigned char ch : identity) {
    if (ch < 32 || ch == 127) {
      escaped << "\\x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(ch) << std::dec;
    } else {
      escaped << static_cast<char>(ch);
    }
  }
  return escaped.str();
}

// Extracts an MVR stream while preserving archive safety and collision rules.
bool ExtractArchive(wxInputStream &input, const fs::path &destination,
                    std::unordered_map<std::string, std::string> &pathRemap,
                    std::vector<MvrImportDiagnostic> &diagnostics) {
  wxZipInputStream zipStream(input);
  std::unique_ptr<wxZipEntry> entry;
  std::unordered_map<std::string, std::string> identityByFoldedKey;
  std::unordered_map<std::string, fs::path> extractedPathByIdentity;
  std::unordered_set<std::string> ambiguousFoldedKeys;

  auto discardCurrentEntry = [&]() {
    char discardBuffer[4096];
    while (true) {
      zipStream.Read(discardBuffer, sizeof(discardBuffer));
      if (zipStream.LastRead() == 0)
        break;
    }
  };

  while ((entry.reset(zipStream.GetNextEntry())), entry) {
    const std::string entryName = entry->GetName().ToUTF8().data();
    const std::string normalizedUnsafeCheck = NormalizeSlashes(entryName);
    const fs::path relativeEntryPath =
        PathUtils::PathFromUtf8(normalizedUnsafeCheck);
    if (normalizedUnsafeCheck.empty() || relativeEntryPath.is_absolute() ||
        relativeEntryPath.has_root_name() ||
        normalizedUnsafeCheck.find(':') != std::string::npos ||
        std::any_of(relativeEntryPath.begin(), relativeEntryPath.end(),
                    [](const fs::path &part) { return part == ".."; })) {
      Logger::Instance().Log(Logger::Level::Warn,
                             "Skipping unsafe MVR archive entry: " + entryName);
      discardCurrentEntry();
      continue;
    }
    fs::path fullPath = destination / relativeEntryPath;

    if (entry->IsDir()) {
      const std::string dirUtf8 = ToString(fullPath.u8string());
      wxFileName::Mkdir(wxString::FromUTF8(dirUtf8.c_str()), wxS_DIR_DEFAULT,
                        wxPATH_MKDIR_FULL);
      continue;
    }

    const std::string archiveIdentity = normalizedUnsafeCheck;
    const std::string foldedIdentity =
        FoldArchiveIdentityAscii(archiveIdentity);
    if (ambiguousFoldedKeys.contains(foldedIdentity)) {
      pathRemap.erase(mvr::NormalizeImportArchivePath(archiveIdentity));
      discardCurrentEntry();
      continue;
    }
    auto priorIdentityIt = identityByFoldedKey.find(foldedIdentity);
    if (priorIdentityIt != identityByFoldedKey.end()) {
      const bool exactDuplicate = priorIdentityIt->second == archiveIdentity;
      const char *code = exactDuplicate ? "duplicate_mvr_archive_entry"
                                        : "case_colliding_mvr_archive_entry";
      diagnostics.push_back(
          {code, std::string("Rejected ambiguous MVR archive entries '") +
                     EscapeArchiveIdentity(priorIdentityIt->second) +
                     "' and '" + EscapeArchiveIdentity(archiveIdentity) +
                     "'."});
      auto extractedIt = extractedPathByIdentity.find(priorIdentityIt->second);
      if (extractedIt != extractedPathByIdentity.end()) {
        std::error_code removeEc;
        fs::remove(extractedIt->second, removeEc);
        extractedPathByIdentity.erase(extractedIt);
      }
      pathRemap.erase(mvr::NormalizeImportArchivePath(priorIdentityIt->second));
      pathRemap.erase(mvr::NormalizeImportArchivePath(archiveIdentity));
      ambiguousFoldedKeys.insert(foldedIdentity);
      discardCurrentEntry();
      if (foldedIdentity == "generalscenedescription.xml")
        return false;
      continue;
    }
    identityByFoldedKey.emplace(foldedIdentity, archiveIdentity);

    const std::string parentUtf8 = ToString(fullPath.parent_path().u8string());
    wxFileName::Mkdir(wxString::FromUTF8(parentUtf8.c_str()), wxS_DIR_DEFAULT,
                      wxPATH_MKDIR_FULL);

    const std::string normalizedEntryName =
        mvr::NormalizeImportArchivePath(entryName);
    const size_t fullPathLength = ToString(fullPath.u8string()).size();
    auto tryOpenOutput = [](const fs::path &path) {
      return std::ofstream(path, std::ios::binary);
    };

    std::ofstream output;
    bool remapped = false;
    if (!IsPathLikelyTooLong(fullPath))
      output = tryOpenOutput(fullPath);

    if (!output.is_open()) {
      const fs::path longDir = destination / "_long";
      const std::string extension =
          PathUtils::PathFromUtf8(entryName).extension().string();
      const std::string hashBase =
          std::to_string(std::hash<std::string>{}(normalizedEntryName));
      wxFileName::Mkdir(
          wxString::FromUTF8(ToString(longDir.u8string()).c_str()),
          wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

      for (int suffix = 0; suffix < 64 && !output.is_open(); ++suffix) {
        std::string candidateName = hashBase;
        if (suffix > 0)
          candidateName += "_" + std::to_string(suffix);
        candidateName += extension;
        const fs::path candidatePath =
            longDir / PathUtils::PathFromUtf8(candidateName);
        output = tryOpenOutput(candidatePath);
        if (output.is_open()) {
          fullPath = candidatePath;
          pathRemap[normalizedEntryName] = ToString(
              (fs::path("_long") / PathUtils::PathFromUtf8(candidateName))
                  .u8string());
          remapped = true;
        }
      }
    }

    if (!output.is_open()) {
      std::ostringstream message;
      message << "Cannot create file while extracting MVR entry. entry='"
              << entryName << "', path='" << ToString(fullPath.u8string())
              << "', pathLength=" << fullPathLength;
      const std::string loweredEntry = ToLowerAscii(normalizedEntryName);
      const bool isSceneXml =
          loweredEntry == "generalscenedescription.xml" ||
          fs::path(loweredEntry).filename().generic_string() ==
              "generalscenedescription.xml";
      if (isSceneXml) {
        Logger::Instance().Log(Logger::Level::Error,
                               message.str() +
                                   " (required scene XML; aborting import)");
        return false;
      }
      Logger::Instance().Log(Logger::Level::Warn,
                             message.str() +
                                 " (asset entry skipped, continuing import)");
      discardCurrentEntry();
      continue;
    }

    if (remapped) {
      const std::string remappedPath = pathRemap[normalizedEntryName];
      std::ostringstream warning;
      warning << "MVR extraction remapped long path entry. entry='" << entryName
              << "', remapped='" << remappedPath
              << "', originalLength=" << fullPathLength;
      Logger::Instance().Log(Logger::Level::Warn, warning.str());
    }

    char buffer[4096];
    while (true) {
      zipStream.Read(buffer, sizeof(buffer));
      const size_t bytes = zipStream.LastRead();
      if (bytes == 0)
        break;
      output.write(buffer, bytes);
    }
    output.close();
    extractedPathByIdentity[archiveIdentity] = fullPath;
  }
  return true;
}

// Locates the normative root scene XML or its legacy case-insensitive variant.
fs::path FindSceneXml(const fs::path &rootPath) {
  fs::path sceneFile = rootPath / "GeneralSceneDescription.xml";
  std::error_code error;
  if (fs::exists(sceneFile, error) && !error)
    return sceneFile;

  error.clear();
  for (const auto &entry : fs::directory_iterator(rootPath, error)) {
    if (error)
      break;
    std::error_code regularFileError;
    if (entry.is_regular_file(regularFileError) && !regularFileError &&
        ToLowerAscii(entry.path().filename().string()) ==
            "generalscenedescription.xml") {
      return entry.path();
    }
  }
  return {};
}

} // namespace

namespace mvr {

// Normalizes a packaged resource path for importer lookup and remapping.
std::string NormalizeImportArchivePath(const std::string &archivePath) {
  std::string normalized = Trim(NormalizeSlashes(archivePath));
  const std::string lowered = ToLowerAscii(normalized);
  const size_t gdtfPos = lowered.rfind(".gdtf");
  if (gdtfPos != std::string::npos && gdtfPos > 0) {
    size_t trimPos = gdtfPos;
    while (trimPos > 0 && normalized[trimPos - 1] == ' ')
      --trimPos;
    if (trimPos != gdtfPos)
      normalized.erase(trimPos, gdtfPos - trimPos);
  }
  return normalized;
}

// Safely extracts an MVR stream and locates its root scene description.
std::optional<ImportPackage>
AcquireImportPackage(wxInputStream &input,
                     std::vector<MvrImportDiagnostic> &diagnostics) {
  runtime_storage::TemporaryWorkspace workspace("mvr-import");
  if (!workspace.IsValid()) {
    Logger::Instance().Log("Failed to create MVR import workspace.");
    return std::nullopt;
  }

  const fs::path rootPath = workspace.Path();
  std::unordered_map<std::string, std::string> pathRemap;
  if (!ExtractArchive(input, rootPath, pathRemap, diagnostics)) {
    Logger::Instance().Log("Failed to extract MVR file.");
    return std::nullopt;
  }

  int extractedGdtfEntryCount = 0;
  std::error_code countError;
  for (const auto &entry : fs::directory_iterator(rootPath, countError)) {
    if (countError)
      break;
    std::error_code regularFileError;
    if (entry.is_regular_file(regularFileError) && !regularFileError &&
        ToLowerAscii(entry.path().extension().string()) == ".gdtf") {
      ++extractedGdtfEntryCount;
    }
  }
  Logger::Instance().Log(
      Logger::Level::Info,
      "MVR extraction diagnostics: basePath='" + ToString(rootPath.u8string()) +
          "', extractedGdtfEntries=" + std::to_string(extractedGdtfEntryCount));

  const fs::path sceneXmlPath = FindSceneXml(rootPath);
  if (sceneXmlPath.empty()) {
    Logger::Instance().Log("Missing GeneralSceneDescription.xml in MVR.");
    return std::nullopt;
  }

  return ImportPackage{std::move(workspace), rootPath, sceneXmlPath,
                       std::move(pathRemap)};
}

} // namespace mvr
