/* This file is part of Perastage, licensed under GPL-3.0-or-later. */
#pragma once
#include <wx/dataview.h>
#include <array>
#include <functional>

// Native table shared by the queue and its interactive source selectors.
class GdtfDownloadQueueTable final : public wxDataViewCtrl {
public:
  explicit GdtfDownloadQueueTable(wxWindow *parent);
  long InsertItem(long row, const wxString &type);
  void SetItem(long row, int column, const wxString &text);
  wxString GetItemText(long row, int column) const;
  void SetItemTextColour(long row, const wxColour &colour);
  wxColour GetItemTextColour(long row) const;
  unsigned GetItemCount() const;
  void SelectRow(unsigned row);
  int SelectedRow() const;
  void EnableSourceChoices(std::function<void(unsigned, unsigned)> select);
  void SetSourceChoices(unsigned row, const std::array<bool, 3> &available,
                        int selected, bool enabled);
  bool SourceEnabled(unsigned row, unsigned source) const;
  bool SourceChecked(unsigned row, unsigned source) const;
private:
  class Model;
  Model *model = nullptr;
  bool updating = false;
  std::function<void(unsigned, unsigned)> selectSource;
};
