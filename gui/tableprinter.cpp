#include "tableprinter.h"
#include "columnselectiondialog.h"
#include <wx/dataview.h>
#include <wx/pdfdoc.h>
#include <wx/filename.h>
#include <wx/utils.h>
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

    wxPdfDocument pdf;
    double margin = 10.0;
    pdf.SetMargins(margin, margin, margin);
    pdf.SetAutoPageBreak(true, margin);
    pdf.AddPage();
    pdf.SetFont("Helvetica", "", 8);

    std::vector<double> colWidths(selCols.size(), 0.0);
    for (size_t i = 0; i < selCols.size(); ++i) {
        int c = selCols[i];
        double w = pdf.GetStringWidth(table->GetColumn(c)->GetTitle()) + 2.0;
        colWidths[i] = std::max(colWidths[i], w);
    }
    for (unsigned int r = 0; r < table->GetItemCount(); ++r) {
        for (size_t i = 0; i < selCols.size(); ++i) {
            int c = selCols[i];
            wxVariant val;
            table->GetValue(val, r, c);
            double w = pdf.GetStringWidth(val.GetString()) + 2.0;
            colWidths[i] = std::max(colWidths[i], w);
        }
    }

    double availWidth = pdf.GetPageWidth() - pdf.GetLeftMargin() - pdf.GetRightMargin();
    double totalWidth = 0.0;
    for (double w : colWidths)
        totalWidth += w;
    if (totalWidth > 0 && totalWidth > availWidth) {
        double scale = availWidth / totalWidth;
        for (double &w : colWidths)
            w *= scale;
    }

    double cellHeight = pdf.GetFontSize() + 2.0;

    pdf.SetFillColour(200, 200, 200);
    for (size_t i = 0; i < selCols.size(); ++i) {
        int c = selCols[i];
        pdf.Cell(colWidths[i], cellHeight, table->GetColumn(c)->GetTitle(), wxPDF_BORDER_FRAME, 0, wxPDF_ALIGN_LEFT, true);
    }
    pdf.Ln(cellHeight);

    for (unsigned int r = 0; r < table->GetItemCount(); ++r) {
        if (r % 2 == 0)
            pdf.SetFillColour(242, 242, 242);
        else
            pdf.SetFillColour(255, 255, 255);
        for (size_t i = 0; i < selCols.size(); ++i) {
            int c = selCols[i];
            wxVariant val;
            table->GetValue(val, r, c);
            pdf.Cell(colWidths[i], cellHeight, val.GetString(), wxPDF_BORDER_FRAME, 0, wxPDF_ALIGN_LEFT, true);
        }
        pdf.Ln(cellHeight);
    }

    wxString tmp = wxFileName::CreateTempFileName("tableprinter");
    wxString path = tmp + ".pdf";
    pdf.SaveAsFile(path);
    wxLaunchDefaultApplication(path);
}

} // namespace TablePrinter

