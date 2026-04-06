#pragma once

#include <string>
#include <vector>

#include <wx/dialog.h>

class wxDataViewEvent;
class wxDataViewListCtrl;
class wxCommandEvent;

class DictionaryExportConflictDialog : public wxDialog {
public:
  struct Item {
    std::string file_name;
    std::string existing_path;
    std::string new_source_path;
    bool use_new = true;
  };

  DictionaryExportConflictDialog(wxWindow *parent, const wxString &title,
                                 std::vector<Item> items);

  const std::vector<Item> &GetItems() const;

private:
  void RefreshTable();
  void SetAllToExisting(wxCommandEvent &event);
  void SetAllToNew(wxCommandEvent &event);
  void OnTableValueChanged(wxDataViewEvent &event);

  std::vector<Item> items_;
  wxDataViewListCtrl *table_ = nullptr;
};
