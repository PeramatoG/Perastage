#pragma once

#include "query/query_contract.h"
#include "scene_grouping.h"

#include <functional>

class Fixture;
class MvrScene;

namespace perastage::query {

using FootprintResolver = std::function<std::optional<int>(const Fixture &)>;

// Returns stable aggregate information about the active scene.
Summary GetSummary(const MvrScene &scene);
// Returns the semantic selection as sorted typed references.
std::vector<scene_identity::ObjectReference>
GetSelection(const scene_grouping::ObjectSelection &selection);
// Returns all public scene descriptors ordered by kind token and UUID.
std::vector<ObjectDescriptor> ListObjects(const MvrScene &scene);
// Finds one object only when both its kind and UUID match.
std::optional<ObjectDescriptor>
GetObject(const MvrScene &scene, const scene_identity::ObjectReference &object,
          std::vector<Diagnostic> &diagnostics);
// Returns layers and typed membership ordered deterministically.
std::vector<LayerDescriptor> ListLayers(const MvrScene &scene);
// Returns group hierarchy and typed children ordered deterministically.
std::vector<GroupDescriptor> ListGroups(const MvrScene &scene);
// Analyzes fixture patch ranges using a caller-owned footprint source.
PatchStatus GetPatchStatus(const MvrScene &scene,
                           const FootprintResolver &resolveFootprint);

} // namespace perastage::query
