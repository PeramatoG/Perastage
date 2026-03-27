#include "file_import_utils.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace FileImportUtils {
namespace {

inline uint32_t RotRight(uint32_t value, uint32_t bits) {
  return (value >> bits) | (value << (32 - bits));
}

struct Sha256State {
  std::array<uint32_t, 8> h = {
      0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  uint64_t totalBytes = 0;
  std::array<uint8_t, 64> buffer{};
  size_t bufferSize = 0;
};

constexpr std::array<uint32_t, 64> kSha256K = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

void ProcessBlock(Sha256State &state, const uint8_t *data) {
  uint32_t w[64]{};
  for (size_t i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(data[i * 4]) << 24) |
           (static_cast<uint32_t>(data[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(data[i * 4 + 2]) << 8) |
           static_cast<uint32_t>(data[i * 4 + 3]);
  }
  for (size_t i = 16; i < 64; ++i) {
    const uint32_t s0 = RotRight(w[i - 15], 7) ^ RotRight(w[i - 15], 18) ^
                        (w[i - 15] >> 3);
    const uint32_t s1 = RotRight(w[i - 2], 17) ^ RotRight(w[i - 2], 19) ^
                        (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = state.h[0];
  uint32_t b = state.h[1];
  uint32_t c = state.h[2];
  uint32_t d = state.h[3];
  uint32_t e = state.h[4];
  uint32_t f = state.h[5];
  uint32_t g = state.h[6];
  uint32_t h = state.h[7];

  for (size_t i = 0; i < 64; ++i) {
    const uint32_t s1 = RotRight(e, 6) ^ RotRight(e, 11) ^ RotRight(e, 25);
    const uint32_t ch = (e & f) ^ (~e & g);
    const uint32_t temp1 = h + s1 + ch + kSha256K[i] + w[i];
    const uint32_t s0 = RotRight(a, 2) ^ RotRight(a, 13) ^ RotRight(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temp2 = s0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  state.h[0] += a;
  state.h[1] += b;
  state.h[2] += c;
  state.h[3] += d;
  state.h[4] += e;
  state.h[5] += f;
  state.h[6] += g;
  state.h[7] += h;
}

void UpdateSha256(Sha256State &state, const uint8_t *data, size_t len) {
  state.totalBytes += len;
  size_t pos = 0;

  if (state.bufferSize > 0) {
    const size_t toCopy = std::min(len, 64 - state.bufferSize);
    std::copy(data, data + toCopy, state.buffer.begin() + state.bufferSize);
    state.bufferSize += toCopy;
    pos += toCopy;
    if (state.bufferSize == 64) {
      ProcessBlock(state, state.buffer.data());
      state.bufferSize = 0;
    }
  }

  while (pos + 64 <= len) {
    ProcessBlock(state, data + pos);
    pos += 64;
  }

  if (pos < len) {
    const size_t remain = len - pos;
    std::copy(data + pos, data + len, state.buffer.begin());
    state.bufferSize = remain;
  }
}

std::string FinalizeSha256(Sha256State &state) {
  std::array<uint8_t, 64> tail{};
  std::copy(state.buffer.begin(), state.buffer.begin() + state.bufferSize,
            tail.begin());
  tail[state.bufferSize] = 0x80;

  if (state.bufferSize >= 56) {
    ProcessBlock(state, tail.data());
    tail.fill(0);
  }

  const uint64_t totalBits = state.totalBytes * 8;
  for (size_t i = 0; i < 8; ++i) {
    tail[56 + i] = static_cast<uint8_t>((totalBits >> (56 - i * 8)) & 0xffu);
  }
  ProcessBlock(state, tail.data());

  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (uint32_t v : state.h)
    oss << std::setw(8) << v;
  return oss.str();
}

} // namespace

std::optional<std::string> ComputeFileSha256(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open())
    return std::nullopt;

  Sha256State state;
  std::array<uint8_t, 16 * 1024> buf{};
  while (in.good()) {
    in.read(reinterpret_cast<char *>(buf.data()),
            static_cast<std::streamsize>(buf.size()));
    const auto bytes = static_cast<size_t>(in.gcount());
    if (bytes > 0)
      UpdateSha256(state, buf.data(), bytes);
  }
  return FinalizeSha256(state);
}

std::string BuildStableRenamedFilename(const std::filesystem::path &path,
                                       const std::string &sha256,
                                       size_t shortLen) {
  const std::string stem = path.stem().string();
  const std::string ext = path.extension().string();
  const std::string shortHash = sha256.substr(0, std::min(shortLen, sha256.size()));
  return stem + "_" + shortHash + ext;
}

std::string NowUtcIso8601() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
  std::tm tmUtc{};
#if defined(_WIN32)
  gmtime_s(&tmUtc, &timeValue);
#else
  gmtime_r(&timeValue, &tmUtc);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tmUtc, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

CopyResult CopyWithConflictPolicy(const std::filesystem::path &sourcePath,
                                  const std::filesystem::path &targetPath,
                                  ConflictPolicy policy) {
  CopyResult result;
  result.finalPath = targetPath;

  const auto srcHashOpt = ComputeFileSha256(sourcePath);
  if (!srcHashOpt)
    return result;
  result.sourceSha256 = *srcHashOpt;

  std::error_code ec;
  std::filesystem::create_directories(targetPath.parent_path(), ec);
  if (ec)
    return result;

  if (std::filesystem::exists(targetPath, ec) && !ec) {
    const auto dstHashOpt = ComputeFileSha256(targetPath);
    if (dstHashOpt && *dstHashOpt == result.sourceSha256) {
      result.success = true;
      result.reusedExisting = true;
      result.finalSha256 = *dstHashOpt;
      return result;
    }

    if (policy == ConflictPolicy::Cancel)
      return result;

    if (policy == ConflictPolicy::Rename) {
      result.finalPath = targetPath.parent_path() /
                         BuildStableRenamedFilename(targetPath, result.sourceSha256);
    }
  }

  std::filesystem::copy_file(sourcePath, result.finalPath,
                             std::filesystem::copy_options::overwrite_existing, ec);
  if (ec)
    return result;

  result.success = true;
  result.copied = true;
  result.finalSha256 = result.sourceSha256;
  return result;
}

} // namespace FileImportUtils
