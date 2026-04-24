#include "fixture_table_edit_service.h"

#include "consolepanel.h"
#include "../dataview_edit_commit.h"
#include "matrixutils.h"
#include "gdtfdictionary.h"
#include "gdtf_fixture_category.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace FixtureTableEditService {

namespace {
const wxString &DegreeSymbol() {
  static const wxString kDegreeSymbol = wxString::FromUTF8("\xC2\xB0");
  return kDegreeSymbol;
}

constexpr const char *kUnassignedPosition = "Unassigned";

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
    msg = "Updated fixture " + tracking.firstName + " (UUID " + tracking.firstUuid + ")";
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

std::vector<size_t> ResolveTargetRows(wxDataViewListCtrl *table,
                                      const std::vector<std::string> &rowUuids) {
  std::vector<size_t> rows;
  if (!table)
    return rows;

  const size_t count = std::min(static_cast<size_t>(table->GetItemCount()),
                                rowUuids.size());
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

void ApplyFullRowChanges(
    FixtureTableEditService::ISceneAdapter &adapter, wxDataViewListCtrl *table,
    const std::vector<std::string> &rowUuids, const std::vector<wxString> &gdtfPaths,
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
      next.gdtfSpec = std::string(gdtfPaths[i].ToUTF8());

    wxVariant v;
    table->GetValue(v, i, 1);
    next.instanceName = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, 0);
    next.fixtureId = static_cast<int>(v.GetLong());

    table->GetValue(v, i, 3);
    next.layer = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, 4);
    next.positionName = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, 5);
    long uni = v.GetLong();
    table->GetValue(v, i, 6);
    long ch = v.GetLong();

    table->GetValue(v, i, 2);
    next.typeName = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, 7);
    next.gdtfMode = std::string(v.GetString().ToUTF8());

    if (uni > 0 && ch > 0)
      next.address = wxString::Format("%ld.%ld", uni, ch).ToStdString();
    else
      next.address.clear();

    double xMm = old.transform.o[0];
    double yMm = old.transform.o[1];
    double zMm = old.transform.o[2];
    table->GetValue(v, i, 10);
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnitSystem);
        parsed.has_value())
      xMm = *parsed;
    table->GetValue(v, i, 11);
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnitSystem);
        parsed.has_value())
      yMm = *parsed;
    table->GetValue(v, i, 12);
    if (const auto parsed = Units::ParseDistanceToMillimeters(
            std::string(v.GetString().ToUTF8()), distanceUnitSystem);
        parsed.has_value())
      zMm = *parsed;

    double roll = 0, pitch = 0, yaw = 0;
    table->GetValue(v, i, 13);
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
        s.Replace(DegreeSymbol(), "");
      s.ToDouble(&roll);
    }
    table->GetValue(v, i, 14);
    {
      wxString s = v.GetString();
      if (!DegreeSymbol().empty())
        s.Replace(DegreeSymbol(), "");
      s.ToDouble(&pitch);
    }
    table->GetValue(v, i, 15);
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
      Matrix rot = MatrixUtils::EulerToMatrix(
          static_cast<float>(yaw), static_cast<float>(pitch),
          static_cast<float>(roll));
      next.transform = MatrixUtils::ApplyRotationPreservingScale(
          old.transform, rot,
          {static_cast<float>(xMm), static_cast<float>(yMm),
           static_cast<float>(zMm)});
    }

    table->GetValue(v, i, 16);
    double pw = 0.0;
    v.GetString().ToDouble(&pw);
    next.powerConsumptionW = static_cast<float>(pw);

    table->GetValue(v, i, 17);
    const float previousWeightKg = next.weightKg;
    if (const auto parsedWeightKg = Units::ParseWeightToKilograms(
            std::string(v.GetString().ToUTF8()), weightUnitSystem);
        parsedWeightKg.has_value()) {
      next.weightKg = static_cast<float>(*parsedWeightKg);
    }
    const bool weightChanged = !Units::NearlyEqualWeightKilograms(
        previousWeightKg, next.weightKg, 0.001);

    table->GetValue(v, i, 18);
    next.category = GdtfFixtureCategory::NormalizeCategory(std::string(v.GetString().ToUTF8()));
    if (!next.category.empty() &&
        (forceManualCategory || next.category != old.category)) {
      next.categorySource = GdtfFixtureCategory::kManualSource;
      next.categorySourceReason.clear();
    }

    table->GetValue(v, i, 19);
    if (v.GetType() == "wxDataViewIconText") {
      wxDataViewIconText icon;
      icon << v;
      wxString txt = icon.GetText();
      if (!txt.IsEmpty())
        next.color = std::string(txt.ToUTF8());
      else
        next.color.clear();
    } else {
      next.color = std::string(v.GetString().ToUTF8());
    }

    const bool fixtureChanged = old.gdtfSpec != next.gdtfSpec ||
                                old.instanceName != next.instanceName ||
                                old.fixtureId != next.fixtureId ||
                                old.layer != next.layer ||
                                old.positionName != next.positionName ||
                                old.address != next.address ||
                                old.typeName != next.typeName ||
                                old.gdtfMode != next.gdtfMode ||
                                transformChanged ||
                                old.powerConsumptionW != next.powerConsumptionW ||
                                !Units::NearlyEqualWeightKilograms(old.weightKg, next.weightKg,
                                                                   0.001) ||
                                old.category != next.category ||
                                old.categorySource != next.categorySource ||
                                old.categorySourceReason != next.categorySourceReason ||
                                old.color != next.color;
    if (!fixtureChanged)
      continue;

    PushUndoIfNeeded(adapter, tracking);
    if (weightChanged && changedWeightPositions) {
      changedWeightPositions->insert(NormalizePositionName(old.positionName));
      changedWeightPositions->insert(NormalizePositionName(next.positionName));
    }
    it->second = next;
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
    table->GetValue(value, row, 5);
    const long universe = value.GetLong();
    table->GetValue(value, row, 6);
    const long channel = value.GetLong();

    if (universe > 0 && channel > 0)
      next.address = wxString::Format("%ld.%ld", universe, channel).ToStdString();
    else
      next.address.clear();

    if (old.address == next.address)
      continue;

    PushUndoIfNeeded(adapter, tracking);
    it->second.address = next.address;
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
  const auto targetRows = ResolveTargetRows(table, rowUuids);

  for (size_t row : targetRows) {
    auto it = scene.fixtures.find(rowUuids[row]);
    if (it == scene.fixtures.end())
      continue;

    wxVariant value;
    table->GetValue(value, row, 19);
    const std::string nextColor = ExtractColorValue(value);
    if (it->second.color == nextColor)
      continue;

    PushUndoIfNeeded(adapter, tracking);
    it->second.color = nextColor;
    TrackUpdatedFixture(it->second, tracking, logChanges);
  }

  AppendChangeLogIfNeeded(tracking, logChanges);
}

