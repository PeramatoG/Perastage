#include "fixture_table_edit_service.h"

#include "consolepanel.h"
#include "matrixutils.h"

#include <algorithm>
#include <unordered_map>

namespace FixtureTableEditService {

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
  if (col != 16 && col != 17 && col != 18)
    return;

  if (col == 18)
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
                     bool logChanges) {
  auto &scene = adapter.GetScene();

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

    double x = 0, y = 0, z = 0;
    table->GetValue(v, i, 10);
    v.GetString().ToDouble(&x);
    table->GetValue(v, i, 11);
    v.GetString().ToDouble(&y);
    table->GetValue(v, i, 12);
    v.GetString().ToDouble(&z);

    double roll = 0, pitch = 0, yaw = 0;
    table->GetValue(v, i, 13);
    {
      wxString s = v.GetString();
      s.Replace("°", "");
      s.ToDouble(&roll);
    }
    table->GetValue(v, i, 14);
    {
      wxString s = v.GetString();
      s.Replace("°", "");
      s.ToDouble(&pitch);
    }
    table->GetValue(v, i, 15);
    {
      wxString s = v.GetString();
      s.Replace("°", "");
      s.ToDouble(&yaw);
    }

    const auto currentEuler = MatrixUtils::MatrixToEuler(old.transform);
    const bool transformChanged =
        wxString::Format("%.3f", old.transform.o[0] / 1000.0f) !=
            wxString::Format("%.3f", x) ||
        wxString::Format("%.3f", old.transform.o[1] / 1000.0f) !=
            wxString::Format("%.3f", y) ||
        wxString::Format("%.3f", old.transform.o[2] / 1000.0f) !=
            wxString::Format("%.3f", z) ||
        wxString::Format("%.1f", currentEuler[2]) !=
            wxString::Format("%.1f", roll) ||
        wxString::Format("%.1f", currentEuler[1]) !=
            wxString::Format("%.1f", pitch) ||
        wxString::Format("%.1f", currentEuler[0]) !=
            wxString::Format("%.1f", yaw);

    if (transformChanged) {
      Matrix rot = MatrixUtils::EulerToMatrix(
          static_cast<float>(yaw), static_cast<float>(pitch),
          static_cast<float>(roll));
      next.transform = MatrixUtils::ApplyRotationPreservingScale(
          old.transform, rot,
          {static_cast<float>(x * 1000.0), static_cast<float>(y * 1000.0),
           static_cast<float>(z * 1000.0)});
    }

    table->GetValue(v, i, 16);
    double pw = 0.0;
    v.GetString().ToDouble(&pw);
    next.powerConsumptionW = static_cast<float>(pw);

    table->GetValue(v, i, 17);
    double wt = 0.0;
    v.GetString().ToDouble(&wt);
    next.weightKg = static_cast<float>(wt);

    table->GetValue(v, i, 18);
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
                                old.weightKg != next.weightKg ||
                                old.color != next.color;
    if (!fixtureChanged)
      continue;

    pushUndoIfNeeded();
    anyChanged = true;
    it->second = next;
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
        msg = wxString::Format("Updated fixture %s (UUID %s)", firstName,
                               firstUuid);
      else if (updatedCount > 1)
        msg = wxString::Format("Updated %zu fixtures", updatedCount);
      if (!msg.empty())
        console->AppendMessage(msg);
    }
  }
}

} // namespace FixtureTableEditService
