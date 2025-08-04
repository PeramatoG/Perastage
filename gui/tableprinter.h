#pragma once

#include <vector>
#include <string>

class wxWindow;
class wxDataViewListCtrl;

namespace TablePrinter {
void Print(wxWindow* parent, wxDataViewListCtrl* table);
}
