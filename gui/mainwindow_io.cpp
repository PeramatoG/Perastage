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
#include "mainwindow_io_controller.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <tinyxml2.h>
#include <wx/busyinfo.h>
#include <wx/log.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "consolepanel.h"
#include "exportfixturedialog.h"
#include "exportobjectdialog.h"
#include "exporttrussdialog.h"
#include "fixture.h"
#include "fixturetablepanel.h"
#include "hoisttablepanel.h"
#include "layoutviewerpanel.h"
#include "mvrexporter.h"
#include "mvrimporter.h"
#include "projectutils.h"
#include "riderimporter.h"
#include "ridertextdialog.h"
#include "sceneobjecttablepanel.h"
#include "tableprinter.h"
#include "truss_gdtf_builder.h"
#include "trusstablepanel.h"
#include "viewer2dpanel.h"
#include "viewer2drenderpanel.h"
#include "viewer3dpanel.h"

namespace {

std::string EnsureProjectFileExtension(const std::string &path) {
  if (path.empty())
    return path;

  const std::filesystem::path candidate(path);
  const std::string ext = candidate.extension().string();
  const std::string requiredExt = ProjectUtils::PROJECT_EXTENSION;
  if (ext == requiredExt)
    return candidate.string();
  if (!ext.empty())
    return candidate.string();

  std::filesystem::path withExtension = candidate;
  withExtension += requiredExt;
  return withExtension.string();
}

class ScopeExit {
public:
  explicit ScopeExit(std::function<void()> fn) : fn_(std::move(fn)) {}
  ~ScopeExit() {
    if (fn_)
      fn_();
  }

  ScopeExit(const ScopeExit &) = delete;
  ScopeExit &operator=(const ScopeExit &) = delete;

private:
  std::function<void()> fn_;
};

} // namespace

void MainWindow::LockViewportInteraction() {
  ++viewportInteractionLockDepth;
  if (viewportInteractionLockDepth > 1)
    return;

  if (viewportPanel)
    viewportPanel->Disable();
  if (viewport2DPanel)
    viewport2DPanel->Disable();
  if (layoutViewerPanel)
    layoutViewerPanel->Disable();
}

void MainWindow::UnlockViewportInteraction() {
  if (viewportInteractionLockDepth <= 0)
    return;

  --viewportInteractionLockDepth;
  if (viewportInteractionLockDepth > 0)
    return;

  if (viewportPanel)
    viewportPanel->Enable();
  if (viewport2DPanel)
    viewport2DPanel->Enable();
  if (layoutViewerPanel)
    layoutViewerPanel->Enable();
}

void MainWindow::OnLoad(wxCommandEvent &event) {
  if (!GuardStartupProjectLoadAction("opening another project"))
    return;

  if (!ConfirmSaveIfDirty("loading a project", "Open Project"))
    return;

  const wxString projectExtension =
      wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION);
  wxString filter = "Perastage files (*" + projectExtension + ")|*" +
                    projectExtension;
  wxString projDir;
  if (auto last = ProjectUtils::LoadLastProjectPath())
    projDir =
        wxString::FromUTF8(std::filesystem::path(*last).parent_path().string());
  else
    projDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("projects"));
  wxFileDialog dlg(this, "Open Project", projDir, "", filter,
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  wxString path = dlg.GetPath();
  if (!LoadProjectFromPath(path.ToStdString()))
    wxMessageBox("Failed to load project.", "Error", wxICON_ERROR);
}

bool MainWindow::OpenPathFromCommandLine(const std::string &path) {
  if (!ioController)
    return false;
  return ioController->OpenPathFromCommandLine(path);
}

void MainWindow::OnSave(wxCommandEvent &event) {
  if (currentProjectPath.empty()) {
    OnSaveAs(event);
    return;
  }
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  // Always sync table edits before saving; each panel now applies only real changes.
  SyncSceneData();
  FlushPendingFixtureSymbolLibraryUpdates();
  SaveUserConfigWithViewport2DState();
  if (!cfg.SaveProject(currentProjectPath))
    wxMessageBox("Failed to save project.", "Error", wxICON_ERROR);
  else {
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
    if (consolePanel)
      consolePanel->AppendMessage("[INFO] Saved " +
                                  wxString::FromUTF8(currentProjectPath));
  }
}

