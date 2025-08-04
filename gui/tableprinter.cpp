#include "tableprinter.h"
#include "columnselectiondialog.h"
#include <wx/dataview.h>
#include <wx/html/htmprint.h>
#include <algorithm>

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
    printer.SetStandardFonts(8);

    std::vector<size_t> maxWidths(selCols.size(), 0);
    for (size_t i = 0; i < selCols.size(); ++i) {
        int c = selCols[i];
        maxWidths[i] = std::max(
            maxWidths[i],
            static_cast<size_t>(table->GetColumn(c)->GetTitle().Length()));
    }
    for (unsigned int r = 0; r < table->GetItemCount(); ++r) {
        for (size_t i = 0; i < selCols.size(); ++i) {
            int c = selCols[i];
            wxVariant val;
            table->GetValue(val, r, c);
            maxWidths[i] = std::max(
                maxWidths[i],
                static_cast<size_t>(val.GetString().Length()));
        }
    }

    wxString html =
        "<html><head><style>body{margin:10mm;font-size:8pt;}table{border-collapse:collapse;}"
        "td,th{white-space:nowrap;}tr:nth-child(even){background-color:#f2f2f2;}</style></head><body><table border=1 cellspacing=0 cellpadding=2><tr>";
    for (size_t i = 0; i < selCols.size(); ++i) {
        int c = selCols[i];
        html += wxString::Format("<th style=\"width:%zuch;\">", maxWidths[i]);
        html += table->GetColumn(c)->GetTitle();
        html += "</th>";
    }
    html += "</tr>";
    for (unsigned int r = 0; r < table->GetItemCount(); ++r) {
        html += "<tr>";
        for (size_t i = 0; i < selCols.size(); ++i) {
            int c = selCols[i];
            wxVariant val;
            table->GetValue(val, r, c);
            html += wxString::Format("<td style=\"width:%zuch;\">", maxWidths[i]);
            html += val.GetString();
            html += "</td>";
        }
        html += "</tr>";
    }
    html += "</table></body></html>";
    printer.PrintText(html, "Table");
}

} // namespace TablePrinter
