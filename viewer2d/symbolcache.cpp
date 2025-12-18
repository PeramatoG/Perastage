#include "symbolcache.h"

#include <utility>

const SymbolDefinition &SymbolCache::GetOrCreate(const SymbolKey &key,
                                                 const BuilderFn &builder) {
  auto it = definitions_.find(key);
  if (it != definitions_.end()) {
    ++hits_;
    return it->second;
  }

  ++misses_;
  const uint32_t symbolId = nextSymbolId_++;
  SymbolDefinition definition = builder ? builder(key, symbolId) : SymbolDefinition{};
  if (definition.symbolId == 0) {
    definition.symbolId = symbolId;
  }

  auto inserted = definitions_.emplace(key, std::move(definition));
  idToKey_.emplace(inserted.first->second.symbolId, key);
  return inserted.first->second;
}

const SymbolDefinition *SymbolCache::GetById(uint32_t id) const {
  auto keyIt = idToKey_.find(id);
  if (keyIt == idToKey_.end()) {
    return nullptr;
  }

  auto defIt = definitions_.find(keyIt->second);
  if (defIt == definitions_.end()) {
    return nullptr;
  }

  return &defIt->second;
}

