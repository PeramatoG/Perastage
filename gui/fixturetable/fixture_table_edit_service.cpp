#include "layer_identity.h"
#include "fixture_table_edit_service.h"
#include "fixture_patch_service.h"
#include "fixture_network_reference.h"
#include "filesystem_path_utils.h"

#include "fixture_table_columns.h"

#include "../dataview_edit_commit.h"
#include "../resource_reference_sync.h"
#include "consolepanel.h"
#include "gdtf_fixture_category.h"
#include "gdtf_mutation_audit.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "matrixutils.h"
#include "scene_grouping.h"
#include "scene_node_operations.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_map>

namespace FixtureTableEditService {

namespace {
// Returns the localized degree symbol used by angle cells.
const wxString &DegreeSymbol() {
  static const wxString kDegreeSymbol = wxString::FromUTF8("\xC2\xB0");
  return kDegreeSymbol;
}

// Presentation-only adapter; Core owns assignment selection and mutation.
bool ApplyPrimaryPatch(Fixture &fixture, const std::string &requested) {
  const auto result = perastage::patch::SetFixturePrimaryPatch(fixture, requested);
  if (!result && ConsolePanel::Instance())
    ConsolePanel::Instance()->AppendMessage(wxString::FromUTF8(result.detail));
  return static_cast<bool>(result);
}

// Rejected edits must not leave displayed patch cells ahead of scene state.
void RestorePatchCells(wxDataViewListCtrl *table, size_t row, const Fixture &fixture) {
  const auto patch = perastage::patch::ParseFixturePatchAddress(fixture.address);
  table->SetValue(wxVariant(patch ? static_cast<long>(patch->universe) : 0L), row,
      FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Universe));
  table->SetValue(wxVariant(patch ? static_cast<long>(patch->channel) : 0L), row,
      FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Channel));
}

constexpr const char *kUnassignedPosition = "Unassigned";

// Checks whether two fixtures share the same category-bearing type profile.
bool IsSameFixtureCategoryType(const Fixture &fixture,
                               const Fixture &reference) {
  if (!reference.gdtfSpec.empty()) {
    if (fixture.gdtfSpec != reference.gdtfSpec)
      return false;
    return reference.gdtfMode.empty() || fixture.gdtfMode == reference.gdtfMode;
  }
  if (!reference.typeName.empty() && fixture.typeName != reference.typeName)
    return false;
  return !reference.typeName.empty();
}

// Normalizes empty position names for grouped physical summaries.
std::string NormalizePositionName(const std::string &positionName) {
  return positionName.empty() ? kUnassignedPosition : positionName;
}

struct SceneUpdateTracking {
  size_t updatedCount = 0;
  wxString firstName;
  wxString firstUuid;
  bool anyChanged = false;
  bool undoPushed = false;
};

void AppendChangeLogIfNeeded(SceneUpdateTracking &tracking, bool logChanges) {
  if (!tracking.anyChanged || !logChanges)
    return;

  ConsolePanel *console = ConsolePanel::Instance();
  if (!console)
    return;

  wxString msg;
  if (tracking.updatedCount == 1)
    msg = "Updated fixture " + tracking.firstName + " (UUID " +
          tracking.firstUuid + ")";
  else if (tracking.updatedCount > 1)
    msg = wxString::Format("Updated %zu fixtures", tracking.updatedCount);
  if (!msg.empty())
    console->AppendMessage(msg);
}

void TrackUpdatedFixture(const Fixture &fixture, SceneUpdateTracking &tracking,
                         bool logChanges) {
  tracking.anyChanged = true;
  if (!logChanges || !ConsolePanel::Instance())
    return;

  ++tracking.updatedCount;
  if (tracking.updatedCount == 1) {
    tracking.firstName = wxString::FromUTF8(fixture.instanceName.c_str());
    tracking.firstUuid = wxString::FromUTF8(fixture.uuid.c_str());
  }
}

