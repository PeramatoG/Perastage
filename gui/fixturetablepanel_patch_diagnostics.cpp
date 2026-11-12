/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "fixturetablepanel.h"
#include "fixturetable/fixture_table_columns.h"
#include "guiconfigservices.h"
#include "configmanager.h"
#include "query/scene_query.h"
#include "fixturetable/patch_warning_ranges.h"
#include <wx/utils.h>
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

// The table's channel count is the existing break-1 GDTF projection, not a sum
// of breaks. Query Core owns validation; hypothetical ranges are presentation only.
void FixtureTablePanel::HighlightPatchConflicts() {
  patchWarnings.clear();
  std::unordered_map<std::string, int> footprints;
  for (unsigned row = 0; row < table->GetItemCount(); ++row) {
    store->ClearCellTextColour(row, 5);
    store->ClearCellTextColour(row, 6);
    wxVariant value;
    table->GetValue(value, row, FixtureTableColumns::ToIndex(
        FixtureTableColumns::Column::ChannelCount));
    long count = 0;
    if (row < rowUuids.size() && value.GetString().ToLong(&count) && count > 0)
      footprints[rowUuids[row]] = static_cast<int>(count);
  }
  const auto &config = static_cast<const ConfigManager &>(guiConfigServices->LegacyConfigManager());
  const auto status = perastage::query::GetPatchStatus(config.GetScene(),
      [&](const Fixture &fixture) -> std::optional<int> {
        const auto it = footprints.find(fixture.uuid);
        return it == footprints.end() ? std::nullopt : std::optional<int>(it->second);
      });
  auto append = [&](const std::string &uuid, const wxString &text) {
    auto &message = patchWarnings[uuid];
    if (!message.empty()) message += "\n";
    message += text;
  };
  for (const auto &fixture : status.fixtures) {
    for (const auto &diagnostic : status.diagnostics) {
      if (!diagnostic.object || diagnostic.object->uuid != fixture.uuid) continue;
      if (diagnostic.code == "scene.patch.invalid_address")
        append(fixture.uuid, _("Invalid DMX start address. The channel must be between 1 and 512."));
      else if (diagnostic.code == "scene.patch.unresolved_footprint")
        append(fixture.uuid, _("The GDTF DMX footprint is unknown or unresolved."));
      else if (diagnostic.code == "scene.patch.range_exceeds_universe") {
        const auto ranges = wxString::FromUTF8(fixture_patch_presentation::SpanRanges(
            *fixture.universe, *fixture.startChannel, *fixture.footprint));
        append(fixture.uuid, wxString::Format(_("DMX footprint exceeds universe %d. Primary footprint: %d channels. Start: %s. Hypothetical continuous Auto Span ranges: %s. The manually entered start address is preserved. Continuous operation requires routing support in the receiving or controlling application; these ranges do not define additional DMX breaks."),
            *fixture.universe, *fixture.footprint, wxString::FromUTF8(fixture.rawAddress), ranges));
      }
    }
  }
  std::unordered_set<std::string> overlaps;
  for (const auto &conflict : status.conflicts)
    for (const auto &uuid : conflict.fixtureUuids) {
      overlaps.insert(uuid);
      append(uuid, _("DMX overlap with another fixture in the assigned universe."));
    }
  for (unsigned row = 0; row < rowUuids.size() && row < table->GetItemCount(); ++row)
    if (patchWarnings.contains(rowUuids[row])) {
      store->SetCellTextColour(row, 6, *wxRED);
      if (overlaps.contains(rowUuids[row])) store->SetCellTextColour(row, 5, *wxRED);
    }
  if (!activeHoverTooltip.empty())
    UpdateHoverTooltip(table->ScreenToClient(wxGetMousePosition()));
}
