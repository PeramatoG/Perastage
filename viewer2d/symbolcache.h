#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "canvas2d.h"

// Describes the orientation used when capturing or instancing a 2D symbol.
enum class SymbolViewKind {
  Top,
  Bottom,
  Left,
  Right,
  Front,
  Back,
};

struct SymbolPoint {
  float x = 0.0f;
  float y = 0.0f;
};

struct SymbolBounds {
  SymbolPoint min{};
  SymbolPoint max{};
};

struct SymbolKey {
  std::string modelKey;
  SymbolViewKind viewKind = SymbolViewKind::Top;
  uint32_t styleVersion = 1;

  bool operator==(const SymbolKey &other) const {
    return modelKey == other.modelKey && viewKind == other.viewKind &&
           styleVersion == other.styleVersion;
  }
};

namespace std {
template <> struct hash<SymbolKey> {
  size_t operator()(const SymbolKey &key) const noexcept {
    size_t h1 = hash<string>{}(key.modelKey);
    size_t h2 = hash<int>{}(static_cast<int>(key.viewKind));
    size_t h3 = hash<uint32_t>{}(key.styleVersion);

    size_t seed = h1;
    seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }
};
} // namespace std

struct SymbolDefinition {
  SymbolKey key{};
  uint32_t symbolId = 0;
  SymbolBounds bounds{};
  CommandBuffer localCommands{};
};

using SymbolDefinitionSnapshot =
    std::unordered_map<uint32_t, SymbolDefinition>;

class SymbolCache {
public:
  using BuilderFn = std::function<SymbolDefinition(const SymbolKey &, uint32_t)>;

  const SymbolDefinition &GetOrCreate(const SymbolKey &key,
                                      const BuilderFn &builder);
  const SymbolDefinition *GetById(uint32_t id) const;
  std::shared_ptr<SymbolDefinitionSnapshot> Snapshot() const;

  uint64_t HitCount() const { return hits_; }
  uint64_t MissCount() const { return misses_; }

private:
  std::unordered_map<SymbolKey, SymbolDefinition> definitions_;
  std::unordered_map<uint32_t, SymbolKey> idToKey_;
  uint32_t nextSymbolId_ = 1;
  uint64_t hits_ = 0;
  uint64_t misses_ = 0;
};
