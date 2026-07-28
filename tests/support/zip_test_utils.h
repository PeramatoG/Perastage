#pragma once

#include "wx_path_utils.h"

#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace tests::zip {

struct Entry {
  std::string name;
  std::string payload;
  bool isDirectory = false;
};

// Converts a wxString to UTF-8 without using the process locale.
inline std::string ToUtf8(const wxString &value) {
  const wxScopedCharBuffer utf8 = value.utf8_str();
  return utf8.data() == nullptr ? std::string() : std::string(utf8.data());
}

// Reads ZIP entries with canonical Unix names while retaining directory metadata.
inline std::vector<Entry> ReadEntries(const std::filesystem::path &archivePath) {
  wxFFileInputStream input(
      WxPathUtils::WxStringFromFilesystemPath(archivePath));
  assert(input.IsOk());
  wxZipInputStream zip(input);
  std::vector<Entry> entries;
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    Entry result;
    result.name = ToUtf8(entry->GetName(wxPATH_UNIX));
    result.isDirectory = entry->IsDir() ||
                         (!result.name.empty() && result.name.back() == '/');
    char buffer[4096];
    while (!result.isDirectory) {
      zip.Read(buffer, sizeof(buffer));
      const size_t bytes = zip.LastRead();
      if (bytes == 0)
        break;
      result.payload.append(buffer, bytes);
    }
    entries.push_back(std::move(result));
  }
  return entries;
}

} // namespace tests::zip
