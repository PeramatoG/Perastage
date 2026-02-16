/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */

#include "uuidutils.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <random>

namespace {

uint64_t Fnv1a64(const std::string &value, uint64_t seed) {
  uint64_t hash = 1469598103934665603ull ^ seed;
  for (unsigned char c : value) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string BytesToUuid(const std::array<uint8_t, 16> &bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(36);
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      out.push_back('-');
    out.push_back(kHex[(bytes[i] >> 4) & 0x0F]);
    out.push_back(kHex[bytes[i] & 0x0F]);
  }
  return out;
}

} // namespace

std::string GenerateUuid() {
  static std::mt19937_64 rng{std::random_device{}()};
  static std::uniform_int_distribution<int> dist(0, 15);
  const char *v = "0123456789abcdef";
  int groups[] = {8, 4, 4, 4, 12};
  std::string out;
  for (int g = 0; g < 5; ++g) {
    if (g)
      out.push_back('-');
    for (int i = 0; i < groups[g]; ++i)
      out.push_back(v[dist(rng)]);
  }
  return out;
}

std::string CanonicalizeUuid(const std::string &rawUuid) {
  std::string hex;
  hex.reserve(32);
  for (unsigned char c : rawUuid) {
    if (c == '-' || std::isspace(c))
      continue;
    if (!std::isxdigit(c))
      return {};
    hex.push_back(static_cast<char>(std::tolower(c)));
  }

  if (hex.size() != 32)
    return {};

  std::string out;
  out.reserve(36);
  for (size_t i = 0; i < hex.size(); ++i) {
    if (i == 8 || i == 12 || i == 16 || i == 20)
      out.push_back('-');
    out.push_back(hex[i]);
  }
  return out;
}

std::string DeriveDeterministicUuid(const std::string &seed) {
  const uint64_t a = Fnv1a64(seed, 0x9e3779b97f4a7c15ull);
  const uint64_t b = Fnv1a64(seed, 0xc2b2ae3d27d4eb4full);

  std::array<uint8_t, 16> bytes{};
  for (int i = 0; i < 8; ++i)
    bytes[i] = static_cast<uint8_t>((a >> (56 - i * 8)) & 0xFFu);
  for (int i = 0; i < 8; ++i)
    bytes[8 + i] = static_cast<uint8_t>((b >> (56 - i * 8)) & 0xFFu);

  // Encode as RFC4122 variant + version 5-like deterministic UUID.
  bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0Fu) | 0x50u);
  bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3Fu) | 0x80u);

  return BytesToUuid(bytes);
}
