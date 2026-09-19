#include "archive_zip_directory.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <optional>

namespace perastage::archive::zip {
namespace {
constexpr std::uint32_t kEndOfCentralDirectorySignature = 0x06054b50;
constexpr std::uint32_t kCentralDirectorySignature = 0x02014b50;
constexpr std::uint32_t kLocalHeaderSignature = 0x04034b50;
constexpr std::uint16_t kZip64Uint16Sentinel = 0xffff;
constexpr std::uint32_t kZip64Uint32Sentinel = 0xffffffffU;
constexpr std::uint64_t kMaximumEocdSize = 22ULL + 0xffffULL;

// Reads one little-endian 16-bit ZIP metadata value.
std::uint16_t ReadLe16(const unsigned char *bytes) {
  return static_cast<std::uint16_t>(bytes[0]) |
         (static_cast<std::uint16_t>(bytes[1]) << 8);
}

// Reads one little-endian 32-bit ZIP metadata value.
std::uint32_t ReadLe32(const unsigned char *bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8) |
         (static_cast<std::uint32_t>(bytes[2]) << 16) |
         (static_cast<std::uint32_t>(bytes[3]) << 24);
}

// Reads an exact bounded byte range without allocating from ZIP offsets.
bool ReadBytes(std::ifstream &input, std::uint64_t offset, void *buffer,
               std::size_t size) {
  if (offset > static_cast<std::uint64_t>(
                   std::numeric_limits<std::streamoff>::max()) ||
      size >
          static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
    return false;
  input.clear();
  input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!input.good())
    return false;
  input.read(static_cast<char *>(buffer), static_cast<std::streamsize>(size));
  return input.gcount() == static_cast<std::streamsize>(size);
}

// Locates the real EOCD by requiring its comment to end at the file boundary.
std::optional<std::size_t>
FindEndOfCentralDirectory(const std::vector<unsigned char> &tail) {
  if (tail.size() < 22)
    return std::nullopt;
  for (std::size_t offset = tail.size() - 22;; --offset) {
    if (ReadLe32(tail.data() + offset) == kEndOfCentralDirectorySignature &&
        offset + 22 + ReadLe16(tail.data() + offset + 20) == tail.size())
      return offset;
    if (offset == 0)
      break;
  }
  return std::nullopt;
}

// Returns a structural parse failure without partially trusted entries.
DirectoryReadResult Fail(DirectoryReadStatus status) { return {status, {}}; }
} // namespace

// Reports whether classic ZIP directory metadata was read completely.
bool DirectoryReadResult::Success() const {
  return status == DirectoryReadStatus::Success;
}

// Reports whether bytes contain one well-formed UTF-8 sequence.
bool IsValidUtf8(const std::string &text) {
  for (std::size_t index = 0; index < text.size();) {
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    std::size_t continuationCount = 0;
    std::uint32_t codePoint = 0;
    if (lead <= 0x7f) {
      ++index;
      continue;
    }
    if (lead >= 0xc2 && lead <= 0xdf) {
      continuationCount = 1;
      codePoint = lead & 0x1f;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      continuationCount = 2;
      codePoint = lead & 0x0f;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      continuationCount = 3;
      codePoint = lead & 0x07;
    } else {
      return false;
    }
    if (continuationCount > text.size() - index - 1)
      return false;
    for (std::size_t part = 1; part <= continuationCount; ++part) {
      const unsigned char byte = static_cast<unsigned char>(text[index + part]);
      if ((byte & 0xc0) != 0x80)
        return false;
      codePoint = (codePoint << 6) | (byte & 0x3f);
    }
    if ((continuationCount == 2 && codePoint < 0x800) ||
        (continuationCount == 3 && codePoint < 0x10000) ||
        (codePoint >= 0xd800 && codePoint <= 0xdfff) || codePoint > 0x10ffff)
      return false;
    index += continuationCount + 1;
  }
  return true;
}

