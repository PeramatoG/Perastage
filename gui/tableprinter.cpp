#include "tableprinter.h"
#include "columnselectiondialog.h"
#include <wx/dataview.h>
#include <wx/html/htmprint.h>

namespace TablePrinter {

void Print(wxWindow* parent, wxDataViewListCtrl* table)
{
    if (!table)
        return;
    std::vector<std::string> cols;
    for (unsigned int i = 0; i < table->GetColumnCount(); ++i)
        cols.push_back(std::string(table->GetColumn(i)->GetTitle().ToUTF8()));
    ColumnSelectionDialog dlg(parent, cols);
    if (dlg.ShowModal() != wxID_OK)
        return;
    std::vector<int> selCols = dlg.GetSelectedColumns();
    if (selCols.empty())
        return;
    wxHtmlEasyPrinting printer;
    wxString html = "<html><body><table border=1 cellspacing=0 cellpadding=2><tr>";
    for (int c : selCols) {
        html += "<th>";
        html += table->GetColumn(c)->GetTitle();
        html += "</th>";
    }
    html += "</tr>";
    for (unsigned int r = 0; r < table->GetItemCount(); ++r) {
        html += "<tr>";
        for (int c : selCols) {
            wxVariant val;
            table->GetValue(val, r, c);
            html += "<td>";
            html += val.GetString();
            html += "</td>";
        }
        html += "</tr>";
    }
    html += "</table></body></html>";
    printer.PrintText(html, "Table");
}

} // namespace TablePrinter
