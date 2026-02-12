#pragma once

#include <vector>
#include <wx/dataview.h>
#include <wx/string.h>

class ColorfulDataViewListStore;

namespace FixtureTableColumns {

std::vector<wxString> DefaultLabels();
void ConfigureColumns(wxDataViewListCtrl *table,
                      const std::vector<wxString> &columnLabels);

} // namespace FixtureTableColumns
