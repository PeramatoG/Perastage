#include "truss_table_edit_service.h"

#include "../resource_reference_sync.h"
#include "matrixutils.h"
#include "scene_node_operations.h"
#include "table_column_indices.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <wx/dataview.h>

namespace TrussTableEditService {
namespace {

using TrussColumn = TrussTableColumns::Column;

// Converts a truss column to its stable model index.
constexpr int ColumnIndex(TrussColumn column) {
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

// Builds the current identity key for shared truss type dimensions.
std::string MakeTypeKey(const Truss &truss) {
  return truss.name + "\x1f" + truss.manufacturer + "\x1f" + truss.model;
}

struct Dimensions {
  float length;
  float width;
  float height;
  float weight;
};

// Records a changed truss only once for presentation-layer logging.
void TrackUpdatedTruss(const Truss &truss,
                       std::unordered_set<std::string> &changedTrussIds,
                       Result &result) {
  if (changedTrussIds.insert(truss.uuid).second)
    result.updatedTrusses.emplace_back(truss.name, truss.uuid);
}

} // namespace

// Applies committed truss table rows to the scene and synchronizes equal types.
Result UpdateSceneData(ISceneAdapter &adapter, wxDataViewListCtrl *table,
                       const std::vector<std::string> &rowUuids,
                       const std::vector<wxString> &modelPaths,
                       const std::vector<wxString> &symbolPaths) {
  Result result;
  if (!table)
    return result;

  auto &scene = adapter.GetScene();
  const auto distanceUnit = adapter.GetDistanceUnitSystem();
  const auto weightUnit = adapter.GetWeightUnitSystem();
  const size_t count =
      std::min(static_cast<size_t>(table->GetItemCount()), rowUuids.size());
  std::unordered_map<std::string, Dimensions> dimensionsByType;
  std::unordered_set<std::string> changedTrussIds;
  bool undoPushed = false;
  const auto pushUndoIfNeeded = [&]() {
    if (!undoPushed) {
      adapter.PushUndoState("edit truss");
      undoPushed = true;
    }
  };

  for (size_t row = 0; row < count; ++row) {
    auto it = scene.trusses.find(rowUuids[row]);
    if (it == scene.trusses.end())
      continue;

    const Truss old = it->second;
    Truss next = old;
    wxVariant value;

    table->GetValue(value, row, ColumnIndex(TrussColumn::Name));
    next.name = std::string(value.GetString().mb_str());
    table->GetValue(value, row, ColumnIndex(TrussColumn::Layer));
    next.layer = std::string(value.GetString().mb_str());

    if (row < symbolPaths.size())
      next.symbolFile = gui::PreserveSceneResourceReferenceForTableSync(
          scene.basePath, old.symbolFile,
          std::string(symbolPaths[row].ToUTF8()));
    else if (row < modelPaths.size())
      next.symbolFile = gui::PreserveSceneResourceReferenceForTableSync(
          scene.basePath, old.symbolFile, std::string(modelPaths[row].ToUTF8()));
    else {
      table->GetValue(value, row, ColumnIndex(TrussColumn::ModelFile));
      next.symbolFile = std::string(value.GetString().ToUTF8());
    }

    if (row < modelPaths.size())
      next.modelFile = gui::PreserveSceneResourceReferenceForTableSync(
          scene.basePath, old.modelFile, std::string(modelPaths[row].ToUTF8()),
          old.symbolFile);
    else {
      table->GetValue(value, row, ColumnIndex(TrussColumn::ModelFile));
      next.modelFile = std::string(value.GetString().ToUTF8());
    }

    table->GetValue(value, row, ColumnIndex(TrussColumn::HangPosition));
    next.positionName = std::string(value.GetString().mb_str());

    double xMm = old.transform.o[0];
    double yMm = old.transform.o[1];
    double zMm = old.transform.o[2];
    table->GetValue(value, row, ColumnIndex(TrussColumn::PositionX));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(value.GetString().ToUTF8()), distanceUnit))
      xMm = *parsed;
    table->GetValue(value, row, ColumnIndex(TrussColumn::PositionY));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(value.GetString().ToUTF8()), distanceUnit))
      yMm = *parsed;
    table->GetValue(value, row, ColumnIndex(TrussColumn::PositionZ));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(value.GetString().ToUTF8()), distanceUnit))
      zMm = *parsed;

    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    const auto readRotation = [&](TrussColumn column, double &rotation) {
      table->GetValue(value, row, ColumnIndex(column));
      wxString text = value.GetString();
      text.Replace(DegreeSymbol(), "");
      text.ToDouble(&rotation);
    };
    readRotation(TrussColumn::Roll, roll);
    readRotation(TrussColumn::Pitch, pitch);
    readRotation(TrussColumn::Yaw, yaw);

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

    table->GetValue(value, row, ColumnIndex(TrussColumn::Manufacturer));
    next.manufacturer = std::string(value.GetString().mb_str());
    table->GetValue(value, row, ColumnIndex(TrussColumn::Model));
    next.model = std::string(value.GetString().mb_str());

    const auto readDistance = [&](TrussColumn column, float &target) {
      table->GetValue(value, row, ColumnIndex(column));
      if (const auto parsed = Units::ParseDistanceToMillimeters(
              std::string(value.GetString().ToUTF8()), distanceUnit))
        target = static_cast<float>(*parsed);
    };
    readDistance(TrussColumn::Length, next.lengthMm);
    readDistance(TrussColumn::Width, next.widthMm);
    readDistance(TrussColumn::Height, next.heightMm);
    table->GetValue(value, row, ColumnIndex(TrussColumn::Weight));
    if (const auto parsed = Units::ParseWeightToKilograms(
            std::string(value.GetString().ToUTF8()), weightUnit))
      next.weightKg = static_cast<float>(*parsed);

    table->GetValue(value, row, ColumnIndex(TrussColumn::Load));
    const std::string loadText = std::string(value.GetString().ToUTF8());
    if (loadText.empty()) {
      next.manualLoadKg = 0.0f;
      next.hasManualLoadOverride = false;
    } else if (const auto parsed =
                   Units::ParseWeightToKilograms(loadText, weightUnit)) {
      next.manualLoadKg = static_cast<float>(*parsed);
      next.hasManualLoadOverride = true;
    }

    const bool weightChanged = !Units::NearlyEqualWeightKilograms(
        old.weightKg, next.weightKg, 0.001);
    const bool hangPositionChanged = old.positionName != next.positionName;
    const bool trussChanged =
        old.name != next.name || old.layer != next.layer ||
        old.modelFile != next.modelFile || old.symbolFile != next.symbolFile ||
        hangPositionChanged || transformChanged ||
        old.manufacturer != next.manufacturer || old.model != next.model ||
        !Units::NearlyEqualDistanceMillimeters(old.lengthMm, next.lengthMm,
                                               0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.widthMm, next.widthMm,
                                               0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.heightMm, next.heightMm,
                                               0.5) ||
        weightChanged ||
        old.hasManualLoadOverride != next.hasManualLoadOverride ||
        !Units::NearlyEqualWeightKilograms(old.manualLoadKg, next.manualLoadKg,
                                           0.001);

    if (trussChanged) {
      pushUndoIfNeeded();
      result.anyChanged = true;
      if (weightChanged || hangPositionChanged) {
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
            scene, MvrNodeType::Truss, it->second.uuid,
            requestedWorldTransform);
      if (!it->second.position.empty())
        scene.positions[it->second.position] = it->second.positionName;
      TrackUpdatedTruss(it->second, changedTrussIds, result);
    }

    const Truss &canonicalSource = trussChanged ? it->second : old;
    const std::string key = MakeTypeKey(canonicalSource);
    if (trussChanged || !dimensionsByType.contains(key)) {
      dimensionsByType[key] = {canonicalSource.lengthMm,
                               canonicalSource.widthMm,
                               canonicalSource.heightMm,
                               canonicalSource.weightKg};
    }
  }

  for (size_t row = 0; row < count; ++row) {
    auto it = scene.trusses.find(rowUuids[row]);
    if (it == scene.trusses.end())
      continue;
    const auto dimensions = dimensionsByType.find(MakeTypeKey(it->second));
    if (dimensions == dimensionsByType.end())
      continue;

    const auto &shared = dimensions->second;
    const bool synchronizedWeightChanged = !Units::NearlyEqualWeightKilograms(
        it->second.weightKg, shared.weight, 0.001);
    if (it->second.lengthMm == shared.length &&
        it->second.widthMm == shared.width &&
        it->second.heightMm == shared.height && !synchronizedWeightChanged)
      continue;

    pushUndoIfNeeded();
    result.anyChanged = true;
    it->second.lengthMm = shared.length;
    it->second.widthMm = shared.width;
    it->second.heightMm = shared.height;
    it->second.weightKg = shared.weight;
    if (synchronizedWeightChanged)
      result.changedWeightPositions.insert(
          NormalizePositionName(it->second.positionName));

    const wxString length = wxString::Format("%.2f", shared.length / 1000.0f);
    const wxString width = shared.width > 0.0f
                               ? wxString::Format("%.2f", shared.width / 1000.0f)
                               : wxString();
    const wxString height = shared.height > 0.0f
                                ? wxString::Format("%.2f", shared.height / 1000.0f)
                                : wxString();
    const wxString weight = wxString::Format("%.2f", shared.weight);
    table->SetValue(wxVariant(length), row, ColumnIndex(TrussColumn::Length));
    table->SetValue(wxVariant(width), row, ColumnIndex(TrussColumn::Width));
    table->SetValue(wxVariant(height), row, ColumnIndex(TrussColumn::Height));
    table->SetValue(wxVariant(weight), row, ColumnIndex(TrussColumn::Weight));
    TrackUpdatedTruss(it->second, changedTrussIds, result);
  }

  return result;
}

} // namespace TrussTableEditService
