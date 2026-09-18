#include "hoist_table_edit_service.h"

#include "matrixutils.h"
#include "scene_node_operations.h"
#include "support.h"
#include "table_column_indices.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <wx/dataview.h>

namespace HoistTableEditService {
namespace {

using HoistColumn = HoistTableColumns::Column;

// Converts a hoist column to its stable model index.
constexpr int ColumnIndex(HoistColumn column) {
  return TableColumnIndices::ToIndex(column);
}

// Returns the degree symbol used by rotation cells.
const wxString &DegreeSymbol() {
  static const wxString kDegreeSymbol = wxString::FromUTF8("\xC2\xB0");
  return kDegreeSymbol;
}

// Normalizes empty position names for load recalculation tracking.
std::string NormalizePositionName(const std::string &positionName) {
  return positionName.empty() ? "Unassigned" : positionName;
}

// Compares source-resolved numeric values without changing existing precision.
bool NearlyEqualFloat(float a, float b) {
  return std::abs(a - b) < 0.0001f;
}

// Preserves inherited text unless the displayed effective value was edited.
void MarkTextFieldManualIfEdited(const std::string &editedValue,
                                 const std::string &oldEffectiveValue,
                                 std::string &fieldSource,
                                 const std::string &oldFieldValue,
                                 std::string &fieldValue) {
  if (editedValue != oldEffectiveValue) {
    fieldSource = "Manual";
    fieldValue = editedValue;
    return;
  }
  if (!IsManualHoistDataSource(fieldSource))
    fieldValue = oldFieldValue;
}

// Preserves inherited numeric data unless the displayed effective value changed.
void MarkNumericFieldManualIfEdited(float editedValue, float oldEffectiveValue,
                                    std::string &fieldSource,
                                    float oldFieldValue, float &fieldValue) {
  if (!NearlyEqualFloat(editedValue, oldEffectiveValue)) {
    fieldSource = "Manual";
    fieldValue = editedValue;
    return;
  }
  if (!IsManualHoistDataSource(fieldSource))
    fieldValue = oldFieldValue;
}

// Resolves preset defaults using the stable profile ID before the legacy label.
std::optional<HoistPresetDefaults>
FindPresetDefaults(const ISceneAdapter &adapter, const Support &support) {
  std::optional<DummyHoistProfile> profile;
  if (!support.dummyProfileId.empty())
    profile = adapter.FindDummyProfileById(support.dummyProfileId);
  if (!profile.has_value() && !support.dummyPreset.empty())
    profile = adapter.FindDummyProfileByDisplayName(support.dummyPreset);
  if (!profile.has_value())
    return std::nullopt;
  return HoistPresetDefaults{profile->motorName, profile->motorManufacturer,
                             profile->motorModel, profile->capacityKg,
                             profile->weightKg, profile->hoistFunction};
}

// Resolves defaults from the fixture linked to a support motor.
std::optional<HoistFixtureDefaults>
FindFixtureDefaults(const MvrScene &scene, const Support &support) {
  if (support.motorFixtureUuid.empty())
    return std::nullopt;
  const auto it = scene.fixtures.find(support.motorFixtureUuid);
  if (it == scene.fixtures.end())
    return std::nullopt;
  return BuildHoistFixtureDefaults(it->second);
}

} // namespace

// Applies committed hoist table rows to their UUID-matched scene supports.
Result UpdateSceneData(ISceneAdapter &adapter, const Request &request) {
  Result result;
  if (!request.table)
    return result;

  auto &scene = adapter.GetScene();
  const auto distanceUnit = adapter.GetDistanceUnitSystem();
  const auto weightUnit = adapter.GetWeightUnitSystem();
  const size_t count = std::min(static_cast<size_t>(request.table->GetItemCount()),
                                request.rowUuids.size());
  bool undoPushed = false;
  const auto pushUndoIfNeeded = [&]() {
    if (!undoPushed) {
      adapter.PushUndoState("edit support");
      undoPushed = true;
    }
  };

  for (size_t row = 0; row < count; ++row) {
    auto it = scene.supports.find(request.rowUuids[row]);
    if (it == scene.supports.end())
      continue;
    const Support old = it->second;
    Support next = old;
    wxVariant value;
    const auto readText = [&](HoistColumn column) {
      request.table->GetValue(value, row, ColumnIndex(column));
      return std::string(value.GetString().ToUTF8());
    };

    next.name = readText(HoistColumn::Name);
    next.function = readText(HoistColumn::Type);
    const auto oldEffective = ResolveEffectiveSupportData(
        old, FindPresetDefaults(adapter, old), FindFixtureDefaults(scene, old));
    const std::string editedHoistFunction =
        NormalizeHoistFunction(readText(HoistColumn::Function));
    next.hoistFunction = editedHoistFunction;
    const std::string editedMotorName = readText(HoistColumn::Motor);
    next.motorName = editedMotorName;
    next.dummyPreset = readText(HoistColumn::DummyPreset);
    if (next.dummyPreset.empty())
      next.dummyProfileId.clear();
    else {
      const auto profile =
          adapter.FindDummyProfileByDisplayName(next.dummyPreset);
      next.dummyProfileId = profile.has_value() ? profile->id : "";
    }
    next.layer = readText(HoistColumn::Layer);
    next.positionName = readText(HoistColumn::HangPosition);

    double xMm = old.transform.o[0];
    double yMm = old.transform.o[1];
    double zMm = old.transform.o[2];
    const auto readDistance = [&](HoistColumn column, double &target) {
      const auto parsed = Units::ParseDistanceToMillimeters(readText(column),
                                                            distanceUnit);
      if (parsed.has_value())
        target = *parsed;
    };
    readDistance(HoistColumn::PositionX, xMm);
    readDistance(HoistColumn::PositionY, yMm);
    readDistance(HoistColumn::PositionZ, zMm);

    double roll = 0.0, pitch = 0.0, yaw = 0.0;
    const auto readRotation = [&](HoistColumn column, double &target) {
      wxString text = wxString::FromUTF8(readText(column));
      text.Replace(DegreeSymbol(), "");
      text.ToDouble(&target);
    };
    readRotation(HoistColumn::Roll, roll);
    readRotation(HoistColumn::Pitch, pitch);
    readRotation(HoistColumn::Yaw, yaw);
    const auto currentEuler = MatrixUtils::MatrixToEuler(old.transform);
    const bool transformChanged =
        !Units::NearlyEqualDistanceMillimeters(old.transform.o[0], xMm, 0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.transform.o[1], yMm, 0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.transform.o[2], zMm, 0.5) ||
        std::abs(static_cast<double>(currentEuler[2]) - roll) > 0.05 ||
        std::abs(static_cast<double>(currentEuler[1]) - pitch) > 0.05 ||
        std::abs(static_cast<double>(currentEuler[0]) - yaw) > 0.05;
    if (transformChanged) {
      const Matrix rotation = MatrixUtils::EulerToMatrix(
          static_cast<float>(yaw), static_cast<float>(pitch),
          static_cast<float>(roll));
      next.transform = MatrixUtils::ApplyRotationPreservingScale(
          old.transform, rotation,
          {static_cast<float>(xMm), static_cast<float>(yMm),
           static_cast<float>(zMm)});
    }

    double chainLength = 0.0;
    wxString::FromUTF8(readText(HoistColumn::ChainLength)).ToDouble(&chainLength);
    next.chainLength = static_cast<float>(chainLength);
    float editedCapacityKg = old.capacityKg;
    if (const auto parsed = Units::ParseWeightToKilograms(
            readText(HoistColumn::Capacity), weightUnit)) {
      editedCapacityKg = static_cast<float>(*parsed);
      next.capacityKg = editedCapacityKg;
    }
    float editedWeightKg = old.weightKg;
    if (const auto parsed = Units::ParseWeightToKilograms(
            readText(HoistColumn::Weight), weightUnit)) {
      editedWeightKg = static_cast<float>(*parsed);
      next.weightKg = editedWeightKg;
    }
    if (const auto parsed = Units::ParseWeightToKilograms(
            readText(HoistColumn::Load), weightUnit)) {
      next.loadKg = static_cast<float>(*parsed);
      const auto automatic = request.pendingAutomaticLoadByUuid.find(old.uuid);
      if (automatic != request.pendingAutomaticLoadByUuid.end()) {
        next.loadSource = ShouldUseAutomaticHoistLoad(
                              old.loadKg, next.loadKg, automatic->second)
                              ? "Auto"
                              : "Manual";
        if (next.loadSource == "Auto")
          next.loadKg = automatic->second;
      }
    }

    next.motorNameSource = ResolveHoistFieldDataSource(
        next.motorNameSource, next.hoistDataSource);
    next.capacitySource = ResolveHoistFieldDataSource(
        next.capacitySource, next.hoistDataSource);
    next.weightSource = ResolveHoistFieldDataSource(
        next.weightSource, next.hoistDataSource);
    next.hoistFunctionSource = ResolveHoistFieldDataSource(
        next.hoistFunctionSource, next.hoistDataSource);
    MarkTextFieldManualIfEdited(editedMotorName, oldEffective.motorName,
                                next.motorNameSource, old.motorName,
                                next.motorName);
    MarkNumericFieldManualIfEdited(editedCapacityKg, oldEffective.capacityKg,
                                   next.capacitySource, old.capacityKg,
                                   next.capacityKg);
    MarkNumericFieldManualIfEdited(editedWeightKg, oldEffective.weightKg,
                                   next.weightSource, old.weightKg,
                                   next.weightKg);
    MarkTextFieldManualIfEdited(editedHoistFunction, oldEffective.hoistFunction,
                                next.hoistFunctionSource, old.hoistFunction,
                                next.hoistFunction);

    const bool supportChanged =
        old.name != next.name || old.function != next.function ||
        old.hoistFunction != next.hoistFunction ||
        old.motorName != next.motorName ||
        old.motorManufacturer != next.motorManufacturer ||
        old.motorModel != next.motorModel ||
        old.dummyProfileId != next.dummyProfileId ||
        old.dummyPreset != next.dummyPreset ||
        NormalizeHoistDataSource(old.hoistDataSource) !=
            NormalizeHoistDataSource(next.hoistDataSource) ||
        old.layer != next.layer || old.positionName != next.positionName ||
        transformChanged || old.chainLength != next.chainLength ||
        !Units::NearlyEqualWeightKilograms(old.capacityKg, next.capacityKg,
                                           0.001) ||
        !Units::NearlyEqualWeightKilograms(old.weightKg, next.weightKg, 0.001) ||
        !Units::NearlyEqualWeightKilograms(old.loadKg, next.loadKg, 0.001) ||
        old.loadSource != next.loadSource ||
        NormalizeHoistDataSource(old.motorNameSource) !=
            NormalizeHoistDataSource(next.motorNameSource) ||
        NormalizeHoistDataSource(old.motorManufacturerSource) !=
            NormalizeHoistDataSource(next.motorManufacturerSource) ||
        NormalizeHoistDataSource(old.motorModelSource) !=
            NormalizeHoistDataSource(next.motorModelSource) ||
        NormalizeHoistDataSource(old.capacitySource) !=
            NormalizeHoistDataSource(next.capacitySource) ||
        NormalizeHoistDataSource(old.weightSource) !=
            NormalizeHoistDataSource(next.weightSource) ||
        NormalizeHoistDataSource(old.hoistFunctionSource) !=
            NormalizeHoistDataSource(next.hoistFunctionSource);
    const bool weightChanged = !Units::NearlyEqualWeightKilograms(
        old.weightKg, next.weightKg, 0.001);
    if (!supportChanged)
      continue;

    pushUndoIfNeeded();
    result.anyChanged = true;
    if (weightChanged) {
      result.changedWeightPositions.insert(
          NormalizePositionName(old.positionName));
      result.changedWeightPositions.insert(
          NormalizePositionName(next.positionName));
    }
    const Matrix requestedWorldTransform = next.transform;
    next.transform = old.transform;
    it->second = next;
    if (transformChanged)
      scene_node_operations::ApplyExactWorldTransform(
          scene, MvrNodeType::Support, it->second.uuid,
          requestedWorldTransform);
    if (!it->second.position.empty())
      scene.positions[it->second.position] = it->second.positionName;
  }
  return result;
}

} // namespace HoistTableEditService
