#include "fixture_table_edit_service.h"

#include "consolepanel.h"
#include "gdtfloader.h"
#include "matrixutils.h"

#include <algorithm>
#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

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

  if (col == 18) {
    std::unordered_map<std::string, wxVariant> typeValues;
    for (const auto &it : selections) {
      int r = table->ItemToRow(it);
      if (r == wxNOT_FOUND)
        continue;
      wxVariant vType, vVal;
      table->GetValue(vType, r, 2);
      table->GetValue(vVal, r, col);
      typeValues[std::string(vType.GetString().ToUTF8())] = vVal;
    }

    unsigned int rowCount = table->GetItemCount();
    for (unsigned int i = 0; i < rowCount; ++i) {
      wxVariant vType;
      table->GetValue(vType, i, 2);
      auto it = typeValues.find(std::string(vType.GetString().ToUTF8()));
      if (it != typeValues.end())
        table->SetValue(it->second, i, col);
    }
    return;
  }

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
                     const std::vector<wxString> &gdtfPaths) {
  adapter.PushUndoState("edit fixture");
  auto &scene = adapter.GetScene();

  std::unordered_set<std::string> updatedSpecs;
  size_t updatedCount = 0;
  wxString firstName, firstUuid;

  size_t count = std::min((size_t)table->GetItemCount(), rowUuids.size());
  for (size_t i = 0; i < count; ++i) {
    auto it = scene.fixtures.find(rowUuids[i]);
    if (it == scene.fixtures.end())
      continue;

    if (i < gdtfPaths.size())
      it->second.gdtfSpec = std::string(gdtfPaths[i].ToUTF8());

    wxVariant v;
    table->GetValue(v, i, 1);
    it->second.instanceName = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, 0);
    it->second.fixtureId = static_cast<int>(v.GetLong());

    table->GetValue(v, i, 3);
    std::string layerStr = std::string(v.GetString().ToUTF8());
    it->second.layer = layerStr;

    table->GetValue(v, i, 4);
    it->second.positionName = std::string(v.GetString().ToUTF8());
    if (!it->second.position.empty())
      scene.positions[it->second.position] = it->second.positionName;

    table->GetValue(v, i, 5);
    long uni = v.GetLong();
    table->GetValue(v, i, 6);
    long ch = v.GetLong();

    table->GetValue(v, i, 2);
    it->second.typeName = std::string(v.GetString().ToUTF8());

    table->GetValue(v, i, 7);
    it->second.gdtfMode = std::string(v.GetString().ToUTF8());

    if (uni > 0 && ch > 0)
      it->second.address = wxString::Format("%ld.%ld", uni, ch).ToStdString();
    else
      it->second.address.clear();

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
      s.Replace("\u00B0", "");
      s.ToDouble(&roll);
    }
    table->GetValue(v, i, 14);
    {
      wxString s = v.GetString();
      s.Replace("\u00B0", "");
      s.ToDouble(&pitch);
    }
    table->GetValue(v, i, 15);
    {
      wxString s = v.GetString();
      s.Replace("\u00B0", "");
      s.ToDouble(&yaw);
    }

    const auto currentEuler = MatrixUtils::MatrixToEuler(it->second.transform);
    const bool transformChanged =
        wxString::Format("%.3f", it->second.transform.o[0] / 1000.0f) !=
            wxString::Format("%.3f", x) ||
        wxString::Format("%.3f", it->second.transform.o[1] / 1000.0f) !=
            wxString::Format("%.3f", y) ||
        wxString::Format("%.3f", it->second.transform.o[2] / 1000.0f) !=
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
      it->second.transform = MatrixUtils::ApplyRotationPreservingScale(
          it->second.transform, rot,
          {static_cast<float>(x * 1000.0), static_cast<float>(y * 1000.0),
           static_cast<float>(z * 1000.0)});
    }

    table->GetValue(v, i, 16);
    double pw = 0.0;
    v.GetString().ToDouble(&pw);
    it->second.powerConsumptionW = static_cast<float>(pw);

    table->GetValue(v, i, 17);
    double wt = 0.0;
    v.GetString().ToDouble(&wt);
    it->second.weightKg = static_cast<float>(wt);

    table->GetValue(v, i, 18);
    if (v.GetType() == "wxDataViewIconText") {
      wxDataViewIconText icon;
      icon << v;
      wxString txt = icon.GetText();
      if (!txt.IsEmpty())
        it->second.color = std::string(txt.ToUTF8());
      else
        it->second.color.clear();
    } else {
      it->second.color = std::string(v.GetString().ToUTF8());
    }

    if (!it->second.color.empty() && !it->second.gdtfSpec.empty()) {
      std::string gdtfPath = it->second.gdtfSpec;
      fs::path p = gdtfPath;
      if (p.is_relative() && !scene.basePath.empty())
        gdtfPath = (fs::path(scene.basePath) / p).string();
      if (updatedSpecs.insert(gdtfPath).second)
        SetGdtfModelColor(gdtfPath, it->second.color);
    }

    if (ConsolePanel::Instance()) {
      ++updatedCount;
      if (updatedCount == 1) {
        firstName = wxString::FromUTF8(it->second.instanceName.c_str());
        firstUuid = wxString::FromUTF8(it->second.uuid.c_str());
      }
    }
  }

  if (ConsolePanel::Instance()) {
    wxString msg;
    if (updatedCount == 1)
      msg = wxString::Format("Updated fixture %s (UUID %s)", firstName,
                             firstUuid);
    else if (updatedCount > 1)
      msg = wxString::Format("Updated %zu fixtures", updatedCount);
    if (!msg.empty())
      ConsolePanel::Instance()->AppendMessage(msg);
  }
}

} // namespace FixtureTableEditService
