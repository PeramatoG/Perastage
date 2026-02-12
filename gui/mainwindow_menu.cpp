/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#include "mainwindow.h"
#include "mainwindow/controllers/mainwindow_view_controller.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <wx/aboutdlg.h>
#include <wx/artprov.h>
#include <wx/choice.h>
#include <wx/filename.h>
#include <wx/filefn.h>
#include <wx/html/htmlwin.h>
#include <wx/iconbndl.h>
#include <wx/numdlg.h>
#include <wx/stdpaths.h>

#include "addfixturedialog.h"
#include "autopatcher.h"
#include "configmanager.h"
#include "consolepanel.h"
#include "credentialstore.h"
#include "dictionaryeditdialog.h"
#include "fixture.h"
#include "fixturetablepanel.h"
#include "gdtfloader.h"
#include "gdtfnet.h"
#include "gdtfsearchdialog.h"
#include "hoisttablepanel.h"
#include "layerpanel.h"
#include "logindialog.h"
#include "markdown.h"
#include "preferencesdialog.h"
#include "projectutils.h"
#include "riggingpanel.h"
#include "sceneobjecttablepanel.h"
#include "selectfixturetypedialog.h"
#include "selectnamedialog.h"
#include "simplecrypt.h"
#include "support.h"
#include "trussloader.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer3dpanel.h"

namespace {
struct HelpMarkdown {
  std::string english;
  std::string spanish;
  bool hasSections = false;
};

std::string TrimLeadingWhitespace(const std::string &text) {
  const auto start = text.find_first_not_of("\r\n");
  if (start == std::string::npos)
    return std::string();
  return text.substr(start);
}

HelpMarkdown SplitHelpMarkdown(const std::string &markdown) {
  constexpr const char *kEnglishMarker = "<!-- LANG:en -->";
  constexpr const char *kSpanishMarker = "<!-- LANG:es -->";
  HelpMarkdown result;

  const auto enPos = markdown.find(kEnglishMarker);
  const auto esPos = markdown.find(kSpanishMarker);
  if (enPos == std::string::npos && esPos == std::string::npos) {
    result.english = markdown;
    result.spanish = markdown;
    return result;
  }

  result.hasSections = true;
  auto extract = [&](size_t start, size_t end, const char *marker) {
    if (start == std::string::npos)
      return std::string();
    start += std::strlen(marker);
    if (end == std::string::npos || end < start)
      end = markdown.size();
    return TrimLeadingWhitespace(markdown.substr(start, end - start));
  };

  if (enPos != std::string::npos && esPos != std::string::npos) {
    if (enPos < esPos) {
      result.english = extract(enPos, esPos, kEnglishMarker);
      result.spanish = extract(esPos, std::string::npos, kSpanishMarker);
    } else {
      result.spanish = extract(esPos, enPos, kSpanishMarker);
      result.english = extract(enPos, std::string::npos, kEnglishMarker);
    }
  } else {
    result.english =
        extract(enPos, std::string::npos, kEnglishMarker);
    result.spanish =
        extract(esPos, std::string::npos, kSpanishMarker);
  }

  if (result.english.empty())
    result.english = markdown;
  if (result.spanish.empty())
    result.spanish = markdown;

  return result;
}

std::string WrapHelpHtml(const std::string &body) {
  return "<html><head><meta charset=\"UTF-8\"></head><body>" + body +
         "</body></html>";
}
} // namespace

