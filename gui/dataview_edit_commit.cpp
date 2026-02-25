#include "dataview_edit_commit.h"

#include <wx/dataview.h>
#include <wx/window.h>
#include <wx/utils.h>

namespace DataViewEditCommit {

void CommitPendingEdit(wxDataViewListCtrl *table) {
  if (!table)
    return;

  wxWindow *focused = wxWindow::FindFocus();
  if (!focused)
    return;

  // Only force focus changes when editing is happening inside this control.
  if (focused != table && !table->IsDescendant(focused))
    return;

  wxWindow *target = table->GetParent() ? table->GetParent() : table;
  if (target == focused)
    return;

  target->SetFocus();
  wxYieldIfNeeded();
}

} // namespace DataViewEditCommit
