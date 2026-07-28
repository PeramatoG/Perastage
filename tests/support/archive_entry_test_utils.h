#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace tests::archive {

struct LogicalArchivePathResult {
  bool ok = false;
  std::string path;
  std::string error;
};

// Returns true when the entry name starts with a Windows drive prefix.
inline bool HasWindowsDrivePrefix(const std::string &name) {
  return name.size() >= 2 && name[1] == ':' &&
         ((name[0] >= 'A' && name[0] <= 'Z') ||
          (name[0] >= 'a' && name[0] <= 'z'));
}

// Converts a wx-presented ZIP entry name into a canonical logical archive path.
inline LogicalArchivePathResult NormalizePresentedArchivePath(
    const std::string &presentedName) {
  LogicalArchivePathResult result;
  if (presentedName.empty()) {
    result.error = "archive entry name is empty";
    return result;
  }
  if (presentedName.front() == '/' || presentedName.front() == '\\' ||
      HasWindowsDrivePrefix(presentedName)) {
    result.error = "archive entry name is absolute or drive-qualified: " +
                   presentedName;
    return result;
  }

  std::string normalized = presentedName;
  for (char &ch : normalized) {
    if (ch == '\\')
      ch = '/';
  }

  std::size_t componentStart = 0;
  while (componentStart <= normalized.size()) {
    const std::size_t separator = normalized.find('/', componentStart);
    const std::size_t componentEnd = separator == std::string::npos
                                         ? normalized.size()
                                         : separator;
    const std::string component =
        normalized.substr(componentStart, componentEnd - componentStart);
    if (component.empty() || component == "." || component == "..") {
      result.error = "archive entry name contains an unsafe path component: " +
                     presentedName;
      return result;
    }
    if (separator == std::string::npos)
      break;
    componentStart = separator + 1;
  }

  result.ok = true;
  result.path = normalized;
  return result;
}