void MainWindow::OnSaveAs(wxCommandEvent &event) {
  const wxString projectExtension =
      wxString::FromUTF8(ProjectUtils::PROJECT_EXTENSION);
  wxString filter = "Perastage files (*" + projectExtension + ")|*" +
                    projectExtension;
  wxString projDir;
  if (!currentProjectPath.empty())
    projDir = wxString::FromUTF8(
        std::filesystem::path(currentProjectPath).parent_path().string());
  else if (auto last = ProjectUtils::LoadLastProjectPath())
    projDir =
        wxString::FromUTF8(std::filesystem::path(*last).parent_path().string());
  else
    projDir =
        wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("projects"));
  wxFileDialog dlg(this, "Save Project", projDir, "", filter,
                   wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  currentProjectPath = EnsureProjectFileExtension(dlg.GetPath().ToStdString());
  auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  // Always sync table edits before saving; each panel now applies only real changes.
  SyncSceneData();
  FlushPendingFixtureSymbolLibraryUpdates();
  SaveUserConfigWithViewport2DState();
  if (!cfg.SaveProject(currentProjectPath))
    wxMessageBox("Failed to save project.", "Error", wxICON_ERROR);
  else {
    ProjectUtils::SaveLastProjectPath(currentProjectPath);
    if (consolePanel)
      consolePanel->AppendMessage("[INFO] Saved " +
                                  wxString::FromUTF8(currentProjectPath));
  }
  UpdateTitle();
}

