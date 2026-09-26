#include "archive_zip_entry_reader.h"

#include "wx_path_utils.h"

#include <algorithm>
#include <memory>
#include <unordered_map>

#include <wx/mstream.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace perastage::archive::zip {
namespace {

// Reads one validated local ZIP record with all-or-nothing limit semantics.
template <typename InputStream>
EntryReadResult ReadEntryFromStream(InputStream &input,
                                    const DirectoryEntry &expected,
                                    std::uint64_t maxBytes) {
  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->GetOffset() >= 0 &&
        static_cast<std::uint64_t>(entry->GetOffset()) ==
            expected.localHeaderOffset)
      break;
  }
  if (!entry)
    return {EntryReadStatus::EntryMissing, {}};
  // ReadDirectory already validated this offset and its raw central/local name.
  if (entry->IsDir() != expected.directory)
    return {EntryReadStatus::ReadFailed, {}};
  const wxFileOffset claimedSize = entry->GetSize();
  if (claimedSize >= 0 && static_cast<std::uint64_t>(claimedSize) > maxBytes)
    return {EntryReadStatus::EntryTooLarge, {}};

  EntryReadResult result{EntryReadStatus::Success, {}};
  std::uint8_t buffer[8192];
  while (true) {
    const std::uint64_t remaining = maxBytes - result.bytes.size();
    const std::uint64_t requested =
        remaining >= sizeof(buffer) ? remaining : remaining + 1;
    const std::size_t request = static_cast<std::size_t>(
        std::min<std::uint64_t>(sizeof(buffer), requested));
    zip.Read(buffer, request);
    const std::size_t count = zip.LastRead();
    if (count == 0)
      break;
    if (count > remaining)
      return {EntryReadStatus::EntryTooLarge, {}};
    result.bytes.insert(result.bytes.end(), buffer, buffer + count);
  }
  if (zip.GetLastError() != wxSTREAM_NO_ERROR &&
      zip.GetLastError() != wxSTREAM_EOF)
    return {EntryReadStatus::ReadFailed, {}};
  result.complete = true;
  return result;
}

// Reads bounded prefixes for selected local records in one sequential pass.
template <typename InputStream>
std::vector<EntryReadResult>
ReadPrefixesFromStream(InputStream &input,
                       const std::vector<DirectoryEntry> &expected,
                       std::uint64_t maxBytes) {
  std::vector<EntryReadResult> results(expected.size(),
                                       {EntryReadStatus::EntryMissing, {}});
  std::unordered_map<std::uint64_t, std::size_t> byOffset;
  for (std::size_t index = 0; index < expected.size(); ++index)
    byOffset.emplace(expected[index].localHeaderOffset, index);

  wxZipInputStream zip(input);
  std::unique_ptr<wxZipEntry> entry;
  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->GetOffset() < 0)
      continue;
    const auto selected =
        byOffset.find(static_cast<std::uint64_t>(entry->GetOffset()));
    if (selected == byOffset.end())
      continue;
    const DirectoryEntry &identity = expected[selected->second];
    // The validated local offset is the physical identity across wx path formats.
    if (entry->IsDir() != identity.directory) {
      results[selected->second].status = EntryReadStatus::ReadFailed;
      continue;
    }
    EntryReadResult &result = results[selected->second];
    result.status = EntryReadStatus::Success;
    std::uint8_t buffer[8192];
    while (result.bytes.size() < maxBytes) {
      const std::size_t request =
          static_cast<std::size_t>(std::min<std::uint64_t>(
              sizeof(buffer), maxBytes - result.bytes.size()));
      zip.Read(buffer, request);
      const std::size_t count = zip.LastRead();
      if (count == 0)
        break;
      result.bytes.insert(result.bytes.end(), buffer, buffer + count);
    }
    if (result.bytes.size() < maxBytes &&
        zip.GetLastError() != wxSTREAM_NO_ERROR &&
        zip.GetLastError() != wxSTREAM_EOF) {
      result.status = EntryReadStatus::ReadFailed;
      result.bytes.clear();
    } else if (result.bytes.size() < maxBytes) {
      result.complete = true;
    }
  }
  return results;
}
} // namespace

// Reports whether the selected ZIP payload was read completely or as a prefix.
bool EntryReadResult::Success() const {
  return status == EntryReadStatus::Success;
}

// Opens a filesystem stream at one validated local-record offset.
EntryReadResult ReadEntry(const std::filesystem::path &archivePath,
                          const DirectoryEntry &entry, std::uint64_t maxBytes) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  if (!input.IsOk())
    return {EntryReadStatus::OpenFailed, {}};
  return ReadEntryFromStream(input, entry, maxBytes);
}

// Opens a memory stream at one validated local-record offset.
EntryReadResult ReadEntry(std::span<const std::uint8_t> archiveBytes,
                          const DirectoryEntry &entry, std::uint64_t maxBytes) {
  wxMemoryInputStream input(archiveBytes.data(), archiveBytes.size());
  return ReadEntryFromStream(input, entry, maxBytes);
}

// Reads selected filesystem prefixes without rescanning the archive per entry.
std::vector<EntryReadResult>
ReadEntryPrefixes(const std::filesystem::path &archivePath,
                  const std::vector<DirectoryEntry> &entries,
                  std::uint64_t maxBytes) {
  wxFileInputStream input(WxPathUtils::WxStringFromFilesystemPath(archivePath));
  if (!input.IsOk())
    return std::vector<EntryReadResult>(entries.size(),
                                        {EntryReadStatus::OpenFailed, {}});
  return ReadPrefixesFromStream(input, entries, maxBytes);
}

// Reads selected memory prefixes without rescanning the archive per entry.
std::vector<EntryReadResult>
ReadEntryPrefixes(std::span<const std::uint8_t> archiveBytes,
                  const std::vector<DirectoryEntry> &entries,
                  std::uint64_t maxBytes) {
  wxMemoryInputStream input(archiveBytes.data(), archiveBytes.size());
  return ReadPrefixesFromStream(input, entries, maxBytes);
}

} // namespace perastage::archive::zip
