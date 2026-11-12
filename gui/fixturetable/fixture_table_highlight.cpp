/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "fixturetablepanel.h"
#include "fixturetable/fixture_table_highlight_event.h"
#include <algorithm>
#include "fixturetablepanel_ui_helpers.h"
#include "guiconfigservices.h"
#include "configmanager.h"
#include "selection_movement_settings.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"
#include "trusstablepanel.h"
#include "hoisttablepanel.h"
#include "sceneobjecttablepanel.h"
#include <wx/utils.h>

// Applies a primary hover highlight to one fixture row.
void FixtureTablePanel::HighlightFixture(const std::string &uuid) {
  HighlightFixture(uuid, {});
}

// Applies primary and related group-hover highlights to fixture rows.
void FixtureTablePanel::HighlightFixture(
    const std::string &uuid, const std::vector<std::string> &relatedUuids, bool force) {
  if (!force && uuid == highlightedUuid && relatedUuids == highlightedRelatedUuids)
    return;

  auto findRow = [&](const std::string &candidate) -> int {
    if (candidate.empty())
      return wxNOT_FOUND;
    auto it = std::find(rowUuids.begin(), rowUuids.end(), candidate);
    if (it == rowUuids.end())
      return wxNOT_FOUND;
    int row = static_cast<int>(std::distance(rowUuids.begin(), it));
    if (row < 0 || row >= static_cast<int>(table->GetItemCount()))
      return wxNOT_FOUND;
    return row;
  };

  std::vector<bool> primaryRows(table->GetItemCount(), false);
  std::vector<bool> secondaryRows(table->GetItemCount(), false);
  const int currentRow = findRow(uuid);
  if (currentRow != wxNOT_FOUND)
    primaryRows[static_cast<size_t>(currentRow)] = true;
  for (const auto &relatedUuid : relatedUuids) {
    const int relatedRow = findRow(relatedUuid);
    if (relatedRow != wxNOT_FOUND && relatedRow != currentRow)
      secondaryRows[static_cast<size_t>(relatedRow)] = true;
  }
  store->SetHighlightRows(primaryRows, secondaryRows, gui::FixtureHoverColor(),
                          gui::FixtureRelatedHoverColor(), wxColour(0, 0, 0));

  highlightedUuid = uuid;
  highlightedRelatedUuids = relatedUuids;
  table->Refresh();
  wxThreadEvent changed(gui::FixtureHoverChanged);
  changed.SetPayload(gui::FixtureHoverHighlight{uuid, relatedUuids});
  wxGetTopLevelParent(this)->GetEventHandler()->ProcessEvent(changed);
}

// Publishes pointer feedback from the table or patch through existing viewers.
void FixtureTablePanel::PreviewFixtureHover(const std::string &candidate) {
  const auto &config = guiConfigServices->LegacyConfigManager();
  const auto &scene = config.GetScene();
  const std::string uuid = scene.fixtures.contains(candidate) ? candidate : std::string{};
  std::vector<std::string> fixtures, trusses, supports, objects;
  for (const auto &related : scene_grouping::ExpandHoverForGroupHighlights(
           scene, uuid, selection_movement_settings::LoadInteractiveTransformPolicy(config))) {
    if (scene.fixtures.contains(related)) fixtures.push_back(related);
    else if (scene.trusses.contains(related)) trusses.push_back(related);
    else if (scene.supports.contains(related)) supports.push_back(related);
    else if (scene.sceneObjects.contains(related)) objects.push_back(related);
  }
  HighlightFixture(uuid, fixtures);
  if (auto *table = TrussTablePanel::Instance()) table->HighlightTruss({}, trusses);
  if (auto *table = HoistTablePanel::Instance()) table->HighlightHoist({}, supports);
  if (auto *table = SceneObjectTablePanel::Instance()) table->HighlightObject({}, objects);
  if (auto *viewer = Viewer2DPanel::Instance()) viewer->SetExternalHoverUuid(uuid);
  if (auto *viewer = Viewer3DPanel::Instance()) viewer->SetExternalHoverUuid(uuid);
}

// Hit-test stable row keys so sorting cannot retarget pointer feedback.
void FixtureTablePanel::OnMouseMove(wxMouseEvent &event) {
  const auto position = NormalizeMousePositionForTable(table, event);
  UpdateHoverTooltip(position);
  wxDataViewItem item; wxDataViewColumn *column = nullptr;
  table->HitTest(position, item, column);
  const auto found = item.IsOk() ? rowUuidByKey.find(store->GetItemData(item)) : rowUuidByKey.end();
  PreviewFixtureHover(found == rowUuidByKey.end() ? std::string{} : found->second);
  event.Skip();
}
void FixtureTablePanel::OnMouseLeave(wxMouseEvent &event) {
  // Native tables contain child windows; crossing them is not leaving the table.
  if (!table->GetScreenRect().Contains(wxGetMousePosition())) {
    PreviewFixtureHover({});
    if (!activeHoverTooltip.IsEmpty()) {
      SetTableAndChildTooltips(table, wxString());
      activeHoverTooltip.clear();
    }
  }
  event.Skip();
}