// Import fixtures and trusses from a rider (.txt/.pdf)
void MainWindow::OnImportRider(wxCommandEvent &event) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("misc"));
  wxFileDialog dlg(this, "Import Rider", miscDir, "",
                   "Rider files (*.txt;*.pdf)|*.txt;*.pdf",
                   wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (dlg.ShowModal() == wxID_CANCEL)
    return;

  const std::filesystem::path selectedPath(dlg.GetPath().ToStdWstring());
  const auto pathU8 = selectedPath.u8string();
  const std::string pathUtf8(pathU8.begin(), pathU8.end());
  std::unique_ptr<wxWindowDisabler> importDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> importOverlay =
      std::make_unique<wxBusyInfo>("Creating scene from rider...");
  wxYieldIfNeeded();

  LockViewportInteraction();
  ScopeExit viewportUnlock([this]() { UnlockViewportInteraction(); });
  if (!RiderImporter::Import(pathUtf8)) {
    wxMessageBox("Failed to import rider.", "Error", wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("[ERROR] Failed to import " + dlg.GetPath());
  } else {
    importOverlay.reset();
    importDisabler.reset();
    if (consolePanel)
      consolePanel->AppendMessage("[INFO] Imported " + dlg.GetPath());
    importDisabler = std::make_unique<wxWindowDisabler>();
    importOverlay = std::make_unique<wxBusyInfo>("Refreshing scene view...");
    wxYieldIfNeeded();
    RefreshAfterSceneChange();
  }
}

void MainWindow::OnImportRiderText(wxCommandEvent &WXUNUSED(event)) {
  RiderTextDialog dlg(this);
  if (dlg.ShowModal() != wxID_OK)
    return;

  const std::string riderText = dlg.GetRiderTextUtf8();
  if (riderText.empty()) {
    wxMessageBox("Rider text is empty.", "Error", wxICON_ERROR);
    return;
  }

  LockViewportInteraction();
  ScopeExit viewportUnlock([this]() { UnlockViewportInteraction(); });
  std::unique_ptr<wxWindowDisabler> createDisabler =
      std::make_unique<wxWindowDisabler>();
  std::unique_ptr<wxBusyInfo> createOverlay =
      std::make_unique<wxBusyInfo>("Generating scene from text...");
  wxYieldIfNeeded();

  if (!RiderImporter::ImportText(riderText)) {
    wxMessageBox("Failed to import rider text.", "Error", wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("[ERROR] Failed to import rider from text.");
    return;
  }

  if (consolePanel)
    consolePanel->AppendMessage("[INFO] Imported rider from text.");
  RefreshAfterSceneChange();
}

// Handles MVR file selection, import, and updates fixture/truss panels
// accordingly
void MainWindow::OnImportMVR(wxCommandEvent &event) {
  if (ioController)
    ioController->OnImportMVR(event);
}

void MainWindow::OnExportMVR(wxCommandEvent &event) {
  wxString miscDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("misc"));
  wxFileDialog saveFileDialog(this, "Export MVR file", miscDir, "",
                              "MVR files (*.mvr)|*.mvr",
                              wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (saveFileDialog.ShowModal() == wxID_CANCEL)
    return;

  SyncSceneData();
  MvrExporter exporter;
  wxString path = saveFileDialog.GetPath();
  if (!exporter.ExportToFile(path.ToStdString())) {
    wxMessageBox("Failed to export MVR file.", "Error", wxICON_ERROR);
    if (consolePanel)
      consolePanel->AppendMessage("[ERROR] Failed to export " + path);
  } else {
    wxMessageBox("MVR file exported successfully.", "Success",
                 wxICON_INFORMATION);
    if (consolePanel)
      consolePanel->AppendMessage("[INFO] Exported " + path);
  }
}

void MainWindow::OnExportTruss(wxCommandEvent &WXUNUSED(event)) {
  const auto &trusses = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().trusses;
  std::set<std::string> names;
  for (const auto &[uuid, t] : trusses)
    names.insert(t.name);
  if (names.empty()) {
    wxMessageBox("No truss data available.", "Export Truss",
                 wxOK | wxICON_INFORMATION);
    return;
  }
  std::vector<std::string> list(names.begin(), names.end());
  ExportTrussDialog dlg(this, list);
  if (dlg.ShowModal() != wxID_OK)
    return;

  std::string sel = dlg.GetSelectedName();
  const Truss *chosen = nullptr;
  for (const auto &[uuid, t] : trusses) {
    if (t.name == sel) {
      chosen = &t;
      break;
    }
  }
  if (!chosen)
    return;

  wxString trussDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("trusses"));
  wxFileDialog saveDlg(this, "Save Truss", trussDir,
                       wxString::FromUTF8(sel) + ".gdtf",
                       "GDTF files (*.gdtf)|*.gdtf",
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDlg.ShowModal() != wxID_OK)
    return;

  namespace fs = std::filesystem;
  std::string modelPath = chosen->symbolFile;
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  if (fs::path(modelPath).is_relative() && !scene.basePath.empty())
    modelPath = (fs::path(scene.basePath) / modelPath).string();
  if (!fs::exists(modelPath)) {
    wxMessageBox("Model file not found.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  Truss exportTruss = *chosen;
  exportTruss.symbolFile = modelPath;
  std::string error;
  if (!BuildTrussGdtfFromInstance(
          exportTruss, std::string(saveDlg.GetPath().mb_str()), &error)) {
    wxMessageBox(error.empty() ? "Failed to export truss GDTF." : error,
                 "Error", wxOK | wxICON_ERROR);
    return;
  }

  wxMessageBox("Truss exported successfully.", "Export Truss",
               wxOK | wxICON_INFORMATION);
}

void MainWindow::OnExportFixture(wxCommandEvent &WXUNUSED(event)) {
  namespace fs = std::filesystem;
  auto createTempDir = []() {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::string folderName = "GDTF_" + std::to_string(now);
    fs::path base = fs::temp_directory_path();
    fs::path full = base / folderName;
    fs::create_directory(full);
    return full.string();
  };
  auto extractZip = [](const std::string &zipPath, const std::string &destDir) {
    if (!fs::exists(zipPath)) {
      if (ConsolePanel::Instance()) {
        wxString msg = "GDTF: cannot open " + wxString::FromUTF8(zipPath);
        ConsolePanel::Instance()->AppendMessage(msg);
      }
      return false;
    }
    wxLogNull logNo;
    wxFileInputStream input(zipPath);
    if (!input.IsOk()) {
      if (ConsolePanel::Instance()) {
        wxString msg = "GDTF: cannot open " + wxString::FromUTF8(zipPath);
        ConsolePanel::Instance()->AppendMessage(msg);
      }
      return false;
    }
    wxZipInputStream zipStream(input);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zipStream.GetNextEntry())), entry) {
      std::string filename = entry->GetName().ToStdString();
      std::string fullPath = destDir + "/" + filename;
      if (entry->IsDir()) {
        wxFileName::Mkdir(fullPath, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        continue;
      }
      wxFileName::Mkdir(wxFileName(fullPath).GetPath(), wxS_DIR_DEFAULT,
                        wxPATH_MKDIR_FULL);
      std::ofstream output(fullPath, std::ios::binary);
      if (!output.is_open())
        return false;
      char buffer[4096];
      while (true) {
        zipStream.Read(buffer, sizeof(buffer));
        size_t bytes = zipStream.LastRead();
        if (bytes == 0)
          break;
        output.write(buffer, bytes);
      }
      output.close();
    }
    return true;
  };
  const auto &fixtures = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().fixtures;
  std::set<std::string> types;
  for (const auto &[uuid, f] : fixtures)
    if (!f.typeName.empty())
      types.insert(f.typeName);
  if (types.empty()) {
    wxMessageBox("No fixture data available.", "Export Fixture",
                 wxOK | wxICON_INFORMATION);
    return;
  }
  std::vector<std::string> list(types.begin(), types.end());
  ExportFixtureDialog dlg(this, list);
  if (dlg.ShowModal() != wxID_OK)
    return;

  std::string sel = dlg.GetSelectedType();
  const Fixture *chosen = nullptr;
  for (const auto &[uuid, f] : fixtures) {
    if (f.typeName == sel) {
      chosen = &f;
      break;
    }
  }
  if (!chosen || chosen->gdtfSpec.empty())
    return;

  fs::path src = chosen->gdtfSpec;
  const std::string &base = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene().basePath;
  if (src.is_relative() && !base.empty())
    src = fs::path(base) / src;
  if (!fs::exists(src)) {
    wxMessageBox("GDTF file not found.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  wxString fixDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("fixtures"));
  wxFileDialog saveDlg(this, "Save Fixture", fixDir,
                       wxString::FromUTF8(sel) + ".gdtf", "*.gdtf",
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDlg.ShowModal() != wxID_OK)
    return;

  std::string tempDir = createTempDir();
  if (!extractZip(src.string(), tempDir)) {
    wxMessageBox("Failed to read GDTF.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  fs::path descPath = fs::path(tempDir) / "description.xml";
  tinyxml2::XMLDocument doc;
  if (doc.LoadFile(descPath.string().c_str()) != tinyxml2::XML_SUCCESS) {
    fs::remove_all(tempDir);
    wxMessageBox("Failed to parse description.xml.", "Error",
                 wxOK | wxICON_ERROR);
    return;
  }

  tinyxml2::XMLElement *ft = doc.FirstChildElement("GDTF");
  if (ft)
    ft = ft->FirstChildElement("FixtureType");
  else
    ft = doc.FirstChildElement("FixtureType");
  if (!ft) {
    fs::remove_all(tempDir);
    wxMessageBox("Invalid GDTF file.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  tinyxml2::XMLElement *phys = ft->FirstChildElement("PhysicalDescriptions");
  if (!phys)
    phys =
        ft->InsertEndChild(doc.NewElement("PhysicalDescriptions"))->ToElement();
  tinyxml2::XMLElement *props = phys->FirstChildElement("Properties");
  if (!props)
    props = phys->InsertEndChild(doc.NewElement("Properties"))->ToElement();

  if (chosen->weightKg != 0.0f) {
    tinyxml2::XMLElement *w = props->FirstChildElement("Weight");
    if (!w)
      w = props->InsertEndChild(doc.NewElement("Weight"))->ToElement();
    w->SetAttribute("Value", chosen->weightKg);
  }

  if (chosen->powerConsumptionW != 0.0f) {
    tinyxml2::XMLElement *pc = props->FirstChildElement("PowerConsumption");
    if (!pc)
      pc = props->InsertEndChild(doc.NewElement("PowerConsumption"))
               ->ToElement();
    pc->SetAttribute("Value", chosen->powerConsumptionW);
  }

  doc.SaveFile(descPath.string().c_str());

  wxFileOutputStream out(std::string(saveDlg.GetPath().mb_str()));
  if (!out.IsOk()) {
    fs::remove_all(tempDir);
    wxMessageBox("Failed to write file.", "Error", wxOK | wxICON_ERROR);
    return;
  }
  wxZipOutputStream zip(out);
  for (auto &p : fs::recursive_directory_iterator(tempDir)) {
    if (!p.is_regular_file())
      continue;
    std::string rel = fs::relative(p.path(), tempDir).string();
    auto *entry = new wxZipEntry(rel);
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(entry);
    std::ifstream in(p.path(), std::ios::binary);
    char buf[4096];
    while (in.good()) {
      in.read(buf, sizeof(buf));
      std::streamsize s = in.gcount();
      if (s > 0)
        zip.Write(buf, s);
    }
    zip.CloseEntry();
  }
  zip.Close();
  fs::remove_all(tempDir);

  wxMessageBox("Fixture exported successfully.", "Export Fixture",
               wxOK | wxICON_INFORMATION);
}

void MainWindow::OnExportSceneObject(wxCommandEvent &WXUNUSED(event)) {
  namespace fs = std::filesystem;
  const auto &scene = GetDefaultGuiConfigServices().LegacyConfigManager().GetScene();
  const auto &objs = scene.sceneObjects;
  std::set<std::string> names;
  for (const auto &[uuid, obj] : objs)
    if (!obj.name.empty())
      names.insert(obj.name);
  if (names.empty()) {
    wxMessageBox("No scene objects available.", "Export Scene Object",
                 wxOK | wxICON_INFORMATION);
    return;
  }
  std::vector<std::string> list(names.begin(), names.end());
  ExportObjectDialog dlg(this, list);
  if (dlg.ShowModal() != wxID_OK)
    return;

  std::string sel = dlg.GetSelectedName();
  const SceneObject *chosen = nullptr;
  for (const auto &[uuid, obj] : objs) {
    if (obj.name == sel) {
      chosen = &obj;
      break;
    }
  }
  if (!chosen || chosen->modelFile.empty())
    return;

  fs::path src = chosen->modelFile;
  if (src.is_relative() && !scene.basePath.empty())
    src = fs::path(scene.basePath) / src;
  if (!fs::exists(src)) {
    wxMessageBox("Model file not found.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  wxString defName =
      wxString::FromUTF8(sel) + wxString(src.extension().string());
  wxString objDir =
      wxString::FromUTF8(ProjectUtils::GetDefaultLibraryPath("scene_objects"));
  wxFileDialog saveDlg(this, "Save Object", objDir, defName,
                       wxString("*") + wxString(src.extension().string()),
                       wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if (saveDlg.ShowModal() != wxID_OK)
    return;

  fs::path dest = std::string(saveDlg.GetPath().mb_str());
  std::error_code ec;
  fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    wxMessageBox("Failed to copy file.", "Error", wxOK | wxICON_ERROR);
    return;
  }

  wxMessageBox("Object exported successfully.", "Export Scene Object",
               wxOK | wxICON_INFORMATION);
}

void MainWindow::OnExportCSV(wxCommandEvent &WXUNUSED(event)) {
  wxArrayString options;
  if (fixturePanel)
    options.Add("Fixtures");
  if (trussPanel)
    options.Add("Trusses");
  if (hoistPanel)
    options.Add("Hoists");
  if (sceneObjPanel)
    options.Add("Objects");
  if (options.IsEmpty())
    return;

  wxSingleChoiceDialog dlg(this, "Select table", "Export CSV", options);
  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString choice = dlg.GetStringSelection();
  wxDataViewListCtrl *ctrl = nullptr;
  TablePrinter::TableType type = TablePrinter::TableType::Fixtures;
  if (choice == "Fixtures" && fixturePanel) {
    ctrl = fixturePanel->GetTableCtrl();
    type = TablePrinter::TableType::Fixtures;
  } else if (choice == "Trusses" && trussPanel) {
    ctrl = trussPanel->GetTableCtrl();
    type = TablePrinter::TableType::Trusses;
  } else if (choice == "Hoists" && hoistPanel) {
    ctrl = hoistPanel->GetTableCtrl();
    type = TablePrinter::TableType::Supports;
  } else if (choice == "Objects" && sceneObjPanel) {
    ctrl = sceneObjPanel->GetTableCtrl();
    type = TablePrinter::TableType::SceneObjects;
  }

  if (ctrl)
    TablePrinter::ExportCSV(this, ctrl, type, GetDefaultGuiConfigServices().LegacyConfigManager());
}