void ApplyCategoryChanges(
    FixtureTableEditService::ISceneAdapter &adapter, wxDataViewListCtrl *table,
    const std::vector<std::string> &rowUuids,
    const std::unordered_set<std::string> *manualCategoryUuids, bool logChanges) {
  auto &scene = adapter.GetScene();
  SceneUpdateTracking tracking;
  const auto targetRows = ResolveTargetRows(table, rowUuids);
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
    table->GetValue(value, row, 18);
    next.category =
        GdtfFixtureCategory::NormalizeCategory(std::string(value.GetString().ToUTF8()));
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
  }

  GdtfDictionary::UpdateCategoriesBulk(manualCategoriesByType);
  AppendChangeLogIfNeeded(tracking, logChanges);
}
}

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
  if (col != 16 && col != 17 && col != 18 && col != 19)
    return;

  if (col == 19)
    return;

  std::unordered_map<std::string, wxString> typeValues;
  for (const auto &it : selections) {
    int r = table->ItemToRow(it);
    if (r == wxNOT_FOUND)
      continue;
    wxVariant vType, vVal;
    table->GetValue(vType, r, 2);
    table->GetValue(vVal, r, col);
    typeValues[std::string(vType.GetString().ToUTF8())] = vVal.GetString();
  }

  unsigned int rowCount = table->GetItemCount();
  for (unsigned int i = 0; i < rowCount; ++i) {
    wxVariant vType;
    table->GetValue(vType, i, 2);
    auto it = typeValues.find(std::string(vType.GetString().ToUTF8()));
    if (it != typeValues.end())
      table->SetValue(wxVariant(it->second), i, col);
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
  // Ensure in-place cell editors commit pending values before reading table rows.
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

void UpdateFullRowData(ISceneAdapter &adapter, wxDataViewListCtrl *table,
                       const std::vector<std::string> &rowUuids,
                       const std::vector<wxString> &gdtfPaths,
                       const std::unordered_set<std::string> *manualCategoryUuids,
                       std::unordered_set<std::string> *changedWeightPositions,
                       bool logChanges) {
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
    table->GetValue(v, row, 1);
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