void MainWindow::CreateToolBars() {
  const long toolbarStyle =
      (wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORIZONTAL);
  fileToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, toolbarStyle);
  fileToolBar->SetToolBitmapSize(wxSize(16, 16));

  const auto loadToolbarIcon = [](const std::string &name,
                                  const wxArtID &fallbackArtId) {
    auto svgPath = ProjectUtils::GetResourceRoot() / "icons" / "outline" /
                   (name + ".svg");
    if (std::filesystem::exists(svgPath)) {
      wxBitmapBundle bundle =
          wxBitmapBundle::FromSVGFile(svgPath.string(), wxSize(16, 16));
      if (bundle.IsOk()) {
        return bundle;
      }
    }
    return wxArtProvider::GetBitmapBundle(fallbackArtId, wxART_TOOLBAR,
                                          wxSize(16, 16));
  };
  const auto loadToolbarDisabledIcon = [&](const std::string &name,
                                           const wxArtID &fallbackArtId) {
    auto svgPath = ProjectUtils::GetResourceRoot() / "icons" / "outline" /
                   (name + "-disabled.svg");
    if (std::filesystem::exists(svgPath)) {
      wxBitmapBundle bundle =
          wxBitmapBundle::FromSVGFile(svgPath.string(), wxSize(16, 16));
      if (bundle.IsOk())
        return bundle;
    }
    return loadToolbarIcon(name, fallbackArtId);
  };
  const auto addToolWithDisabledIcon =
      [&](wxAuiToolBar *toolbar, int id, const wxString &label,
          const std::string &iconName, const wxArtID &fallbackArtId,
          const wxString &shortHelp) {
        toolbar->AddTool(id, label, loadToolbarIcon(iconName, fallbackArtId),
                         shortHelp);

        wxAuiToolBarItem *toolItem = toolbar->FindTool(id);
        if (toolItem) {
          toolItem->SetDisabledBitmap(
              loadToolbarDisabledIcon(iconName, fallbackArtId)
                  .GetBitmap(wxSize(16, 16)));
        }
      };
  fileToolBar->AddTool(ID_File_New, "New",
                       loadToolbarIcon("file", wxART_NEW),
                       "Create a new project");
  fileToolBar->AddTool(ID_File_Load, "Open",
                       loadToolbarIcon("folder-open", wxART_FILE_OPEN),
                       "Open an existing project");
  fileToolBar->AddTool(ID_File_Save, "Save",
                       loadToolbarIcon("save", wxART_FILE_SAVE),
                       "Save the current project");
  fileToolBar->AddTool(ID_File_SaveAs, "Save As",
                       loadToolbarIcon("save-all", wxART_FILE_SAVE),
                       "Save the current project with a new name");
  fileToolBar->AddTool(ID_File_ImportMVR, "Import MVR",
                       loadToolbarIcon("file-input", wxART_FILE_OPEN),
                       "Import an MVR file");
  fileToolBar->AddTool(ID_File_ExportMVR, "Export MVR",
                       loadToolbarIcon("file-output", wxART_FILE_SAVE),
                       "Export the project to MVR");
  fileToolBar->AddTool(ID_File_PrintMenu, "Print",
                       loadToolbarIcon("printer", wxART_PRINT),
                       "Choose what to print");
  fileToolBar->Realize();

  auiManager->AddPane(
      fileToolBar, wxAuiPaneInfo()
                       .Name("FileToolbar")
                       .Caption("File")
                       .ToolbarPane()
                       .Top());

  editToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition,
                                 wxDefaultSize, toolbarStyle);
  editToolBar->SetToolBitmapSize(wxSize(16, 16));
  editToolBar->AddTool(ID_Edit_Undo, "Undo",
                       loadToolbarIcon("undo-2", wxART_UNDO),
                       "Undo last action");
  editToolBar->AddTool(ID_Edit_Redo, "Redo",
                       loadToolbarIcon("redo-2", wxART_REDO),
                       "Redo last undone action");
  editToolBar->Realize();
  auiManager->AddPane(
      editToolBar, wxAuiPaneInfo()
                       .Name("EditToolbar")
                       .Caption("Edit")
                       .ToolbarPane()
                       .Top());

  layoutViewsToolBar =
      new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       toolbarStyle);
  layoutViewsToolBar->SetToolBitmapSize(wxSize(16, 16));
  layoutViewsToolBar->AddTool(ID_View_Layout_Default, "Vista layout 3D",
                              loadToolbarIcon("box",
                                              wxART_MISSING_IMAGE),
                              "Switch to 3D Layout View");
  layoutViewsToolBar->AddTool(ID_View_Layout_2D, "Vista layout 2D",
                              loadToolbarIcon("panels-right-bottom",
                                              wxART_MISSING_IMAGE),
                              "Switch to 2D Layout View");
  layoutViewsToolBar->AddTool(ID_View_Layout_Mode, "Modo layout",
                              loadToolbarIcon("square-asterisk",
                                              wxART_MISSING_IMAGE),
                              "Switch to Layout Mode View");
  layoutViewsToolBar->Realize();
  auiManager->AddPane(
      layoutViewsToolBar, wxAuiPaneInfo()
                              .Name("LayoutViewsToolbar")
                              .Caption("Layout Views")
                              .ToolbarPane()
                              .Top());

  toolsToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition,
                                  wxDefaultSize, toolbarStyle);
  toolsToolBar->SetToolBitmapSize(wxSize(16, 16));
  addToolWithDisabledIcon(toolsToolBar, ID_Edit_AddFixture, "Add Fixture",
                          "spotlight", wxART_MISSING_IMAGE, "Add fixture");
  addToolWithDisabledIcon(toolsToolBar, ID_Edit_AddTruss, "Add Truss",
                          "truss", wxART_MISSING_IMAGE, "Add truss");
  addToolWithDisabledIcon(toolsToolBar, ID_Edit_AddSceneObject, "Add Object",
                          "guitar", wxART_MISSING_IMAGE, "Add object");
  toolsToolBar->AddSeparator();
  toolsToolBar->AddTool(ID_Tools_DownloadGdtf, "Download GDTF",
                        loadToolbarIcon("cloud-download", wxART_MISSING_IMAGE),
                        "Download GDTF");
  toolsToolBar->AddTool(ID_Tools_ImportRiderText, "Create by text",
                        loadToolbarIcon("notepad-text", wxART_TIP),
                        "Create by text");
  toolsToolBar->Realize();
  auiManager->AddPane(
      toolsToolBar, wxAuiPaneInfo()
                        .Name("ToolsToolbar")
                        .Caption("Tools")
                        .ToolbarPane()
                        .Top());

  layoutToolBar = new wxAuiToolBar(this, wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, toolbarStyle);
  layoutToolBar->SetToolBitmapSize(wxSize(16, 16));
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_2DView,
                          "Añadir vista 2D", "panel-top-bottom-dashed",
                          wxART_MISSING_IMAGE, "Add 2D View to Layout");
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_Legend,
                          "Añadir leyenda", "layout-list",
                          wxART_MISSING_IMAGE, "Add fixture legend to layout");
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_EventTable,
                          "Añadir tabla de evento", "table", wxART_LIST_VIEW,
                          "Add event table to layout");
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_Text, "Añadir texto",
                          "text-select", wxART_TIP,
                          "Add text box to layout");
  addToolWithDisabledIcon(layoutToolBar, ID_View_Layout_Image,
                          "Añadir imagen", "image-plus",
                          wxART_MISSING_IMAGE, "Add image to layout");
  layoutToolBar->Realize();
  auiManager->AddPane(
      layoutToolBar, wxAuiPaneInfo()
                         .Name("LayoutToolbar")
                         .Caption("Layout")
                         .ToolbarPane()
                         .Top());

  UpdateToolBarAvailability();
}

