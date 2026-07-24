#pragma once

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
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