void PushUndoIfNeeded(FixtureTableEditService::ISceneAdapter &adapter,
                      SceneUpdateTracking &tracking) {
  if (tracking.undoPushed)
    return;
  adapter.PushUndoState("edit fixture");
  tracking.undoPushed = true;
}

std::vector<size_t>
ResolveTargetRows(wxDataViewListCtrl *table,
                  const std::vector<std::string> &rowUuids) {
  std::vector<size_t> rows;
  if (!table)
    return rows;

  const size_t count =
      std::min(static_cast<size_t>(table->GetItemCount()), rowUuids.size());
  rows.reserve(count);

  wxDataViewItemArray selections;
  table->GetSelections(selections);
  if (!selections.empty()) {
    for (const auto &selection : selections) {
      const int row = table->ItemToRow(selection);
      if (row >= 0 && static_cast<size_t>(row) < count)
        rows.push_back(static_cast<size_t>(row));
    }
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    return rows;
  }

  for (size_t i = 0; i < count; ++i)
    rows.push_back(i);
  return rows;
}

std::vector<size_t>
ResolveTargetRows(wxDataViewListCtrl *table,
                  const std::vector<std::string> &rowUuids,
                  const std::vector<unsigned int> *explicitTargetRows) {
  if (!explicitTargetRows)
    return ResolveTargetRows(table, rowUuids);

  std::vector<size_t> rows;
  if (!table)
    return rows;

  const size_t count =
      std::min(static_cast<size_t>(table->GetItemCount()), rowUuids.size());
  rows.reserve(explicitTargetRows->size());
  for (const unsigned int row : *explicitTargetRows) {
    if (static_cast<size_t>(row) < count)
      rows.push_back(static_cast<size_t>(row));
  }
  std::sort(rows.begin(), rows.end());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
  return rows;
}

