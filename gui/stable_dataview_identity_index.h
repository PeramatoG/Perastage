#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gui {

struct DataViewResourceMetadata {
  std::string modelPath;
  std::string symbolPath;
};

class StableDataViewIdentityIndex {
public:
  bool Add(std::uintptr_t itemKey, const std::string &uuid,
           DataViewResourceMetadata metadata = {});
  void RemoveKey(std::uintptr_t itemKey);
  void Clear();

  std::string UuidForKey(std::uintptr_t itemKey) const;
  std::optional<std::uintptr_t> KeyForUuid(const std::string &uuid) const;
  const DataViewResourceMetadata *MetadataForKey(std::uintptr_t itemKey) const;
  void SetMetadata(std::uintptr_t itemKey, DataViewResourceMetadata metadata);

  std::vector<std::string> UuidsForKeys(
      const std::vector<std::uintptr_t> &itemKeys) const;
  std::vector<std::string> PruneExistingUuids(
      const std::vector<std::string> &uuids) const;
  bool HasDuplicateUuid() const;

private:
  std::unordered_map<std::uintptr_t, std::string> uuidByKey;
  std::unordered_map<std::string, std::uintptr_t> keyByUuid;
  std::unordered_map<std::uintptr_t, DataViewResourceMetadata> metadataByKey;
  bool duplicateUuid = false;
};

} // namespace gui
