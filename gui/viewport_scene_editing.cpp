/*
 * This file is part of Perastage.
 * Copyright (C) 2026 Luisma Peramato
 * License: GNU General Public License version 3 or later
 */
#include "viewport_scene_editing.h"
#include "configmanager.h"
#include "fixture_patch_service.h"
#include "fixturepatchdialog.h"
#include "fixturetablepanel.h"
#include "guiconfigservices.h"
#include "mainwindow.h"
#include "project_mutation_host.h"
#include "trusstablepanel.h"

// Resolve identities through the table's stable item keys, including sorted rows.
void FixtureTablePanel::EditByUuid(const std::string &uuid) {
  for (unsigned row = 0; row < table->GetItemCount(); ++row) {
    const auto item = table->RowToItem(row);
    if (UuidForItem(item) != uuid) continue;
    wxDataViewEvent event(wxEVT_DATAVIEW_ITEM_ACTIVATED, table, item);
    OnItemActivated(event);
    return;
  }
}

void TrussTablePanel::EditByUuid(const std::string &uuid) {
  const auto item = ItemForUuid(uuid);
  if (!item.IsOk()) return;
  wxDataViewEvent event(wxEVT_DATAVIEW_ITEM_ACTIVATED, table, item);
  OnItemActivated(event);
}

namespace gui::viewport {
bool ApplyFixturePatch(ConfigManager &config, const std::string &uuid,
                       int fixtureId, const std::string &address,
                       std::string &error) {
  auto &scene = config.GetScene();
  const auto found = scene.fixtures.find(uuid);
  if (found == scene.fixtures.end() || fixtureId < 0) return false;
  Fixture candidate = found->second;
  const auto result = perastage::patch::SetFixturePrimaryPatch(candidate, address);
  if (!result) { error = result.detail; return false; }
  candidate.fixtureId = fixtureId;
  if (candidate.fixtureId == found->second.fixtureId &&
      candidate.address == found->second.address &&
      candidate.dmxAddresses == found->second.dmxAddresses) return false;
  const scene_grouping::ObjectSelection selection{
      config.GetSelectedFixtures(), config.GetSelectedTrusses(),
      config.GetSelectedSupports(), config.GetSelectedSceneObjects(),
      config.GetSelectedGroupObjects()};
  GuiProjectMutationHost host(config);
  const MvrScene before = scene;
  found->second = std::move(candidate);
  host.CommitMutation(before, selection, "edit fixture patch");
  return true;
}

bool EditFixturePatch(wxWindow *parent, const std::string &uuid) {
  auto &config = GetDefaultGuiConfigServices().LegacyConfigManager();
  const auto found = config.GetScene().fixtures.find(uuid);
  if (found == config.GetScene().fixtures.end()) return false;
  FixturePatchDialog dialog(parent, found->second);
  if (dialog.ShowModal() != wxID_OK) return false;
  const int universe = dialog.GetUniverse(), channel = dialog.GetChannel();
  const std::string address = universe == 0 ? std::string{}
      : std::to_string(universe) + "." + std::to_string(channel);
  std::string error;
  if (!ApplyFixturePatch(config, uuid, dialog.GetFixtureId(), address, error)) {
    if (!error.empty()) wxMessageBox(wxString::FromUTF8(error),
        _("Edit DMX Patch"), wxOK | wxICON_ERROR, parent);
    return false;
  }
  if (MainWindow::Instance()) MainWindow::Instance()->RefreshAfterToolSceneUpdate();
  return true;
}
} // namespace gui::viewport
