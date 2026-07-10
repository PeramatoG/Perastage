#pragma once

#include "gdtf/gdtf_mode_browser_presenter.h"

#include <map>
#include <memory>
#include <vector>

#include <wx/dataview.h>

class GdtfModeDataViewModel : public wxDataViewModel {
public:
  enum Column { Item = 0, Address, DmxRange, PhysicalRange, Unit, ColumnCount };

  GdtfModeDataViewModel();

  void SetNodes(const std::vector<GdtfModeBrowserNodePresentation> &nodes);
  const GdtfModeBrowserNodePresentation *GetNode(const wxDataViewItem &item) const;
  wxDataViewItem GetItemById(const std::string &id) const;
  std::vector<wxDataViewItem> GetTopLevelItems() const;

  unsigned int GetColumnCount() const override;
  wxString GetColumnType(unsigned int col) const override;
  void GetValue(wxVariant &variant, const wxDataViewItem &item, unsigned int col) const override;
  bool SetValue(const wxVariant &variant, const wxDataViewItem &item, unsigned int col) override;
  wxDataViewItem GetParent(const wxDataViewItem &item) const override;
  bool IsContainer(const wxDataViewItem &item) const override;
  unsigned int GetChildren(const wxDataViewItem &item, wxDataViewItemArray &children) const override;

private:
  struct Node;
  std::vector<std::unique_ptr<Node>> ownedNodes;
  std::vector<Node *> roots;
  std::map<std::string, Node *> byId;
};
