#include "stable_dataview_identity_index.h"

#include <cassert>
#include <string>
#include <vector>

int main() {
  gui::StableDataViewIdentityIndex index;
  assert(index.Add(10, "truss-a", {"/show/a/model.glb", "/show/a/symbol.glb"}));
  assert(index.Add(20, "truss-b", {"/show/b/model.glb", "/show/b/symbol.glb"}));

  assert(index.UuidForKey(10) == "truss-a");
  assert(index.KeyForUuid("truss-b").value() == 20);

  const auto *metadata = index.MetadataForKey(10);
  assert(metadata != nullptr);
  assert(metadata->modelPath == "/show/a/model.glb");

  const std::vector<std::uintptr_t> sortedKeys{20, 10};
  const std::vector<std::string> sortedUuids = index.UuidsForKeys(sortedKeys);
  assert((sortedUuids == std::vector<std::string>{"truss-b", "truss-a"}));

  const std::vector<std::string> pruned =
      index.PruneExistingUuids({"missing", "truss-a", "truss-a", "truss-b"});
  assert((pruned == std::vector<std::string>{"truss-a", "truss-b"}));

  assert(!index.Add(30, "truss-a"));
  assert(index.HasDuplicateUuid());
  assert(index.UuidForKey(30).empty());

  index.RemoveKey(10);
  assert(!index.KeyForUuid("truss-a").has_value());
  assert(index.MetadataForKey(10) == nullptr);

  index.Clear();
  assert(index.UuidForKey(20).empty());
  assert(!index.HasDuplicateUuid());
  return 0;
}
