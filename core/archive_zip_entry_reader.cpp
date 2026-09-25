#include "archive_zip_entry_reader.h"

#include "wx_path_utils.h"

#include <memory>

#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace perastage::archive::zip {
namespace {

// Reads one indexed ZIP entry while enforcing the limit during decompression.
template <typename InputStream>
EntryReadResult ReadEntryFromStream(InputStream &input, std::size_t entryIndex,
                                    std::uint64_t maxBytes) {
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  for (std::size_t index = 0; (entry.reset(zip.GetNextEntry())), entry;
       ++index) {
    if (index != entryIndex)
      continue;
    const wxFileOffset claimedSize = entry->GetSize();
    if (claimedSize >= 0 && static_cast<std::uint64_t>(claimedSize) > maxBytes)
      return {EntryReadStatus::EntryTooLarge, {}};

    EntryReadResult result{EntryReadStatus::Success, {}};
    std::uint8_t buffer[8192];
    while (true) {
      zip.Read(buffer, sizeof(buffer));
      const std::size_t count = zip.LastRead();
      if (count == 0)
        break;
      if (count > maxBytes || result.bytes.size() > maxBytes - count)
        return {EntryReadStatus::EntryTooLarge, {}};
      result.bytes.insert(result.bytes.end(), buffer, buffer + count);
    }
    if (zip.GetLastError() != wxSTREAM_NO_ERROR &&
        zip.GetLastError() != wxSTREAM_EOF)
      return {EntryReadStatus::ReadFailed, {}};
    return result;
  }
  return {EntryReadStatus::EntryMissing, {}};
}
} // namespace

// Reports whether the indexed ZIP payload was read completely.
bool EntryReadResult::Success() const {
  return status == EntryReadStatus::Success;
}

// Opens a fresh filesystem stream and reads one authoritative entry index.
EntryReadResult ReadEntry(const std::filesystem::path &archivePath,
                          std::size_t entryIndex, std::uint64_t maxBytes) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  if (!input.IsOk())
    return {EntryReadStatus::OpenFailed, {}};
  return ReadEntryFromStream(input, entryIndex, maxBytes);
}

// Opens a fresh memory stream and reads one authoritative entry index.
EntryReadResult ReadEntry(std::span<const std::uint8_t> archiveBytes,
                          std::size_t entryIndex, std::uint64_t maxBytes) {
  wxMemoryInputStream input(archiveBytes.data(), archiveBytes.size());
  return ReadEntryFromStream(input, entryIndex, maxBytes);
}

} // namespace perastage::archive::zip
