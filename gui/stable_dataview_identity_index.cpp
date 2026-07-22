#include "stable_dataview_identity_index.h"

#include <unordered_set>

namespace gui {

// Adds or replaces a stable table item identity mapping.
bool StableDataViewIdentityIndex::Add(std::uintptr_t itemKey,
                                      const std::string &uuid,
                                      DataViewResourceMetadata metadata) {
  if (itemKey == 0 || uuid.empty())
    return false;
  RemoveKey(itemKey);
  if (auto existing = keyByUuid.find(uuid); existing != keyByUuid.end() &&
                                            existing->second != itemKey) {
    duplicateUuid = true;
    return false;
  }
  uuidByKey[itemKey] = uuid;
  keyByUuid[uuid] = itemKey;
  metadataByKey[itemKey] = std::move(metadata);
  return true;
}

// Removes all identity and metadata associated with a table item key.
void StableDataViewIdentityIndex::RemoveKey(std::uintptr_t itemKey) {
  auto it = uuidByKey.find(itemKey);
  if (it != uuidByKey.end()) {
    keyByUuid.erase(it->second);
    uuidByKey.erase(it);
  }
  metadataByKey.erase(itemKey);
}

// Clears every table item identity mapping.
void StableDataViewIdentityIndex::Clear() {
  uuidByKey.clear();
  keyByUuid.clear();
  metadataByKey.clear();
  duplicateUuid = false;
}

// Returns the scene UUID associated with a stable table item key.
std::string StableDataViewIdentityIndex::UuidForKey(std::uintptr_t itemKey) const {
  auto it = uuidByKey.find(itemKey);
  return it == uuidByKey.end() ? std::string{} : it->second;
}

// Returns the stable table item key associated with a scene UUID.
std::optional<std::uintptr_t>
StableDataViewIdentityIndex::KeyForUuid(const std::string &uuid) const {
  auto it = keyByUuid.find(uuid);
  if (it == keyByUuid.end())
    return std::nullopt;
  return it->second;
}

// Returns resource metadata attached to a stable table item key.
const DataViewResourceMetadata *
StableDataViewIdentityIndex::MetadataForKey(std::uintptr_t itemKey) const {
  auto it = metadataByKey.find(itemKey);
  return it == metadataByKey.end() ? nullptr : &it->second;
}

// Replaces resource metadata attached to a stable table item key.
void StableDataViewIdentityIndex::SetMetadata(
    std::uintptr_t itemKey, DataViewResourceMetadata metadata) {
  if (uuidByKey.contains(itemKey))
    metadataByKey[itemKey] = std::move(metadata);
}

// Resolves table item keys to scene UUIDs while preserving input order.
std::vector<std::string> StableDataViewIdentityIndex::UuidsForKeys(
    const std::vector<std::uintptr_t> &itemKeys) const {
  std::vector<std::string> uuids;
  uuids.reserve(itemKeys.size());
  for (const auto key : itemKeys) {
    const std::string uuid = UuidForKey(key);
    if (!uuid.empty())
      uuids.push_back(uuid);
  }
  return uuids;
}

// Keeps only UUIDs that are still present in the table identity index.
std::vector<std::string> StableDataViewIdentityIndex::PruneExistingUuids(
    const std::vector<std::string> &uuids) const {
  std::vector<std::string> pruned;
  std::unordered_set<std::string> seen;
  pruned.reserve(uuids.size());
  for (const auto &uuid : uuids) {
    if (keyByUuid.contains(uuid) && seen.insert(uuid).second)
      pruned.push_back(uuid);
  }
  return pruned;
}

// Reports whether duplicate UUID mappings were rejected.
bool StableDataViewIdentityIndex::HasDuplicateUuid() const {
  return duplicateUuid;
}

} // namespace gui