void MainWindow::CreateMenuBar() {
  wxMenuBar *menuBar = new wxMenuBar();

  // File menu
  wxMenu *fileMenu = new wxMenu();
  fileMenu->Append(ID_File_New, "New\tCtrl+N");
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_Load, "Load\tCtrl+L");
  fileMenu->Append(ID_File_Save, "Save\tCtrl+S");
  fileMenu->Append(ID_File_SaveAs, "Save As...");
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_ImportMVR, "Import MVR...");
  fileMenu->Append(ID_File_ExportMVR, "Export MVR...");
  fileMenu->Append(ID_File_PrintViewer2D, "Print Viewer 2D...");
  fileMenu->Append(ID_File_PrintLayout, "Print Layout...");
  fileMenu->Append(ID_File_PrintTable, "Print Table...");
  fileMenu->Append(ID_File_ExportCSV, "Export CSV...");
  fileMenu->AppendSeparator();
  fileMenu->Append(ID_File_Close, "Close\tCtrl+Q");

  menuBar->Append(fileMenu, "&File");

  // Edit menu
  wxMenu *editMenu = new wxMenu();
  editMenu->Append(ID_Edit_Undo, "Undo\tCtrl+Z");
  editMenu->Append(ID_Edit_Redo, "Redo\tCtrl+Y");
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_AddFixture, "Add fixture...");
  editMenu->Append(ID_Edit_AddTruss, "Add truss...");
  editMenu->Append(ID_Edit_AddSceneObject, "Add scene object...");
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_Delete, "Delete\tDel");
  editMenu->AppendSeparator();
  editMenu->Append(ID_Edit_Preferences, "Preferences...");

  menuBar->Append(editMenu, "&Edit");

  // View menu for toggling panels
  wxMenu *viewMenu = new wxMenu();
  viewMenu->AppendCheckItem(ID_View_ToggleConsole, "Console");
  viewMenu->AppendCheckItem(ID_View_ToggleFixtures, "Fixtures");
  viewMenu->AppendCheckItem(ID_View_ToggleViewport, "3D Viewport");
  viewMenu->AppendCheckItem(ID_View_ToggleViewport2D, "2D Viewport");
  viewMenu->AppendCheckItem(ID_View_ToggleRender2D, "2D Render Options");
  viewMenu->AppendCheckItem(ID_View_ToggleLayers, "Layers");
  viewMenu->AppendCheckItem(ID_View_ToggleLayouts, "Layouts");
  viewMenu->AppendCheckItem(ID_View_ToggleSummary, "Summary");
  viewMenu->AppendCheckItem(ID_View_ToggleRigging, "Rigging");

  wxMenu *layoutMenu = new wxMenu();
  layoutMenu->Append(ID_View_Layout_Default, "3D Layout View");
  layoutMenu->Append(ID_View_Layout_2D, "2D Layout View");
  layoutMenu->Append(ID_View_Layout_Mode, "Layout Mode View");
  viewMenu->AppendSubMenu(layoutMenu, "Layout Views");

  menuBar->Append(viewMenu, "&View");

  // Tools menu
  wxMenu *toolsMenu = new wxMenu();
  toolsMenu->Append(ID_Tools_DownloadGdtf, "Download GDTF fixture...");
  toolsMenu->Append(ID_Tools_EditDictionaries, "Edit dictionaries...");
  toolsMenu->Append(ID_Tools_ImportRiderText, "Create from text...");
  toolsMenu->Append(ID_Tools_ExportFixture, "Export Fixture...");
  toolsMenu->Append(ID_Tools_ExportTruss, "Export Truss...");
  toolsMenu->Append(ID_Tools_ExportSceneObject, "Export Scene Object...");
  toolsMenu->Append(ID_Tools_AutoPatch, "Auto patch");
  toolsMenu->Append(ID_Tools_AutoColor, "Auto color");
  toolsMenu->Append(ID_Tools_ConvertToHoist, "Convert to Hoist");

  menuBar->Append(toolsMenu, "&Tools");

  // Help menu
  wxMenu *helpMenu = new wxMenu();
  helpMenu->Append(ID_Help_Help, "Help\tF1");
  helpMenu->Append(ID_Help_About, "About");

  menuBar->Append(helpMenu, "&Help");

  SetMenuBar(menuBar);
}

void MainWindow::OnNew(wxCommandEvent &WXUNUSED(event)) {
  if (!ConfirmSaveIfDirty("creating a new project", "New Project"))
    return;

  ResetProject();
}

