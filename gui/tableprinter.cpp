#include "tableprinter.h"
#include "columnselectiondialog.h"
#include "configmanager.h"
#include <wx/dataview.h>
#include <wx/html/htmprint.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <algorithm>
#include <fstream>

namespace TablePrinter {

void Print(wxWindow* parent, wxDataViewListCtrl* table, TableType type)
{
    if (!table)
        return;

    std::vector<std::string> cols;
    for (unsigned int i = 0; i < table->GetColumnCount(); ++i)
        cols.push_back(std::string(table->GetColumn(i)->GetTitle().ToUTF8()));

    std::vector<std::string> saved;
    switch (type)
    {
    case TableType::Fixtures: saved = ConfigManager::Get().GetFixturePrintColumns(); break;
    case TableType::Trusses: saved = ConfigManager::Get().GetTrussPrintColumns(); break;
    case TableType::SceneObjects: saved = ConfigManager::Get().GetSceneObjectPrintColumns(); break;
    }

    std::vector<int> defaultIdx;
    for (const auto& name : saved)
    {
        auto it = std::find(cols.begin(), cols.end(), name);
        if (it != cols.end())
            defaultIdx.push_back(static_cast<int>(std::distance(cols.begin(), it)));
    }

    ColumnSelectionDialog dlg(parent, cols, defaultIdx);
    if (dlg.ShowModal() != wxID_OK)
        return;

    std::vector<int> selCols = dlg.GetSelectedColumns();
    if (selCols.empty())
        return;

    std::vector<std::string> toSave;
    for (int c : selCols)
        toSave.push_back(cols[c]);

    switch (type)
    {
    case TableType::Fixtures: ConfigManager::Get().SetFixturePrintColumns(toSave); break;
    case TableType::Trusses: ConfigManager::Get().SetTrussPrintColumns(toSave); break;
    case TableType::SceneObjects: ConfigManager::Get().SetSceneObjectPrintColumns(toSave); break;
    }

    static wxHtmlEasyPrinting printer("Table Printer", parent);
    printer.SetParentWindow(parent);
    printer.SetStandardFonts(8, "Helvetica", "Courier");
    auto *pageSetup = printer.GetPageSetupData();
    pageSetup->SetMarginTopLeft(wxPoint(5, 5));
    pageSetup->SetMarginBottomRight(wxPoint(5, 5));

    auto EscapeHtml = [](const wxString &text) {
        wxString s(text);
        s.Replace("&", "&amp;");
        s.Replace("<", "&lt;");
        s.Replace(">", "&gt;");
        s.Replace("\"", "&quot;");
        return s;
    };

    wxString html;
    html << "<html><body style=\"margin:5px;\">";
    html << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"2\" style=\"border-collapse:collapse;\">";

    html << "<tr bgcolor=\"#C8C8C8\">";
    for (int c : selCols)
        html << "<th style=\"white-space:nowrap;\">" << EscapeHtml(table->GetColumn(c)->GetTitle()) << "</th>";
    html << "</tr>";

    for (unsigned int r = 0; r < table->GetItemCount(); ++r) {
        html << wxString::Format("<tr bgcolor=\"%s\">", (r % 2 == 0) ? "#F2F2F2" : "#FFFFFF");
        for (int c : selCols) {
            wxVariant val;
            table->GetValue(val, r, c);
            html << "<td style=\"white-space:nowrap;\">" << EscapeHtml(val.GetString()) << "</td>";
        }
        html << "</tr>";
    }

    html << "</table></body></html>";

    printer.PreviewText(html);
}

void ExportCSV(wxWindow* parent, wxDataViewListCtrl* table, TableType type)
{
    if (!table)
        return;

    std::vector<std::string> cols;
    for (unsigned int i = 0; i < table->GetColumnCount(); ++i)
        cols.push_back(std::string(table->GetColumn(i)->GetTitle().ToUTF8()));

    std::vector<std::string> saved;
    switch (type)
    {
    case TableType::Fixtures: saved = ConfigManager::Get().GetFixturePrintColumns(); break;
    case TableType::Trusses: saved = ConfigManager::Get().GetTrussPrintColumns(); break;
    case TableType::SceneObjects: saved = ConfigManager::Get().GetSceneObjectPrintColumns(); break;
    }

    std::vector<int> defaultIdx;
    for (const auto& name : saved)
    {
        auto it = std::find(cols.begin(), cols.end(), name);
        if (it != cols.end())
            defaultIdx.push_back(static_cast<int>(std::distance(cols.begin(), it)));
    }

    ColumnSelectionDialog dlg(parent, cols, defaultIdx);
    if (dlg.ShowModal() != wxID_OK)
        return;

    std::vector<int> selCols = dlg.GetSelectedColumns();
    if (selCols.empty())
        return;

    std::vector<std::string> toSave;
    for (int c : selCols)
        toSave.push_back(cols[c]);

    switch (type)
    {
    case TableType::Fixtures: ConfigManager::Get().SetFixturePrintColumns(toSave); break;
    case TableType::Trusses: ConfigManager::Get().SetTrussPrintColumns(toSave); break;
    case TableType::SceneObjects: ConfigManager::Get().SetSceneObjectPrintColumns(toSave); break;
    }

    wxFileDialog saveDlg(parent, "Export CSV", "", "", "CSV files (*.csv)|*.csv|All files (*.*)|*.*",
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDlg.ShowModal() != wxID_OK)
        return;

    std::ofstream file(saveDlg.GetPath().ToUTF8().data());
    if (!file)
    {
        wxMessageBox("Failed to save file.", "Export CSV", wxOK | wxICON_ERROR, parent);
        return;
    }

    auto EscapeCSV = [](const wxString& text)
    {
        wxString s(text);
        s.Replace("\"", "\"\"");
        if (s.Contains(",") || s.Contains("\"") || s.Contains("\n") || s.Contains("\r"))
            s = "\"" + s + "\"";
        return std::string(s.ToUTF8());
    };

    for (size_t i = 0; i < selCols.size(); ++i)
    {
        if (i)
            file << ",";
        file << EscapeCSV(table->GetColumn(selCols[i])->GetTitle());
    }
    file << "\n";

    for (unsigned int r = 0; r < table->GetItemCount(); ++r)
    {
        for (size_t i = 0; i < selCols.size(); ++i)
        {
            if (i)
                file << ",";
            wxVariant val;
            table->GetValue(val, r, selCols[i]);
            file << EscapeCSV(val.GetString());
        }
        file << "\n";
    }

    file.close();
    wxMessageBox("CSV exported successfully.", "Export CSV", wxOK | wxICON_INFORMATION, parent);
}

} // namespace TablePrinter

