/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "mvr_export_archive_writer.h"
#include "project_archive_publication.h"
#include <wx/filename.h>

#include "wx_path_utils.h"

#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

#include <fstream>
#include <unordered_set>
#include <utility>

namespace mvr_export_archive {
namespace {

// Constructs a failed archive result with operation context.
Result Failure(std::string operation, std::string archivePath,
               std::string sourcePath, std::string reason) {
  return {false, std::move(operation), std::move(archivePath),
          std::move(sourcePath), std::move(reason)};
}

// Writes one byte range as a deflated ZIP entry.
Result WriteBytes(wxZipOutputStream &zip,
                  std::unordered_set<std::string> &writtenEntries,
                  const std::string &operation, const std::string &archivePath,
                  const std::string &sourcePath, const char *data, size_t size) {
  if (!writtenEntries.insert(archivePath).second)
    return Failure(operation, archivePath, sourcePath, "duplicate ZIP entry");
  auto *entry = new wxZipEntry(archivePath);
  entry->SetMethod(wxZIP_METHOD_DEFLATE);
  if (!zip.PutNextEntry(entry))
    return Failure(operation, archivePath, sourcePath,
                   "could not create ZIP entry");
  zip.Write(data, size);
  if (!zip.IsOk()) {
    zip.CloseEntry();
    return Failure(operation, archivePath, sourcePath,
                   operation == "WriteXml" ? "could not write XML bytes"
                                             : "could not write resource bytes");
  }
  if (!zip.CloseEntry())
    return Failure(operation, archivePath, sourcePath,
                   "could not close ZIP entry");
  return {true};
}

} // namespace

// Writes prepared XML and resources to an MVR ZIP package.
static Result WritePrepared(const Request &request) {
  wxFileOutputStream output(
      WxPathUtils::WxStringFromFilesystemPath(request.destinationPath));
  if (!output.IsOk())
    return Failure("OpenOutput", {}, request.destinationPath.string(),
                   "could not open output file");

  wxZipOutputStream zip(output);
  std::unordered_set<std::string> writtenEntries;
  Result result = WriteBytes(zip, writtenEntries, "WriteXml",
                             "GeneralSceneDescription.xml", {},
                             request.sceneXml.data(), request.sceneXml.size());
  if (!result.success) {
    zip.Close();
    return result;
  }

  for (const Resource &resource : request.resources) {
    if (!writtenEntries.insert(resource.archivePath).second) {
      zip.Close();
      return Failure("WriteResource", resource.archivePath,
                     resource.sourcePath.string(), "duplicate ZIP entry");
    }
    std::ifstream input(resource.sourcePath, std::ios::binary);
    if (!input.is_open()) {
      zip.Close();
      return Failure("WriteResource", resource.archivePath,
                     resource.sourcePath.string(), "could not open source file");
    }
    auto *entry = new wxZipEntry(resource.archivePath);
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    if (!zip.PutNextEntry(entry)) {
      zip.Close();
      return Failure("WriteResource", resource.archivePath,
                     resource.sourcePath.string(), "could not create ZIP entry");
    }
    char buffer[4096];
    while (input.good()) {
      input.read(buffer, sizeof(buffer));
      const std::streamsize count = input.gcount();
      if (count > 0) {
        zip.Write(buffer, count);
        if (!zip.IsOk()) {
          zip.CloseEntry();
          zip.Close();
          return Failure("WriteResource", resource.archivePath,
                         resource.sourcePath.string(),
                         "could not write resource bytes");
        }
      }
    }
    if (input.bad()) {
      zip.CloseEntry();
      zip.Close();
      return Failure("WriteResource", resource.archivePath,
                     resource.sourcePath.string(), "could not read source file");
    }
    if (!zip.CloseEntry()) {
      zip.Close();
      return Failure("WriteResource", resource.archivePath,
                     resource.sourcePath.string(), "could not close ZIP entry");
    }
  }

  if (!zip.Close())
    return Failure("FinalizeArchive", {}, request.destinationPath.string(),
                   "could not close ZIP");
  if (!output.Close())
    return Failure("FinalizeArchive", {}, request.destinationPath.string(),
                   "could not flush and close archive");
  return {true};
}

// Publish only a complete, closed archive. A semantic or resource failure must
// never truncate an earlier standalone MVR at the requested destination.
Result Write(const Request &request) {
  std::error_code locationError;
  const auto target =
      std::filesystem::absolute(request.destinationPath, locationError);
  if (locationError)
    return Failure("OpenOutput", {}, request.destinationPath.string(),
                   locationError.message());
  const auto temporaryName = wxFileName::CreateTempFileName(
      WxPathUtils::WxStringFromFilesystemPath(target) + ".tmp-");
  if (temporaryName.empty())
    return Failure("OpenOutput", {}, request.destinationPath.string(),
                   "could not create output temporary file");
  const auto temporary = WxPathUtils::FilesystemPathFromWxString(temporaryName);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove(path, error);
    }
  } cleanup{temporary};
  auto prepared = request;
  prepared.destinationPath = temporary;
  auto result = WritePrepared(prepared);
  if (!result.success)
    return result;
  std::string reason;
  if (!project_archive::Publish(temporary, target, reason))
    return Failure("FinalizeArchive", {}, request.destinationPath.string(),
                   reason);
  return {true};
}

} // namespace mvr_export_archive