void MainWindow::OnDownloadGdtf(wxCommandEvent &WXUNUSED(event)) {
  // Flow overview: reuse stored credentials to reduce friction, authenticate,
  // and persist the session before requesting the list and downloading.
  // The order matters because the list needs a valid cookie; we also save
  // credentials early so a later network failure doesn't discard user input.
  std::string savedUser;
  std::string savedPass;
  if (auto creds = CredentialStore::Load()) {
    savedUser = creds->username;
    savedPass = creds->password;
  } else {
    std::string savedPassEnc =
        ConfigManager::Get().GetValue("gdtf_password").value_or("");
    savedUser = ConfigManager::Get().GetValue("gdtf_username").value_or("");
    savedPass = SimpleCrypt::Decode(savedPassEnc);
  }

  GdtfLoginDialog loginDlg(this, savedUser, savedPass);
  if (loginDlg.ShowModal() != wxID_OK)
    return;
  wxString username =
      wxString::FromUTF8(loginDlg.GetUsername()).Trim(true).Trim(false);
  wxString password = wxString::FromUTF8(loginDlg.GetPassword());
  ConfigManager::Get().SetValue("gdtf_username",
                                std::string(username.mb_str()));
  ConfigManager::Get().SetValue(
      "gdtf_password", SimpleCrypt::Encode(std::string(password.mb_str())));
  CredentialStore::Save(
      {std::string(username.mb_str()), std::string(password.mb_str())});

  if (!currentProjectPath.empty())
    ConfigManager::Get().SaveProject(currentProjectPath);

  wxString cookieFileWx = wxFileName::GetTempDir() + "/gdtf_session.txt";
  std::string cookieFile = std::string(cookieFileWx.mb_str());
  long httpCode = 0;
  if (consolePanel)
    consolePanel->AppendMessage("Logging into GDTF Share using libcurl");
  if (!GdtfLogin(std::string(username.mb_str()), std::string(password.mb_str()),
                 cookieFile, httpCode)) {
    wxMessageBox("Failed to connect to GDTF Share.", "Login Error",
                 wxOK | wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("Login connection failed");
    return;
  }
  if (consolePanel)
    consolePanel->AppendMessage(
        wxString::Format("Login HTTP code: %ld", httpCode));
  if (httpCode != 200) {
    wxMessageBox("Login failed.", "Login Error", wxOK | wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("Login failed with code " +
                                  wxString::Format("%ld", httpCode));
    return;
  }

  if (consolePanel)
    consolePanel->AppendMessage("Retrieving fixture list via libcurl");
  std::string listData;
  if (!GdtfGetList(cookieFile, listData)) {
    wxMessageBox("Failed to retrieve fixture list.", "Error",
                 wxOK | wxICON_ERROR);
    return;
  }

  if (consolePanel)
    consolePanel->AppendMessage(
        wxString::Format("Retrieved list size: %zu bytes", listData.size()));

  ConfigManager::Get().SetValue("gdtf_fixture_list", listData);

  // open search dialog
  GdtfSearchDialog searchDlg(this, listData);
  if (searchDlg.ShowModal() == wxID_OK) {
    wxString rid = wxString::FromUTF8(searchDlg.GetSelectedId());
    wxString name = wxString::FromUTF8(searchDlg.GetSelectedName());

    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
    wxFileDialog saveDlg(this, "Save GDTF file", fixDir, name + ".gdtf",
                         "*.gdtf", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDlg.ShowModal() == wxID_OK) {
      wxString dest = saveDlg.GetPath();
      if (!rid.empty()) {
        if (consolePanel)
          consolePanel->AppendMessage("Downloading via libcurl rid=" + rid);
        long dlCode = 0;
        bool ok = GdtfDownload(std::string(rid.mb_str()),
                               std::string(dest.mb_str()), cookieFile, dlCode);
        if (consolePanel)
          consolePanel->AppendMessage(
              wxString::Format("Download HTTP code: %ld", dlCode));
        if (ok && dlCode == 200)
          wxMessageBox("GDTF downloaded.", "Success",
                       wxOK | wxICON_INFORMATION);
        else
          wxMessageBox("Failed to download GDTF.", "Error",
                       wxOK | wxICON_ERROR);
      } else {
        wxMessageBox("Download information missing.", "Error",
                     wxOK | wxICON_ERROR);
      }
    }
  }

  wxRemoveFile(cookieFileWx);
}

void MainWindow::OnEditDictionaries(wxCommandEvent &WXUNUSED(event)) {
  DictionaryEditDialog dlg(this);
  dlg.ShowModal();
}

void MainWindow::OnAutoPatch(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.PushUndoState("auto patch");
  AutoPatcher::AutoPatch(cfg.GetScene());
  RefreshAfterSceneChange();
}

void MainWindow::OnAutoColor(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  cfg.PushUndoState("auto color");
  auto &scene = cfg.GetScene();
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 255);
  auto randHex = [&]() {
    return wxString::Format("#%02X%02X%02X", dist(rng), dist(rng), dist(rng))
        .ToStdString();
  };
  auto isWhiteColor = [](const std::string &color) {
    if (color.empty())
      return false;
    std::string normalized = color;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (normalized.rfind('#', 0) == 0)
      normalized.erase(0, 1);
    return normalized == "fff" || normalized == "ffffff" ||
           normalized == "white";
  };
  const std::string trussColor = "#D3D3D3";

  std::set<std::string> layerNames;
  for (const auto &[uuid, layer] : scene.layers)
    layerNames.insert(layer.name);
  for (const auto &[u, f] : scene.fixtures)
    layerNames.insert(f.layer);
  for (const auto &[u, t] : scene.trusses)
    layerNames.insert(t.layer);
  for (const auto &[u, o] : scene.sceneObjects)
    layerNames.insert(o.layer);
  layerNames.insert(DEFAULT_LAYER_NAME);

  for (const auto &name : layerNames) {
    auto current = cfg.GetLayerColor(name);
    if (!current || current->empty()) {
      std::string lower = name;
      std::transform(lower.begin(), lower.end(), lower.begin(),
                     [](unsigned char c) { return std::tolower(c); });
      std::string c = lower.rfind("truss", 0) == 0 ? trussColor : randHex();
      cfg.SetLayerColor(name, c);
      if (Viewer3DPanel::Instance())
        Viewer3DPanel::Instance()->SetLayerColor(name, c);
    }
  }

  std::map<std::string, std::string> typeColors;
  for (auto &[uuid, f] : scene.fixtures) {
    if (!f.gdtfSpec.empty()) {
      auto it = typeColors.find(f.gdtfSpec);
      if (it == typeColors.end()) {
        std::string c =
            (f.color.empty() || isWhiteColor(f.color)) ? randHex() : f.color;
        typeColors[f.gdtfSpec] = c;
        f.color = c;
      } else {
        f.color = it->second;
      }
    } else if (f.color.empty()) {
      f.color = randHex();
    }
  }
  for (const auto &[spec, color] : typeColors) {
    std::string gdtfPath = spec;
    std::filesystem::path p = gdtfPath;
    if (p.is_relative() && !scene.basePath.empty())
      gdtfPath = (std::filesystem::path(scene.basePath) / p).string();
    SetGdtfModelColor(gdtfPath, color);
  }

  if (layerPanel)
    layerPanel->ReloadLayers();
  RefreshAfterSceneChange();
}

void MainWindow::OnConvertToHoist(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  const auto selected = cfg.GetSelectedFixtures();
  if (selected.empty()) {
    wxMessageBox("Please select fixtures to convert first.", "Convert to Hoist",
                 wxOK | wxICON_INFORMATION);
    return;
  }

  cfg.PushUndoState("convert fixtures to hoists");
  auto &scene = cfg.GetScene();

  auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  int idx = 0;
  std::vector<std::string> newIds;
  for (const auto &uuid : selected) {
    auto it = scene.fixtures.find(uuid);
    if (it == scene.fixtures.end())
      continue;

    const auto &fixture = it->second;

    Support s;
    s.uuid = wxString::Format("uuid_%lld_%d", static_cast<long long>(baseId),
                             idx++)
                .ToStdString();
    s.name = fixture.instanceName;
    s.gdtfSpec = fixture.gdtfSpec;
    s.gdtfMode = fixture.gdtfMode;
    s.function = fixture.function.empty() ? "Hoist" : fixture.function;
    s.chainLength = 0.0f;
    s.position = fixture.position;
    s.positionName = fixture.positionName;
    s.layer = fixture.layer;
    s.capacityKg = 0.0f;
    s.weightKg = fixture.weightKg;
    s.hoistFunction = NormalizeHoistFunction(s.function);
    s.transform = fixture.transform;

    scene.supports[s.uuid] = s;
    newIds.push_back(s.uuid);
  }

  for (const auto &uuid : selected)
    scene.fixtures.erase(uuid);

  cfg.SetSelectedSupports(newIds);
  cfg.SetSelectedFixtures({});

  if (fixturePanel)
    fixturePanel->ReloadData();
  if (hoistPanel)
    hoistPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
  RefreshRigging();

  wxMessageBox(wxString::Format("Converted %zu fixture(s) to hoists.",
                                newIds.size()),
               "Convert to Hoist", wxOK | wxICON_INFORMATION);
}

void MainWindow::OnClose(wxCommandEvent &event) {
  // Allow the close event to be vetoed when the user chooses Cancel
  Close(false);
}

void MainWindow::OnCloseWindow(wxCloseEvent &event) {
  SaveUserConfigWithViewport2DState();
  if (!ConfirmSaveIfDirty("exiting", "Exit")) {
    event.Veto();
    return;
  }

  if (viewportPanel)
    viewportPanel->StopRefreshThread();

  Destroy();
}

void MainWindow::OnToggleConsole(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleConsole(event);
}

void MainWindow::OnToggleFixtures(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleFixtures(event);
}

void MainWindow::OnToggleViewport(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleViewport(event);
}

void MainWindow::OnToggleViewport2D(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleViewport2D(event);
}

void MainWindow::OnToggleRender2D(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleRender2D(event);
}

void MainWindow::OnToggleLayers(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleLayers(event);
}

void MainWindow::OnToggleLayouts(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleLayouts(event);
}

void MainWindow::OnToggleSummary(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleSummary(event);
}

void MainWindow::OnToggleRigging(wxCommandEvent &event) {
  if (viewController)
    viewController->OnToggleRigging(event);
}

void MainWindow::OnShowHelp(wxCommandEvent &WXUNUSED(event)) {
  // Attempt to load the Markdown help file located alongside the executable.
  wxFileName helpPath(wxStandardPaths::Get().GetExecutablePath());
  helpPath.SetFullName("help.md");
  if (helpPath.Exists()) {
    // Read the file contents.
    std::ifstream in(helpPath.GetFullPath().ToStdString());
    std::string markdown((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    HelpMarkdown help = SplitHelpMarkdown(markdown);

    // Create a resizable dialog containing a wxHtmlWindow to render the
    // generated HTML.
    const wxSize parentSize = GetSize();
    const wxSize dialogSize(
        std::max(900, static_cast<int>(parentSize.x * 0.85)),
        std::max(700, static_cast<int>(parentSize.y * 0.85)));
    wxDialog dlg(this, wxID_ANY, "Perastage Help", wxDefaultPosition,
                 dialogSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *langSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText *langLabel =
        new wxStaticText(&dlg, wxID_ANY, "Language:");
    wxChoice *langChoice = new wxChoice(&dlg, wxID_ANY);
    langChoice->Append("English");
    langChoice->Append("Español");
    langChoice->SetSelection(0);
    langSizer->Add(langLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    langSizer->Add(langChoice, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(langSizer, 0, wxLEFT | wxRIGHT | wxTOP, 8);
    wxHtmlWindow *htmlWin = new wxHtmlWindow(
        &dlg, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHW_SCROLLBAR_AUTO);

    auto setHelpPage = [&](const std::string &markdownBody) {
      std::string html = MarkdownToHtml(markdownBody);
      std::string wrapped = WrapHelpHtml(html);
      htmlWin->SetPage(wxString::FromUTF8(wrapped));
    };

    setHelpPage(help.english);
    langChoice->Bind(wxEVT_CHOICE, [&](wxCommandEvent &) {
      if (langChoice->GetSelection() == 1)
        setHelpPage(help.spanish);
      else
        setHelpPage(help.english);
    });

    sizer->Add(htmlWin, 1, wxEXPAND | wxALL, 5);
    dlg.SetSizer(sizer);
    dlg.ShowModal();
  } else {
    wxMessageBox("help.md file not found", "Perastage Help",
                 wxOK | wxICON_ERROR, this);
  }
}

void MainWindow::OnShowAbout(wxCommandEvent &WXUNUSED(event)) {
  wxAboutDialogInfo info;
  info.SetName("Perastage");
  info.SetVersion("1.0");
  wxString description =
      "High-performance MVR scene viewer with 3D rendering support.\n\n"
      "This application makes use of the following open-source libraries:\n"
      "  - wxWidgets\n"
      "  - tinyxml2\n"
      "  - nlohmann-json\n"
      "  - OpenGL (or Vulkan backend)";
  info.SetDescription(description);
  info.SetWebSite("https://luismaperamato.com");
  info.AddDeveloper("Luisma Peramato");
  info.SetLicence(
      "This software is licensed under the GNU General Public License v3.0.");

  // Load the largest available icon
  wxIconBundle bundle;
  const wxString iconPaths[] = {"resources/Perastage.ico",
                                "../resources/Perastage.ico",
                                "../../resources/Perastage.ico"};
  for (const wxString &path : iconPaths) {
    if (wxFileExists(path))
      bundle.AddIcon(path, wxBITMAP_TYPE_ICO);
  }
  wxIcon icon = bundle.GetIcon(wxSize(256, 256));
  if (icon.IsOk())
    info.SetIcon(icon);

  wxAboutBox(info, this);
}

void MainWindow::OnSelectFixtures(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->SetSelection(0);
}

void MainWindow::OnSelectTrusses(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->SetSelection(1);
}

void MainWindow::OnSelectSupports(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->SetSelection(2);
}

void MainWindow::OnSelectObjects(wxCommandEvent &WXUNUSED(event)) {
  if (notebook)
    notebook->SetSelection(3);
}

void MainWindow::OnPreferences(wxCommandEvent &WXUNUSED(event)) {
  PreferencesDialog dlg(this);
  if (dlg.ShowModal() == wxID_OK) {
    ConfigManager::Get().SaveUserConfig();
  }
}

void MainWindow::OnUndo(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  if (!cfg.CanUndo())
    return;
  std::string action = cfg.Undo();
  if (consolePanel)
    consolePanel->AppendMessage(action.empty() ? "Undo" : "Undo " + action);
  if (fixturePanel) {
    fixturePanel->ReloadData();
    fixturePanel->SelectByUuid(cfg.GetSelectedFixtures());
  }
  if (trussPanel) {
    trussPanel->ReloadData();
    trussPanel->SelectByUuid(cfg.GetSelectedTrusses());
  }
  if (hoistPanel) {
    hoistPanel->ReloadData();
    hoistPanel->SelectByUuid(cfg.GetSelectedSupports());
  }
  if (sceneObjPanel) {
    sceneObjPanel->ReloadData();
    sceneObjPanel->SelectByUuid(cfg.GetSelectedSceneObjects());
  }
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    if (fixturePanel && fixturePanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedFixtures());
    else if (trussPanel && trussPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedTrusses());
    else if (hoistPanel && hoistPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedSupports());
    else if (sceneObjPanel && sceneObjPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedSceneObjects());
    else
      viewportPanel->SetSelectedFixtures({});
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnRedo(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  if (!cfg.CanRedo())
    return;
  std::string action = cfg.Redo();
  if (consolePanel)
    consolePanel->AppendMessage(action.empty() ? "Redo" : "Redo " + action);
  if (fixturePanel) {
    fixturePanel->ReloadData();
    fixturePanel->SelectByUuid(cfg.GetSelectedFixtures());
  }
  if (trussPanel) {
    trussPanel->ReloadData();
    trussPanel->SelectByUuid(cfg.GetSelectedTrusses());
  }
  if (hoistPanel) {
    hoistPanel->ReloadData();
    hoistPanel->SelectByUuid(cfg.GetSelectedSupports());
  }
  if (sceneObjPanel) {
    sceneObjPanel->ReloadData();
    sceneObjPanel->SelectByUuid(cfg.GetSelectedSceneObjects());
  }
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    if (fixturePanel && fixturePanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedFixtures());
    else if (trussPanel && trussPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedTrusses());
    else if (hoistPanel && hoistPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedSupports());
    else if (sceneObjPanel && sceneObjPanel->IsActivePage())
      viewportPanel->SetSelectedFixtures(cfg.GetSelectedSceneObjects());
    else
      viewportPanel->SetSelectedFixtures({});
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnAddFixture(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();

  std::string gdtfPath;
  std::string defaultName;

  if (!scene.fixtures.empty()) {
    std::map<std::string, std::string> typeToSpec;
    for (const auto &[uuid, f] : scene.fixtures)
      if (!f.typeName.empty() && !f.gdtfSpec.empty())
        typeToSpec.try_emplace(f.typeName, f.gdtfSpec);
    std::vector<std::string> types;
    types.reserve(typeToSpec.size());
    for (const auto &[name, spec] : typeToSpec)
      types.push_back(name);

    SelectFixtureTypeDialog chooseDlg(this, types);
    int dlgRes = chooseDlg.ShowModal();
    if (dlgRes == wxID_CANCEL)
      return;
    if (dlgRes == wxID_OPEN) {
      wxString fixDir =
          wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
      wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString,
                        "*.gdtf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (fdlg.ShowModal() != wxID_OK)
        return;
      wxString gdtfPathWx = fdlg.GetPath();
      gdtfPath = std::string(gdtfPathWx.mb_str());
      defaultName = GetGdtfFixtureName(gdtfPath);
      if (defaultName.empty())
        defaultName = wxFileName(gdtfPathWx).GetName().ToStdString();
    } else {
      int sel = chooseDlg.GetSelection();
      if (sel < 0 || sel >= static_cast<int>(types.size()))
        return;
      defaultName = types[sel];
      std::string spec = typeToSpec[defaultName];
      namespace fs = std::filesystem;
      if (fs::path(spec).is_absolute())
        gdtfPath = spec;
      else
        gdtfPath = (fs::path(scene.basePath) / spec).string();
    }
  } else {
    wxString fixDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
    wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString, "*.gdtf",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxString gdtfPathWx = fdlg.GetPath();
    gdtfPath = std::string(gdtfPathWx.mb_str());
    defaultName = GetGdtfFixtureName(gdtfPath);
    if (defaultName.empty())
      defaultName = wxFileName(gdtfPathWx).GetName().ToStdString();
  }

  std::vector<std::string> modes = GetGdtfModes(gdtfPath);
  AddFixtureDialog dlg(this, wxString::FromUTF8(defaultName), modes);
  if (dlg.ShowModal() != wxID_OK)
    return;

  float weight = 0.0f, power = 0.0f;
  GetGdtfProperties(gdtfPath, weight, power);
  std::string defaultColor = GetGdtfModelColor(gdtfPath);

  int count = dlg.GetUnitCount();
  std::string name = dlg.GetFixtureName();
  int startId = dlg.GetFixtureId();
  std::string mode = dlg.GetMode();

  namespace fs = std::filesystem;
  cfg.PushUndoState("add fixture");
  auto &sceneRef = cfg.GetScene();
  std::string base = sceneRef.basePath;
  std::string spec = gdtfPath;
  if (!base.empty()) {
    fs::path abs = fs::absolute(gdtfPath);
    fs::path b = fs::absolute(base);
    if (abs.string().rfind(b.string(), 0) == 0)
      spec = fs::relative(abs, b).string();
  }

  auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  std::string layerName = cfg.GetCurrentLayer();
  bool hasLayer = false;
  for (const auto &[uid, layer] : sceneRef.layers) {
    if (layer.name == layerName) {
      hasLayer = true;
      break;
    }
  }
  if (!hasLayer) {
    Layer layer;
    layer.uuid = wxString::Format("layer_%lld", static_cast<long long>(baseId))
                     .ToStdString();
    layer.name = layerName;
    sceneRef.layers[layer.uuid] = layer;
  }

  int maxId = 0;
  for (const auto &[uuid, fix] : sceneRef.fixtures)
    if (fix.fixtureId > maxId)
      maxId = fix.fixtureId;
  if (startId <= 0)
    startId = maxId + 1;

  for (int i = 0; i < count; ++i) {
    Fixture f;
    f.uuid = wxString::Format("uuid_%lld_%d", static_cast<long long>(baseId), i)
                 .ToStdString();
    f.instanceName = name;
    f.typeName = defaultName;
    f.fixtureId = startId + i;
    f.gdtfSpec = spec;
    f.gdtfMode = mode;
    f.layer = layerName;
    f.weightKg = weight;
    f.powerConsumptionW = power;
    f.color = defaultColor;
    sceneRef.fixtures[f.uuid] = f;
  }

  if (fixturePanel)
    fixturePanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnAddTruss(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();

  std::string path;
  std::string defaultName;

  if (!scene.trusses.empty()) {
    std::map<std::string, std::string> nameToFile;
    for (const auto &[uuid, t] : scene.trusses)
      if (!t.name.empty() && !t.symbolFile.empty())
        nameToFile.try_emplace(t.name, t.symbolFile);
    std::vector<std::string> names;
    names.reserve(nameToFile.size());
    for (const auto &[n, _] : nameToFile)
      names.push_back(n);

    SelectNameDialog chooseDlg(this, names, "Select Truss", "Choose a truss:");
    int dlgRes = chooseDlg.ShowModal();
    if (dlgRes == wxID_CANCEL)
      return;
    if (dlgRes == wxID_OPEN) {
      wxString trussDir =
          wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("trusses"));
      wxFileDialog fdlg(this, "Select Truss file", trussDir, wxEmptyString,
                        "*.gtruss", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (fdlg.ShowModal() != wxID_OK)
        return;
      wxFileName fn(fdlg.GetPath());
      defaultName = fn.GetName().ToStdString();
      path = std::string(fdlg.GetPath().mb_str());
    } else {
      int sel = chooseDlg.GetSelection();
      if (sel < 0 || sel >= static_cast<int>(names.size()))
        return;
      defaultName = names[sel];
      path = nameToFile[defaultName];
    }
  } else {
    wxString trussDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("trusses"));
    wxFileDialog fdlg(this, "Select Truss file", trussDir, wxEmptyString,
                      "*.gtruss", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxFileName fn(fdlg.GetPath());
    defaultName = fn.GetName().ToStdString();
    path = std::string(fdlg.GetPath().mb_str());
  }

  Truss baseTruss;
  namespace fs = std::filesystem;
  if (fs::path(path).extension() == ".gtruss") {
    if (!LoadTrussArchive(path, baseTruss)) {
      wxMessageBox("Failed to read truss file.", "Error", wxOK | wxICON_ERROR);
      return;
    }
    if (!baseTruss.name.empty())
      defaultName = baseTruss.name;
  } else {
    baseTruss.symbolFile = path;
    baseTruss.modelFile = path;
  }

  long qty = wxGetNumberFromUser("Enter truss quantity:", wxEmptyString,
                                 "Add Truss", 1, 1, 1000, this);
  if (qty <= 0)
    return;

  cfg.PushUndoState("add truss");
  std::string base = scene.basePath;
  std::string modelPath = baseTruss.symbolFile;
  if (!base.empty()) {
    fs::path abs = fs::absolute(modelPath);
    fs::path b = fs::absolute(base);
    if (abs.string().rfind(b.string(), 0) == 0)
      modelPath = fs::relative(abs, b).string();
  }
  baseTruss.symbolFile = modelPath;
  if (!baseTruss.modelFile.empty()) {
    std::string archiveRel = baseTruss.modelFile;
    if (!base.empty()) {
      fs::path absM = fs::absolute(archiveRel);
      fs::path b = fs::absolute(base);
      if (absM.string().rfind(b.string(), 0) == 0)
        archiveRel = fs::relative(absM, b).string();
    }
    baseTruss.modelFile = archiveRel;
  }

  auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  std::string layerName = cfg.GetCurrentLayer();
  bool hasLayer = false;
  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == layerName) {
      hasLayer = true;
      break;
    }
  }
  if (!hasLayer) {
    Layer layer;
    layer.uuid = wxString::Format("layer_%lld", static_cast<long long>(baseId))
                     .ToStdString();
    layer.name = layerName;
    scene.layers[layer.uuid] = layer;
  }

  for (long i = 0; i < qty; ++i) {
    Truss t = baseTruss;
    t.uuid = wxString::Format("uuid_%lld", static_cast<long long>(baseId + i))
                 .ToStdString();
    if (qty > 1)
      t.name = defaultName + " " + std::to_string(i + 1);
    else
      t.name = defaultName;
    t.layer = layerName;
    scene.trusses[t.uuid] = t;
  }

  if (trussPanel)
    trussPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnAddSceneObject(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = ConfigManager::Get();
  auto &scene = cfg.GetScene();

  std::string path;
  std::string defaultName;

  if (!scene.sceneObjects.empty()) {
    std::map<std::string, std::string> nameToFile;
    for (const auto &[uuid, o] : scene.sceneObjects)
      if (!o.name.empty() && !o.modelFile.empty())
        nameToFile.try_emplace(o.name, o.modelFile);
    std::vector<std::string> names;
    names.reserve(nameToFile.size());
    for (const auto &[n, _] : nameToFile)
      names.push_back(n);

    SelectNameDialog chooseDlg(this, names, "Select Scene Object",
                               "Choose an object:");
    int dlgRes = chooseDlg.ShowModal();
    if (dlgRes == wxID_CANCEL)
      return;
    if (dlgRes == wxID_OPEN) {
      wxString objDir = wxString::FromUTF8(
          ProjectUtils::GetDefaultLibraryPath("scene objects"));
      wxFileDialog fdlg(this, "Select Object file", objDir, wxEmptyString,
                        "*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
      if (fdlg.ShowModal() != wxID_OK)
        return;
      wxFileName fn(fdlg.GetPath());
      defaultName = fn.GetName().ToStdString();
      path = std::string(fdlg.GetPath().mb_str());
    } else {
      int sel = chooseDlg.GetSelection();
      if (sel < 0 || sel >= static_cast<int>(names.size()))
        return;
      defaultName = names[sel];
      path = nameToFile[defaultName];
    }
  } else {
    wxString objDir = wxString::FromUTF8(
        ProjectUtils::GetDefaultLibraryPath("scene objects"));
    wxFileDialog fdlg(this, "Select Object file", objDir, wxEmptyString, "*.*",
                      wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    wxFileName fn(fdlg.GetPath());
    defaultName = fn.GetName().ToStdString();
    path = std::string(fdlg.GetPath().mb_str());
  }

  long qty = wxGetNumberFromUser("Enter object quantity:", wxEmptyString,
                                 "Add Scene Object", 1, 1, 1000, this);
  if (qty <= 0)
    return;

  namespace fs = std::filesystem;
  cfg.PushUndoState("add scene object");
  std::string base = scene.basePath;
  if (!base.empty()) {
    fs::path abs = fs::absolute(path);
    fs::path b = fs::absolute(base);
    if (abs.string().rfind(b.string(), 0) == 0)
      path = fs::relative(abs, b).string();
  }

  auto baseId = std::chrono::steady_clock::now().time_since_epoch().count();
  std::string layerName = cfg.GetCurrentLayer();
  bool hasLayer = false;
  for (const auto &[uid, layer] : scene.layers) {
    if (layer.name == layerName) {
      hasLayer = true;
      break;
    }
  }
  if (!hasLayer) {
    Layer layer;
    layer.uuid = wxString::Format("layer_%lld", static_cast<long long>(baseId))
                     .ToStdString();
    layer.name = layerName;
    scene.layers[layer.uuid] = layer;
  }

  for (long i = 0; i < qty; ++i) {
    SceneObject obj;
    obj.uuid = wxString::Format("uuid_%lld", static_cast<long long>(baseId + i))
                   .ToStdString();
    if (qty > 1)
      obj.name = defaultName + " " + std::to_string(i + 1);
    else
      obj.name = defaultName;
    obj.modelFile = path;
    obj.layer = layerName;
    scene.sceneObjects[obj.uuid] = obj;
  }

  if (sceneObjPanel)
    sceneObjPanel->ReloadData();
  if (viewportPanel) {
    viewportPanel->UpdateScene();
    viewportPanel->Refresh();
  }
  RefreshSummary();
}

void MainWindow::OnDelete(wxCommandEvent &WXUNUSED(event)) {
  if (fixturePanel && fixturePanel->IsActivePage())
    fixturePanel->DeleteSelected();
  else if (trussPanel && trussPanel->IsActivePage())
    trussPanel->DeleteSelected();
  else if (hoistPanel && hoistPanel->IsActivePage())
    hoistPanel->DeleteSelected();
  else if (sceneObjPanel && sceneObjPanel->IsActivePage())
    sceneObjPanel->DeleteSelected();
}
