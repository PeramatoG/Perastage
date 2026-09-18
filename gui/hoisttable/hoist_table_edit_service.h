#pragma once

#include "mvrscene.h"
#include "dummyprofilelibrary.h"
#include "units/units.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class wxDataViewListCtrl;

namespace HoistTableEditService {

class ISceneAdapter {
public:
  virtual ~ISceneAdapter() = default;
  virtual void PushUndoState(const std::string &description) = 0;
  virtual MvrScene &GetScene() = 0;
  virtual Units::DistanceUnitSystem GetDistanceUnitSystem() const = 0;
  virtual Units::WeightUnitSystem GetWeightUnitSystem() const = 0;
  virtual std::optional<DummyHoistProfile>
  FindDummyProfileById(const std::string &profileId) const = 0;
  virtual std::optional<DummyHoistProfile>
  FindDummyProfileByDisplayName(const std::string &displayName) const = 0;
};

struct Request {
  wxDataViewListCtrl *table = nullptr;
  const std::vector<std::string> &rowUuids;
  const std::unordered_map<std::string, float> &pendingAutomaticLoadByUuid;
};

struct Result {
  bool anyChanged = false;
  std::unordered_set<std::string> changedWeightPositions;
};

// Applies committed hoist table rows to their UUID-matched scene supports.
Result UpdateSceneData(ISceneAdapter &adapter, const Request &request);

} // namespace HoistTableEditService
