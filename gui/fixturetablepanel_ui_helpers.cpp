#include "fixturetablepanel_ui_helpers.h"

// Applies the same tooltip text to the table and its child windows.
void SetTableAndChildTooltips(wxDataViewListCtrl *table,
                              const wxString &tooltip) {
  if (!table)
    return;

  table->SetToolTip(tooltip);
  wxWindowList &children = table->GetChildren();
  for (wxWindowList::compatibility_iterator it = children.GetFirst(); it;
       it = it->GetNext()) {
    if (wxWindow *child = it->GetData())
      child->SetToolTip(tooltip);
  }
}

// Converts mouse coordinates from child windows into table client coordinates.
wxPoint NormalizeMousePositionForTable(wxDataViewListCtrl *table,
                                       const wxMouseEvent &event) {
  wxPoint position = event.GetPosition();
  wxWindow *sourceWindow = dynamic_cast<wxWindow *>(event.GetEventObject());
  if (!table || !sourceWindow || sourceWindow == table)
    return position;

  return table->ScreenToClient(sourceWindow->ClientToScreen(position));
}

// Returns tooltip text describing known validation conflicts for a column.
wxString BuildFixtureTooltipForColumn(int modelColumn) {
  if (modelColumn == 0)
    return "Duplicate Fixture ID. Each fixture must have a unique ID.";
  if (modelColumn == 5 || modelColumn == 6)
    return "DMX patch conflict detected. Universe and channel overlap with another fixture.";
  return wxString();
}

// Returns tooltip text when a fixture category was auto-assigned by fallback rules.
wxString BuildCategoryFallbackTooltip(const Fixture &fixture) {
  if (fixture.categorySource != GdtfFixtureCategory::kAutoFallbackSource)
    return wxString();
  if (!fixture.categorySourceReason.empty()) {
    return wxString::Format(
        "Category auto-assigned by fallback: %s.",
        wxString::FromUTF8(fixture.categorySourceReason).c_str());
  }
  return "Category auto-assigned by fallback.";
}
