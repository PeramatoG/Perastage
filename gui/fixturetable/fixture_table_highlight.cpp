/* This file is part of Perastage. License: GPL-3.0-or-later. */
#include "fixturetablepanel.h"
#include "fixturetable/fixture_table_highlight_event.h"
#include <algorithm>

// Applies a primary hover highlight to one fixture row.
void FixtureTablePanel::HighlightFixture(const std::string &uuid) {
  HighlightFixture(uuid, {});
}

// Applies primary and related group-hover highlights to fixture rows.
void FixtureTablePanel::HighlightFixture(
    const std::string &uuid, const std::vector<std::string> &relatedUuids) {
  if (uuid == highlightedUuid && relatedUuids == highlightedRelatedUuids)
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
