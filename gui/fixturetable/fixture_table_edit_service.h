#pragma once

#include "mvrscene.h"
#include <string>
#include <unordered_set>
#include <vector>
#include <wx/dataview.h>

class wxDataViewListCtrl;

namespace FixtureTableEditService {

class ISceneAdapter {
public:
  virtual ~ISceneAdapter() = default;
  virtual void PushUndoState(const std::string &description) = 0;
  virtual MvrScene &GetScene() = 0;
};

std::vector<int> BuildOrderedRows(const std::vector<int> &selectedRows,
                                  const std::vector<int> &selectionOrder);

void PropagateTypeValues(wxDataViewListCtrl *table,
                         const wxDataViewItemArray &selections, int col);

void UpdateSceneData(ISceneAdapter &adapter, wxDataViewListCtrl *table,
                     const std::vector<std::string> &rowUuids,
                     const std::vector<wxString> &gdtfPaths);

} // namespace FixtureTableEditService
