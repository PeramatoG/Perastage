#pragma once

#include "mvrscene.h"
#include "units/units.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <wx/string.h>

class wxDataViewListCtrl;

namespace TrussTableEditService {

class ISceneAdapter {
public:
  virtual ~ISceneAdapter() = default;
  virtual void PushUndoState(const std::string &description) = 0;
  virtual MvrScene &GetScene() = 0;
  virtual Units::DistanceUnitSystem GetDistanceUnitSystem() const = 0;
  virtual Units::WeightUnitSystem GetWeightUnitSystem() const = 0;
};

struct Result {
  bool anyChanged = false;
  std::vector<std::pair<std::string, std::string>> updatedTrusses;
  std::unordered_set<std::string> changedWeightPositions;
};

// Applies committed truss table rows to the scene and synchronizes equal types.
Result UpdateSceneData(ISceneAdapter &adapter, wxDataViewListCtrl *table,
                       const std::vector<std::string> &rowUuids,
                       const std::vector<wxString> &modelPaths,
                       const std::vector<wxString> &symbolPaths);

} // namespace TrussTableEditService