// Reads bounded, structurally validated classic-ZIP central-directory metadata.
DirectoryReadResult ReadDirectory(const std::filesystem::path &archivePath) {
  std::ifstream input(archivePath, std::ios::binary);
  if (!input.is_open())
    return Fail(DirectoryReadStatus::OpenFailed);
  input.seekg(0, std::ios::end);
  const std::streamoff endPosition = input.tellg();
  if (endPosition < 22)
    return Fail(DirectoryReadStatus::Malformed);
  const std::uint64_t fileSize = static_cast<std::uint64_t>(endPosition);
  const std::size_t tailSize = static_cast<std::size_t>(
      std::min<std::uint64_t>(fileSize, kMaximumEocdSize));
  std::vector<unsigned char> tail(tailSize);
  if (!ReadBytes(input, fileSize - tailSize, tail.data(), tail.size()))
    return Fail(DirectoryReadStatus::Malformed);

  const std::optional<std::size_t> eocdOffset = FindEndOfCentralDirectory(tail);
  if (!eocdOffset)
    return Fail(DirectoryReadStatus::Malformed);
  const unsigned char *eocd = tail.data() + *eocdOffset;
  const std::uint16_t disk = ReadLe16(eocd + 4);
  const std::uint16_t centralDisk = ReadLe16(eocd + 6);
  const std::uint16_t entriesOnDisk = ReadLe16(eocd + 8);
  const std::uint16_t entryCount = ReadLe16(eocd + 10);
  const std::uint32_t centralSize = ReadLe32(eocd + 12);
  const std::uint32_t centralOffset = ReadLe32(eocd + 16);
  if (disk != 0 || centralDisk != 0 || entriesOnDisk != entryCount)
    return Fail(DirectoryReadStatus::MultiDiskUnsupported);
  if (entryCount == kZip64Uint16Sentinel ||
      centralSize == kZip64Uint32Sentinel ||
      centralOffset == kZip64Uint32Sentinel)
    return Fail(DirectoryReadStatus::Zip64Unsupported);
  const std::uint64_t eocdFileOffset = fileSize - tailSize + *eocdOffset;
  if (centralOffset > fileSize || centralSize > fileSize - centralOffset ||
      static_cast<std::uint64_t>(centralOffset) + centralSize != eocdFileOffset)
    return Fail(DirectoryReadStatus::Malformed);

  DirectoryReadResult result{DirectoryReadStatus::Success, {}};
  result.entries.reserve(entryCount);
  std::uint64_t cursor = centralOffset;
  const std::uint64_t centralEnd = cursor + centralSize;
  for (std::uint16_t index = 0; index < entryCount; ++index) {
    std::array<unsigned char, 46> central{};
    if (cursor > centralEnd || centralEnd - cursor < central.size() ||
        !ReadBytes(input, cursor, central.data(), central.size()) ||
        ReadLe32(central.data()) != kCentralDirectorySignature)
      return Fail(DirectoryReadStatus::Malformed);
    const std::uint16_t flags = ReadLe16(central.data() + 8);
    const std::uint32_t uncompressedSize = ReadLe32(central.data() + 24);
    const std::uint16_t nameLength = ReadLe16(central.data() + 28);
    const std::uint16_t extraLength = ReadLe16(central.data() + 30);
    const std::uint16_t commentLength = ReadLe16(central.data() + 32);
    const std::uint16_t startDisk = ReadLe16(central.data() + 34);
    const std::uint32_t localOffset = ReadLe32(central.data() + 42);
    const std::uint64_t recordSize =
        46ULL + nameLength + extraLength + commentLength;
    if (startDisk != 0)
      return Fail(DirectoryReadStatus::MultiDiskUnsupported);
    if (uncompressedSize == kZip64Uint32Sentinel)
      return Fail(DirectoryReadStatus::Zip64Unsupported);
    if (nameLength == 0 || recordSize > centralEnd - cursor)
      return Fail(DirectoryReadStatus::Malformed);
    std::string centralName(nameLength, '\0');
    if (!ReadBytes(input, cursor + 46, centralName.data(), nameLength))
      return Fail(DirectoryReadStatus::Malformed);

    std::array<unsigned char, 30> local{};
    if (localOffset == kZip64Uint32Sentinel)
      return Fail(DirectoryReadStatus::Zip64Unsupported);
    if (localOffset >= centralOffset ||
        centralOffset - localOffset < local.size() ||
        !ReadBytes(input, localOffset, local.data(), local.size()) ||
        ReadLe32(local.data()) != kLocalHeaderSignature)
      return Fail(DirectoryReadStatus::Malformed);
    const std::uint16_t localNameLength = ReadLe16(local.data() + 26);
    const std::uint16_t localExtraLength = ReadLe16(local.data() + 28);
    if (30ULL + localNameLength + localExtraLength >
        centralOffset - localOffset)
      return Fail(DirectoryReadStatus::Malformed);
    std::string localName(localNameLength, '\0');
    if (!ReadBytes(input, static_cast<std::uint64_t>(localOffset) + 30,
                   localName.data(), localNameLength) ||
        localName != centralName)
      return Fail(DirectoryReadStatus::Malformed);

    const bool directory = !centralName.empty() && centralName.back() == '/';
    result.entries.push_back({std::move(centralName), (flags & (1U << 11)) != 0,
                              uncompressedSize, directory});
    cursor += recordSize;
  }
  if (cursor != centralEnd)
    return Fail(DirectoryReadStatus::Malformed);
  return result;
}

} // namespace perastage::archive::zip