// Applies full fixture table rows back into the scene while preserving stable
// resource references.
void ApplyFullRowChanges(
    FixtureTableEditService::ISceneAdapter &adapter, wxDataViewListCtrl *table,
    const std::vector<std::string> &rowUuids,
    const std::vector<wxString> &gdtfPaths,
    const std::unordered_set<std::string> *manualCategoryUuids,
    std::unordered_set<std::string> *changedWeightPositions, bool logChanges) {
  auto &scene = adapter.GetScene();
  const auto distanceUnitSystem = adapter.GetDistanceUnitSystem();
  const auto weightUnitSystem = adapter.GetWeightUnitSystem();

  SceneUpdateTracking tracking;

  size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());
  // Keep scene writes and undo states limited to rows with real changes.
  for (size_t i = 0; i < count; ++i) {
    auto it = scene.fixtures.find(rowUuids[i]);
    if (it == scene.fixtures.end())
      continue;

    const Fixture old = it->second;
    Fixture next = old;
    const bool forceManualCategory =
        manualCategoryUuids &&
        manualCategoryUuids->find(rowUuids[i]) != manualCategoryUuids->end();

    if (i < gdtfPaths.size())
      next.gdtfSpec = gui::PreserveSceneResourceReferenceForTableSync(
          scene.basePath, old.gdtfSpec, std::string(gdtfPaths[i].ToUTF8()));

    if (next.gdtfSpec != old.gdtfSpec) {
      auto path = PathUtils::PathFromUtf8(next.gdtfSpec);
      if (!path.is_absolute()) path = PathUtils::PathFromUtf8(scene.basePath) / path;
      const auto check = fixture_network::InspectReferences(next, path);
      if (!check) {
        if (ConsolePanel::Instance()) ConsolePanel::Instance()->AppendMessage(wxString::FromUTF8(check.detail));
        continue;
      }
    }
    wxVariant v;
    table->GetValue(
        v, i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Name));
    next.instanceName = std::string(v.GetString().ToUTF8());

    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::FixtureId));
    next.fixtureId = static_cast<int>(v.GetLong());

    table->GetValue(
        v, i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Layer));
    layerdomain::PrepareLayerEdit(scene, next, std::string(v.GetString().ToUTF8()));

    table->GetValue(v, i,
                    FixtureTableColumns::ToIndex(
                        FixtureTableColumns::Column::HangPosition));
    next.positionName = std::string(v.GetString().ToUTF8());

    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Universe));
    long uni = v.GetLong();
    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Channel));
    long ch = v.GetLong();

    table->GetValue(
        v, i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));
    next.typeName = std::string(v.GetString().ToUTF8());

    table->GetValue(
        v, i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Mode));
    next.gdtfMode = std::string(v.GetString().ToUTF8());

    const auto requestedPatch = uni > 0 && ch > 0
        ? wxString::Format("%ld.%ld", uni, ch).ToStdString() : std::string{};
    if (requestedPatch != old.address && !ApplyPrimaryPatch(next, requestedPatch)) {
      RestorePatchCells(table, i, old);
      continue;
    }

    double xMm = old.transform.o[0];
    double yMm = old.transform.o[1];
    double zMm = old.transform.o[2];
    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::PositionX));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnitSystem);
        parsed.has_value())
      xMm = *parsed;
    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::PositionY));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnitSystem);
        parsed.has_value())
      yMm = *parsed;
    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::PositionZ));
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnitSystem);
        parsed.has_value())
      zMm = *parsed;

    double roll = 0, pitch = 0, yaw = 0;
    table->GetValue(
        v, i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Roll));
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
        s.Replace(DegreeSymbol(), "");
      s.ToDouble(&roll);
    }
    table->GetValue(
        v, i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Pitch));
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
        s.Replace(DegreeSymbol(), "");
      s.ToDouble(&pitch);
    }
    table->GetValue(
        v, i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Yaw));
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
        s.Replace(DegreeSymbol(), "");
      s.ToDouble(&yaw);
    }

    const auto currentEuler = MatrixUtils::MatrixToEuler(old.transform);
    const bool transformChanged =
        !Units::NearlyEqualDistanceMillimeters(old.transform.o[0], xMm, 0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.transform.o[1], yMm, 0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.transform.o[2], zMm, 0.5) ||
        std::abs(static_cast<double>(currentEuler[2]) - roll) > 0.05 ||
        std::abs(static_cast<double>(currentEuler[1]) - pitch) > 0.05 ||
        std::abs(static_cast<double>(currentEuler[0]) - yaw) > 0.05;

    if (transformChanged) {
      Matrix rot = MatrixUtils::EulerToMatrix(static_cast<float>(yaw),
                                              static_cast<float>(pitch),
                                              static_cast<float>(roll));
      next.transform = MatrixUtils::ApplyRotationPreservingScale(
          old.transform, rot,
          {static_cast<float>(xMm), static_cast<float>(yMm),
           static_cast<float>(zMm)});
    }

    table->GetValue(
        v, i, FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Power));
    double pw = 0.0;
    const bool validPower = v.GetString().ToDouble(&pw) && std::isfinite(pw) && pw >= 0;
    if (validPower)
      next.powerConsumptionW = static_cast<float>(pw);
    const bool powerChanged = old.powerConsumptionW != next.powerConsumptionW ||
                              (!old.HasPlanningPower() && validPower);

    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Weight));
    const float previousWeightKg = next.weightKg;
    if (const auto parsedWeightKg = Units::ParseWeightToKilograms(
            std::string(v.GetString().ToUTF8()), weightUnitSystem);
        parsedWeightKg.has_value()) {
      next.weightKg = static_cast<float>(*parsedWeightKg);
    }
    const bool weightChanged = !Units::NearlyEqualWeightKilograms(
        previousWeightKg, next.weightKg, 0.001);
    if (powerChanged) next.SetPlanningPowerOverride(next.powerConsumptionW);
    if (weightChanged) {
      next.physicalPropertiesSource = FixturePhysicalPropertiesSource::Manual;
      next.physicalPropertiesDirty = true;
    }

    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Category));
    next.category = GdtfFixtureCategory::NormalizeCategory(
        std::string(v.GetString().ToUTF8()));
    if (!next.category.empty() &&
        (forceManualCategory || next.category != old.category)) {
      next.categorySource = GdtfFixtureCategory::kManualSource;
      next.categorySourceReason.clear();
    }

    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::VisualColor));
    if (v.GetType() == "wxDataViewIconText") {
      wxDataViewIconText icon;
      icon << v;
      wxString txt = icon.GetText();
      if (!txt.IsEmpty())
        next.visualColorHex = std::string(txt.ToUTF8());
      else
        next.visualColorHex.clear();
    } else {
      next.visualColorHex = std::string(v.GetString().ToUTF8());
    }
    table->GetValue(
        v, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::MvrColor));
    if (v.GetType() == "wxDataViewIconText") {
      wxDataViewIconText icon;
      icon << v;
      next.mvrFixtureColorHex = std::string(icon.GetText().ToUTF8());
    } else {
      next.mvrFixtureColorHex = std::string(v.GetString().ToUTF8());
    }

    const bool fixtureChanged =
        old.gdtfSpec != next.gdtfSpec ||
        old.instanceName != next.instanceName ||
        old.fixtureId != next.fixtureId || old.layer != next.layer || old.layerUuid != next.layerUuid ||
        old.positionName != next.positionName || old.address != next.address ||
        old.typeName != next.typeName || old.gdtfMode != next.gdtfMode ||
        transformChanged || old.powerConsumptionW != next.powerConsumptionW ||
        !Units::NearlyEqualWeightKilograms(old.weightKg, next.weightKg,
                                           0.001) ||
        old.category != next.category ||
        old.categorySource != next.categorySource ||
        old.categorySourceReason != next.categorySourceReason ||
        old.visualColorHex != next.visualColorHex ||
        old.mvrFixtureColorHex != next.mvrFixtureColorHex;
    if (!fixtureChanged)
      continue;

    PushUndoIfNeeded(adapter, tracking);
    if (weightChanged && changedWeightPositions) {
      changedWeightPositions->insert(NormalizePositionName(old.positionName));
      changedWeightPositions->insert(NormalizePositionName(next.positionName));
    }
    const Matrix requestedWorldTransform = next.transform;
    next.transform = old.transform;
    layerdomain::CommitLayerEdit(scene, next);
    it->second = next;
    if (transformChanged) {
      scene_node_operations::ApplyExactWorldTransform(
          scene, MvrNodeType::Fixture, it->second.uuid,
          requestedWorldTransform);
    }
    if (!next.typeName.empty() && !next.category.empty() &&
        next.categorySource == GdtfFixtureCategory::kManualSource) {
      GdtfDictionary::UpdateCategoryForFile(next.typeName, next.gdtfSpec,
                                            next.category);
    }
    if (!it->second.position.empty())
      scene.positions[it->second.position] = it->second.positionName;

    TrackUpdatedFixture(it->second, tracking, logChanges);
  }

  AppendChangeLogIfNeeded(tracking, logChanges);
}

