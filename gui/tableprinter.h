#pragma once

#include <vector>
#include <string>

class wxWindow;
class wxDataViewListCtrl;

namespace TablePrinter {
enum class TableType { Fixtures, Trusses, SceneObjects };
void Print(wxWindow* parent, wxDataViewListCtrl* table, TableType type);
}
