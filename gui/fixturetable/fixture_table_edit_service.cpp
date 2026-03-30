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
                     bool logChanges) {
  // Ensure in-place cell editors commit pending values before reading table rows.
  if (table)
    DataViewEditCommit::CommitPendingEdit(table);

  auto &scene = adapter.GetScene();
  const auto distanceUnitSystem = adapter.GetDistanceUnitSystem();
  const auto weightUnitSystem = adapter.GetWeightUnitSystem();

  size_t updatedCount = 0;
  wxString firstName, firstUuid;
  bool anyChanged = false;
  bool undoPushed = false;
  auto pushUndoIfNeeded = [&]() {
    if (!undoPushed) {
      adapter.PushUndoState("edit fixture");
      undoPushed = true;
    }
  };

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
        !Units::NearlyEqualDistanceMillimeters(old.transform.o[0], xMm,
                                                     0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.transform.o[1], yMm,
                                                     0.5) ||
        !Units::NearlyEqualDistanceMillimeters(old.transform.o[2], zMm,
                                                     0.5) ||
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
    if (const auto parsedWeightKg = Units::ParseWeightToKilograms(
            std::string(v.GetString().ToUTF8()), weightUnitSystem);
        parsedWeightKg.has_value()) {
      next.weightKg = static_cast<float>(*parsedWeightKg);
    }

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

    pushUndoIfNeeded();
    anyChanged = true;
    it->second = next;
    if (!next.typeName.empty() && !next.category.empty() &&
        next.categorySource == GdtfFixtureCategory::kManualSource) {
      GdtfDictionary::UpdateCategoryForFile(next.typeName, next.gdtfSpec,
                                            next.category);
    }
    if (!it->second.position.empty())
      scene.positions[it->second.position] = it->second.positionName;

    if (logChanges && ConsolePanel::Instance()) {
      ++updatedCount;
      if (updatedCount == 1) {
        firstName = wxString::FromUTF8(it->second.instanceName.c_str());
        firstUuid = wxString::FromUTF8(it->second.uuid.c_str());
      }
    }
  }

  if (!anyChanged)
    return;

  if (logChanges) {
    ConsolePanel *console = ConsolePanel::Instance();
    if (console) {
      wxString msg;
      if (updatedCount == 1)
        msg = "Updated fixture " + firstName + " (UUID " + firstUuid + ")";
      else if (updatedCount > 1)
        msg = wxString::Format("Updated %zu fixtures", updatedCount);
      if (!msg.empty())
        console->AppendMessage(msg);
    }
  }
}

} // namespace FixtureTableEditService