// Reads the complete binary contents of a file for strict ZIP inspection.
inline std::vector<unsigned char> ReadBinaryFile(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

// Reads a little-endian 16-bit value from a byte buffer.
inline std::uint16_t ReadLe16(const std::vector<unsigned char> &bytes,
                              std::size_t offset) {
  return static_cast<std::uint16_t>(bytes[offset]) |
         (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

// Reads a little-endian 32-bit value from a byte buffer.
inline std::uint32_t ReadLe32(const std::vector<unsigned char> &bytes,
                              std::size_t offset) {
  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

// Appends one little-endian 16-bit value to a ZIP fixture buffer.
inline void AppendLe16(std::vector<unsigned char> &bytes, std::uint16_t value) {
  bytes.push_back(static_cast<unsigned char>(value & 0xff));
  bytes.push_back(static_cast<unsigned char>((value >> 8) & 0xff));
}

// Appends one little-endian 32-bit value to a ZIP fixture buffer.
inline void AppendLe32(std::vector<unsigned char> &bytes, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8)
    bytes.push_back(static_cast<unsigned char>((value >> shift) & 0xff));
}

// Computes the standard ZIP CRC-32 for one stored payload.
inline std::uint32_t ComputeCrc32(const std::string &payload) {
  std::uint32_t crc = 0xffffffffU;
  for (const unsigned char byte : payload) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
  }
  return crc ^ 0xffffffffU;
}

// Writes a deterministic stored ZIP whose entry names remain exact raw bytes.
inline bool WriteStoredZipWithRawNames(
    const std::string &archivePath,
    const std::vector<std::pair<std::string, std::string>> &entries,
    std::string &error) {
  if (entries.size() > std::numeric_limits<std::uint16_t>::max()) {
    error = "ZIP fixture has too many entries";
    return false;
  }
  std::vector<unsigned char> bytes;
  struct CentralRecord {
    std::string name;
    std::string payload;
    std::uint32_t crc = 0;
    std::uint32_t localOffset = 0;
  };
  std::vector<CentralRecord> records;
  for (const auto &[name, payload] : entries) {
    if (name.empty() || name.size() > std::numeric_limits<std::uint16_t>::max() ||
        payload.size() > std::numeric_limits<std::uint32_t>::max() ||
        bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
      error = "ZIP fixture entry exceeds classic ZIP limits";
      return false;
    }
    CentralRecord record{name, payload, ComputeCrc32(payload),
                         static_cast<std::uint32_t>(bytes.size())};
    AppendLe32(bytes, 0x04034b50);
    AppendLe16(bytes, 20);
    AppendLe16(bytes, 0x0800);
    AppendLe16(bytes, 0);
    AppendLe16(bytes, 0);
    AppendLe16(bytes, 0);
    AppendLe32(bytes, record.crc);
    AppendLe32(bytes, static_cast<std::uint32_t>(payload.size()));
    AppendLe32(bytes, static_cast<std::uint32_t>(payload.size()));
    AppendLe16(bytes, static_cast<std::uint16_t>(name.size()));
    AppendLe16(bytes, 0);
    bytes.insert(bytes.end(), name.begin(), name.end());
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    records.push_back(std::move(record));
  }
  if (bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
    error = "ZIP fixture local records exceed classic ZIP limits";
    return false;
  }
  const std::uint32_t centralOffset = static_cast<std::uint32_t>(bytes.size());
  for (const auto &record : records) {
    AppendLe32(bytes, 0x02014b50);
    AppendLe16(bytes, 20);
    AppendLe16(bytes, 20);
    AppendLe16(bytes, 0x0800);
    AppendLe16(bytes, 0);
    AppendLe16(bytes, 0);
    AppendLe16(bytes, 0);
    AppendLe32(bytes, record.crc);
    AppendLe32(bytes, static_cast<std::uint32_t>(record.payload.size()));
    AppendLe32(bytes, static_cast<std::uint32_t>(record.payload.size()));
    AppendLe16(bytes, static_cast<std::uint16_t>(record.name.size()));
    AppendLe16(bytes, 0);
    AppendLe16(bytes, 0);
    AppendLe16(bytes, 0);
    AppendLe16(bytes, 0);
    AppendLe32(bytes, !record.name.empty() && record.name.back() == '/' ? 0x10 : 0);
    AppendLe32(bytes, record.localOffset);
    bytes.insert(bytes.end(), record.name.begin(), record.name.end());
  }
  const std::uint64_t centralSize64 = bytes.size() - centralOffset;
  if (centralSize64 > std::numeric_limits<std::uint32_t>::max()) {
    error = "ZIP fixture central directory exceeds classic ZIP limits";
    return false;
  }
  AppendLe32(bytes, 0x06054b50);
  AppendLe16(bytes, 0);
  AppendLe16(bytes, 0);
  AppendLe16(bytes, static_cast<std::uint16_t>(records.size()));
  AppendLe16(bytes, static_cast<std::uint16_t>(records.size()));
  AppendLe32(bytes, static_cast<std::uint32_t>(centralSize64));
  AppendLe32(bytes, centralOffset);
  AppendLe16(bytes, 0);
  std::ofstream output(archivePath, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output.good()) {
    error = "Could not write raw ZIP fixture";
    return false;
  }
  error.clear();
  return true;
}

// Returns local-header entry names exactly as stored in the ZIP archive.
inline std::vector<std::string> ReadRawLocalHeaderEntryNames(
    const std::string &archivePath, std::string &error) {
  const auto bytes = ReadBinaryFile(archivePath);
  std::vector<std::string> names;
  std::size_t cursor = 0;
  while (cursor + 4 <= bytes.size() && ReadLe32(bytes, cursor) == 0x04034b50) {
    if (cursor + 30 > bytes.size()) {
      error = "ZIP local header is truncated";
      return {};
    }
    const std::uint16_t nameLength = ReadLe16(bytes, cursor + 26);
    const std::uint16_t extraLength = ReadLe16(bytes, cursor + 28);
    const std::uint32_t payloadSize = ReadLe32(bytes, cursor + 18);
    const std::size_t nameOffset = cursor + 30;
    const std::uint64_t next = static_cast<std::uint64_t>(nameOffset) +
                               nameLength + extraLength + payloadSize;
    if (next > bytes.size()) {
      error = "ZIP local record exceeds file bounds";
      return {};
    }
    names.emplace_back(reinterpret_cast<const char *>(bytes.data() + nameOffset),
                       nameLength);
    cursor = static_cast<std::size_t>(next);
  }
  if (names.empty())
    error = "ZIP contains no local entry headers";
  return names;
}

// Replaces one local-header name with same-length bytes to create a mismatch.
inline bool ReplaceRawLocalHeaderName(const std::string &archivePath,
                                      std::size_t entryIndex,
                                      const std::string &replacement,
                                      std::string &error) {
  auto bytes = ReadBinaryFile(archivePath);
  std::size_t cursor = 0;
  for (std::size_t index = 0; index <= entryIndex; ++index) {
    if (cursor + 30 > bytes.size() || ReadLe32(bytes, cursor) != 0x04034b50) {
      error = "Requested ZIP local entry was not found";
      return false;
    }
    const std::uint16_t nameLength = ReadLe16(bytes, cursor + 26);
    const std::uint16_t extraLength = ReadLe16(bytes, cursor + 28);
    const std::uint32_t payloadSize = ReadLe32(bytes, cursor + 18);
    if (index == entryIndex) {
      if (replacement.size() != nameLength) {
        error = "Replacement ZIP local name has a different length";
        return false;
      }
      std::copy(replacement.begin(), replacement.end(),
                bytes.begin() + static_cast<std::ptrdiff_t>(cursor + 30));
      std::ofstream output(archivePath, std::ios::binary | std::ios::trunc);
      output.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
      if (!output.good()) {
        error = "Could not update ZIP local entry name";
        return false;
      }
      error.clear();
      return true;
    }
    const std::uint64_t next = static_cast<std::uint64_t>(cursor) + 30 +
                               nameLength + extraLength + payloadSize;
    if (next > bytes.size()) {
      error = "ZIP local record exceeds file bounds";
      return false;
    }
    cursor = static_cast<std::size_t>(next);
  }
  error = "Requested ZIP local entry was not found";
  return false;
}

// Returns central-directory entry names exactly as stored in the ZIP archive.
inline std::vector<std::string> ReadRawCentralDirectoryEntryNames(
    const std::string &archivePath, std::string &error) {
  const std::vector<unsigned char> bytes = ReadBinaryFile(archivePath);
  if (bytes.size() < 22) {
    error = "ZIP file is too small to contain an end-of-central-directory record";
    return {};
  }

  std::size_t eocdOffset = std::string::npos;
  const std::size_t maxComment = 0xffff;
  const std::size_t minOffset = bytes.size() > 22 + maxComment
                                    ? bytes.size() - 22 - maxComment
                                    : 0;
  for (std::size_t offset = bytes.size() - 22;; --offset) {
    if (ReadLe32(bytes, offset) == 0x06054b50) {
      eocdOffset = offset;
      break;
    }
    if (offset == minOffset)
      break;
  }
  if (eocdOffset == std::string::npos) {
    error = "ZIP end-of-central-directory record was not found";
    return {};
  }

  const std::uint16_t entryCount = ReadLe16(bytes, eocdOffset + 10);
  const std::uint32_t centralDirectorySize = ReadLe32(bytes, eocdOffset + 12);
  const std::uint32_t centralDirectoryOffset = ReadLe32(bytes, eocdOffset + 16);
  if (centralDirectoryOffset > bytes.size() ||
      centralDirectorySize > bytes.size() - centralDirectoryOffset ||
      centralDirectoryOffset + centralDirectorySize > eocdOffset) {
    error = "ZIP central-directory bounds are invalid";
    return {};
  }

  std::vector<std::string> names;
  std::size_t cursor = centralDirectoryOffset;
  for (std::uint16_t index = 0; index < entryCount; ++index) {
    if (cursor + 46 > bytes.size() || ReadLe32(bytes, cursor) != 0x02014b50) {
      error = "ZIP central-directory entry header is invalid";
      return {};
    }
    const std::uint16_t nameLength = ReadLe16(bytes, cursor + 28);
    const std::uint16_t extraLength = ReadLe16(bytes, cursor + 30);
    const std::uint16_t commentLength = ReadLe16(bytes, cursor + 32);
    const std::size_t nameOffset = cursor + 46;
    if (nameOffset + nameLength > bytes.size()) {
      error = "ZIP central-directory entry name exceeds file bounds";
      return {};
    }
    names.emplace_back(reinterpret_cast<const char *>(bytes.data() + nameOffset),
                       nameLength);
    const std::size_t next = nameOffset + nameLength + extraLength + commentLength;
    if (next < cursor || next > centralDirectoryOffset + centralDirectorySize) {
      error = "ZIP central-directory entry extent is invalid";
      return {};
    }
    cursor = next;
  }
  return names;
}

} // namespace tests::archive
