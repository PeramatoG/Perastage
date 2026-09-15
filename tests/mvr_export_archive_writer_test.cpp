/*
 * This file is part of Perastage.
 */
#include "mvr_export_archive_writer.h"
#include "wx_path_utils.h"

#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

// Reads all payloads from an archive for exact characterization.
static std::unordered_map<std::string, std::string>
ReadEntries(const fs::path &path) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(path));
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::unordered_map<std::string, std::string> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    std::string payload;
    char buffer[256];
    while (zip.Read(buffer, sizeof(buffer)).LastRead() > 0)
      payload.append(buffer, zip.LastRead());
    assert(entries.emplace(entry->GetName().ToStdString(), payload).second);
  }
  return entries;
}

// Characterizes package entries, duplicate safety, missing sources, and closure.
int main() {
  wxInitializer wx;
  assert(wx.IsOk());
  const fs::path directory =
      fs::temp_directory_path() / "perastage-mvr-archive-writer-test";
  fs::remove_all(directory);
  fs::create_directories(directory / "sources");
  std::ofstream(directory / "sources" / "one.bin", std::ios::binary) << "one";
  std::ofstream(directory / "sources" / "two.bin", std::ios::binary) << "two";

  mvr_export_archive::Request request;
  request.destinationPath = directory / "complete.mvr";
  request.sceneXml = "<GeneralSceneDescription verMajor=\"1\" verMinor=\"6\"/>";
  request.resources = {{directory / "sources" / "one.bin", "models/one.bin"},
                       {directory / "sources" / "two.bin", "two.bin"}};
  const auto result = mvr_export_archive::Write(request);
  assert(result.success);
  const auto entries = ReadEntries(request.destinationPath);
  assert(entries.at("GeneralSceneDescription.xml") == request.sceneXml);
  assert(entries.at("models/one.bin") == "one");
  assert(entries.at("two.bin") == "two");

  request.destinationPath = directory / "duplicate.mvr";
  request.resources[1].archivePath = "models/one.bin";
  const auto duplicate = mvr_export_archive::Write(request);
  assert(!duplicate.success);
  assert(duplicate.operation == "WriteResource");
  assert(duplicate.archivePath == "models/one.bin");
  assert(duplicate.reason == "duplicate ZIP entry");

  request.destinationPath = directory / "missing.mvr";
  request.resources = {{directory / "sources" / "missing.bin", "missing.bin"}};
  const auto missing = mvr_export_archive::Write(request);
  assert(!missing.success);
  assert(missing.archivePath == "missing.bin");
  assert(missing.sourcePath.find("missing.bin") != std::string::npos);
  assert(missing.reason == "could not open source file");
  fs::remove_all(directory);
  return 0;
}