void ApplyPatchChanges(FixtureTableEditService::ISceneAdapter &adapter,
                       wxDataViewListCtrl *table,
                       const std::vector<std::string> &rowUuids,
                       bool logChanges) {
  auto &scene = adapter.GetScene();
  SceneUpdateTracking tracking;
  const auto targetRows = ResolveTargetRows(table, rowUuids);

  for (size_t row : targetRows) {
    auto it = scene.fixtures.find(rowUuids[row]);
    if (it == scene.fixtures.end())
      continue;

    const Fixture old = it->second;
    Fixture next = old;

    wxVariant value;
    table->GetValue(
        value, row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Universe));
    const long universe = value.GetLong();
    table->GetValue(
        value, row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Channel));
    const long channel = value.GetLong();

    if (universe > 0 && channel > 0)
      next.address =
          wxString::Format("%ld.%ld", universe, channel).ToStdString();
    else
      next.address.clear();

    if (old.address == next.address)
      continue;

    if (!ApplyPrimaryPatch(next, next.address)) {
      RestorePatchCells(table, row, old);
      continue;
    }
    PushUndoIfNeeded(adapter, tracking);
    it->second = std::move(next);
    TrackUpdatedFixture(it->second, tracking, logChanges);
  }

  AppendChangeLogIfNeeded(tracking, logChanges);
}

