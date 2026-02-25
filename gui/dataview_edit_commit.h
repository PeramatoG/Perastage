#pragma once

class wxDataViewListCtrl;

namespace DataViewEditCommit {

// Commits an active in-place editor by moving focus away from the table.
// This is compatible with wx versions where wxDataViewListCtrl has no
// FinishEditing() API.
void CommitPendingEdit(wxDataViewListCtrl *table);

} // namespace DataViewEditCommit
