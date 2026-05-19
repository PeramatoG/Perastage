#pragma once

#include "fixture.h"
#include <wx/dataview.h>

// Applies the same tooltip text to the table and all child windows.
void SetTableAndChildTooltips(wxDataViewListCtrl *table,
                              const wxString &tooltip);

// Converts event mouse coordinates into the table's client coordinate space.
wxPoint NormalizeMousePositionForTable(wxDataViewListCtrl *table,
                                       const wxMouseEvent &event);

// Returns column-specific tooltip text for known fixture validation conflicts.
wxString BuildFixtureTooltipForColumn(int modelColumn);

// Returns tooltip text for fixture categories assigned by fallback logic.
wxString BuildCategoryFallbackTooltip(const Fixture &fixture);

// Binds hover-related mouse events to the table and each internal child window.
template <typename Owner>
void BindTableHoverEvents(wxDataViewListCtrl *table, Owner *owner,
                          void (Owner::*onMouseMove)(wxMouseEvent &),
                          void (Owner::*onMouseLeave)(wxMouseEvent &)) {
  if (!table || !owner)
    return;

  auto bindEvents = [&](wxWindow *window) {
    if (!window)
      return;
    window->Unbind(wxEVT_MOTION, onMouseMove, owner);
    window->Unbind(wxEVT_LEAVE_WINDOW, onMouseLeave, owner);
    window->Bind(wxEVT_MOTION, onMouseMove, owner);
    window->Bind(wxEVT_LEAVE_WINDOW, onMouseLeave, owner);
  };

  bindEvents(table);
  wxWindowList &children = table->GetChildren();
  for (wxWindowList::compatibility_iterator it = children.GetFirst(); it;
       it = it->GetNext()) {
    bindEvents(it->GetData());
  }
}