std::string ExtractColorValue(const wxVariant &value) {
  if (value.GetType() == "wxDataViewIconText") {
    wxDataViewIconText icon;
    icon << value;
    const wxString text = icon.GetText();
    return text.IsEmpty() ? std::string() : std::string(text.ToUTF8());
  }
  return std::string(value.GetString().ToUTF8());
}

void ApplyAppearanceChanges(FixtureTableEditService::ISceneAdapter &adapter,
                            wxDataViewListCtrl *table,
                            const std::vector<std::string> &rowUuids,
                            bool logChanges) {
  auto &scene = adapter.GetScene();
  SceneUpdateTracking tracking;
  const size_t rowCount =
      std::min(static_cast<size_t>(table->GetItemCount()), rowUuids.size());
  std::unordered_set<std::string> updatedDictionaryKeys;

  for (size_t row = 0; row < rowCount; ++row) {
    auto it = scene.fixtures.find(rowUuids[row]);
    if (it == scene.fixtures.end())
      continue;

    wxVariant value;
    table->GetValue(
        value, row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::VisualColor));
    const std::string nextColor = ExtractColorValue(value);
    if (it->second.visualColorHex == nextColor)
      continue;

    PushUndoIfNeeded(adapter, tracking);
    it->second.visualColorHex = nextColor;
    TrackUpdatedFixture(it->second, tracking, logChanges);

    const std::string dictionaryKey =
        it->second.typeName + '\x1f' + it->second.gdtfSpec + '\x1f' +
        it->second.gdtfMode;
    if (updatedDictionaryKeys.insert(dictionaryKey).second) {
      GdtfDictionary::UpdateVisualColorForFile(
          it->second.typeName, it->second.gdtfSpec, it->second.gdtfMode,
          nextColor);
    }
  }

  AppendChangeLogIfNeeded(tracking, logChanges);
}

// Applies category edits and synchronizes the type-level category across
// matching fixtures.
void ApplyCategoryChanges(
    FixtureTableEditService::ISceneAdapter &adapter, wxDataViewListCtrl *table,
    const std::vector<std::string> &rowUuids,
    const std::unordered_set<std::string> *manualCategoryUuids, bool logChanges,
    const std::vector<unsigned int> *explicitTargetRows = nullptr) {
  auto &scene = adapter.GetScene();
  SceneUpdateTracking tracking;
  auto targetRows = ResolveTargetRows(table, rowUuids, explicitTargetRows);
  if (table && manualCategoryUuids) {
    const size_t count =
        std::min(static_cast<size_t>(table->GetItemCount()), rowUuids.size());
    for (size_t row = 0; row < count; ++row) {
      if (manualCategoryUuids->find(rowUuids[row]) !=
          manualCategoryUuids->end())
        targetRows.push_back(row);
    }
    std::sort(targetRows.begin(), targetRows.end());
    targetRows.erase(std::unique(targetRows.begin(), targetRows.end()),
                     targetRows.end());
  }
  std::unordered_map<std::string, std::string> manualCategoriesByType;

  for (size_t row : targetRows) {
    auto it = scene.fixtures.find(rowUuids[row]);
    if (it == scene.fixtures.end())
      continue;

    const Fixture old = it->second;
    Fixture next = old;

    const bool forceManualCategory =
        manualCategoryUuids &&
        manualCategoryUuids->find(rowUuids[row]) != manualCategoryUuids->end();

    wxVariant value;
    table->GetValue(
        value, row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Category));
    next.category = GdtfFixtureCategory::NormalizeCategory(
        std::string(value.GetString().ToUTF8()));
    if (!next.category.empty() &&
        (forceManualCategory || next.category != old.category)) {
      next.categorySource = GdtfFixtureCategory::kManualSource;
      next.categorySourceReason.clear();
    }

    if (old.category == next.category &&
        old.categorySource == next.categorySource &&
        old.categorySourceReason == next.categorySourceReason)
      continue;

    PushUndoIfNeeded(adapter, tracking);
    it->second.category = next.category;
    it->second.categorySource = next.categorySource;
    it->second.categorySourceReason = next.categorySourceReason;
    if (!next.typeName.empty() && !next.category.empty() &&
        next.categorySource == GdtfFixtureCategory::kManualSource) {
      manualCategoriesByType[next.typeName] = next.category;
    }
    TrackUpdatedFixture(it->second, tracking, logChanges);

    for (auto &[uuid, fixture] : scene.fixtures) {
      if (uuid == it->first || !IsSameFixtureCategoryType(fixture, next))
        continue;
      if (fixture.category == next.category &&
          fixture.categorySource == next.categorySource &&
          fixture.categorySourceReason == next.categorySourceReason)
        continue;
      fixture.category = next.category;
      fixture.categorySource = next.categorySource;
      fixture.categorySourceReason = next.categorySourceReason;
      TrackUpdatedFixture(fixture, tracking, logChanges);
    }
  }

  GdtfDictionary::UpdateCategoriesBulk(manualCategoriesByType);
  AppendChangeLogIfNeeded(tracking, logChanges);
}
} // namespace

std::vector<int> BuildOrderedRows(const std::vector<int> &selectedRows,
                                  const std::vector<int> &selectionOrder) {
  std::vector<int> orderedRows;
  for (int idx : selectionOrder)
    if (std::find(selectedRows.begin(), selectedRows.end(), idx) !=
        selectedRows.end())
      orderedRows.push_back(idx);
  for (int idx : selectedRows)
    if (std::find(orderedRows.begin(), orderedRows.end(), idx) ==
        orderedRows.end())
      orderedRows.push_back(idx);
  return orderedRows;
}

void PropagateTypeValues(wxDataViewListCtrl *table,
                         const wxDataViewItemArray &selections, int col) {
  const auto column = FixtureTableColumns::FromIndex(col);
  if (!column || !FixtureTableColumns::IsTypeLevelPropagated(*column))
    return;

  std::unordered_map<std::string, wxVariant> typeValues;
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r == wxNOT_FOUND)
      continue;
    wxVariant vType, vMode, vVal;
    table->GetValue(
        vType, r,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));
    table->GetValue(
        vMode, r,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Mode));
    table->GetValue(vVal, r, col);
    std::string key = std::string(vType.GetString().ToUTF8());
    if (FixtureTableColumns::IsVisualColor(*column)) {
      key.push_back('\x1f');
      key += std::string(vMode.GetString().ToUTF8());
    }
    typeValues[key] = vVal;
  }

  unsigned int rowCount = table->GetItemCount();
  for (unsigned int i = 0; i < rowCount; ++i) {
    wxVariant vType, vMode;
    table->GetValue(
        vType, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Type));
    table->GetValue(
        vMode, i,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Mode));
    std::string key = std::string(vType.GetString().ToUTF8());
    if (FixtureTableColumns::IsVisualColor(*column)) {
      key.push_back('\x1f');
      key += std::string(vMode.GetString().ToUTF8());
    }
    auto it = typeValues.find(key);
    if (it == typeValues.end())
      continue;

    wxVariant currentValue;
    table->GetValue(currentValue, i, col);
    if (FixtureTableColumns::IsVisualColor(*column)) {
      if (ExtractColorValue(currentValue) == ExtractColorValue(it->second))
        continue;
    } else if (currentValue.GetString() == it->second.GetString()) {
      continue;
    }

    table->SetValue(it->second, i, col);
  }
}

void UpdateSceneData(ISceneAdapter &adapter, wxDataViewListCtrl *table,
                     const std::vector<std::string> &rowUuids,
                     const std::vector<wxString> &gdtfPaths,
                     const std::unordered_set<std::string> *manualCategoryUuids,
                     std::unordered_set<std::string> *changedWeightPositions,
                     bool logChanges) {
  UpdateFullRowData(adapter, table, rowUuids, gdtfPaths, manualCategoryUuids,
                    changedWeightPositions, logChanges);
}

void UpdatePatchForRows(ISceneAdapter &adapter, wxDataViewListCtrl *table,
                        const std::vector<std::string> &rowUuids,
                        bool logChanges) {
  // Ensure in-place cell editors commit pending values before reading table
  // rows.
  if (table)
    DataViewEditCommit::CommitPendingEdit(table);
  ApplyPatchChanges(adapter, table, rowUuids, logChanges);
}

void UpdateAppearanceForRows(ISceneAdapter &adapter, wxDataViewListCtrl *table,
                             const std::vector<std::string> &rowUuids,
                             bool logChanges) {
  if (table)
    DataViewEditCommit::CommitPendingEdit(table);
  ApplyAppearanceChanges(adapter, table, rowUuids, logChanges);
}

void UpdateCategoryForRows(
    ISceneAdapter &adapter, wxDataViewListCtrl *table,
    const std::vector<std::string> &rowUuids,
    const std::unordered_set<std::string> *manualCategoryUuids,
    bool logChanges) {
  if (table)
    DataViewEditCommit::CommitPendingEdit(table);
  ApplyCategoryChanges(adapter, table, rowUuids, manualCategoryUuids,
                       logChanges);
}

void UpdateCategoryForRows(
    ISceneAdapter &adapter, wxDataViewListCtrl *table,
    const std::vector<std::string> &rowUuids,
    const std::vector<unsigned int> &targetRows,
    const std::unordered_set<std::string> *manualCategoryUuids,
    bool logChanges) {
  if (table)
    DataViewEditCommit::CommitPendingEdit(table);
  ApplyCategoryChanges(adapter, table, rowUuids, manualCategoryUuids,
                       logChanges, &targetRows);
}

void UpdateFullRowData(
    ISceneAdapter &adapter, wxDataViewListCtrl *table,
    const std::vector<std::string> &rowUuids,
    const std::vector<wxString> &gdtfPaths,
    const std::unordered_set<std::string> *manualCategoryUuids,
    std::unordered_set<std::string> *changedWeightPositions, bool logChanges) {
  if (table)
    DataViewEditCommit::CommitPendingEdit(table);
  ApplyFullRowChanges(adapter, table, rowUuids, gdtfPaths, manualCategoryUuids,
                      changedWeightPositions, logChanges);
}

void ApplyNameChanges(ISceneAdapter &adapter, wxDataViewListCtrl *table,
                      const std::vector<std::string> &rowUuids,
                      const std::vector<int> &selectedRows, bool logChanges) {
  if (table)
    DataViewEditCommit::CommitPendingEdit(table);

  auto &scene = adapter.GetScene();
  SceneUpdateTracking tracking;
  for (int row : selectedRows) {
    if (row < 0 || static_cast<size_t>(row) >= rowUuids.size() ||
        static_cast<size_t>(row) >= table->GetItemCount())
      continue;

    auto it = scene.fixtures.find(rowUuids[static_cast<size_t>(row)]);
    if (it == scene.fixtures.end())
      continue;

    const Fixture old = it->second;
    wxVariant v;
    table->GetValue(
        v, row,
        FixtureTableColumns::ToIndex(FixtureTableColumns::Column::Name));
    const std::string nextInstanceName = std::string(v.GetString().ToUTF8());
    if (old.instanceName == nextInstanceName)
      continue;

    PushUndoIfNeeded(adapter, tracking);
    it->second.instanceName = nextInstanceName;
    TrackUpdatedFixture(it->second, tracking, logChanges);
  }

  AppendChangeLogIfNeeded(tracking, logChanges);
}

} // namespace FixtureTableEditService
