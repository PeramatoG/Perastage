#include "mainwindow.h"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>

#include "configmanager.h"
#include "guiconfigservices.h"
#include "projectutils.h"
#include "resource_path_utils.h"
#include "truss_creation_source.h"
#include "trussdictionary.h"
#include "trussloader.h"
#include "trusstablepanel.h"

namespace {

// Extracts the list of selected truss UUIDs that still exist in the scene.
std::vector<std::string> GetSelectedExistingTrussUuids(ConfigManager &cfg) {
  const auto &scene = cfg.GetScene();
  std::vector<std::string> selected;
  for (const std::string &uuid : cfg.GetSelectedTrusses()) {
    if (scene.trusses.find(uuid) != scene.trusses.end())
      selected.push_back(uuid);
  }
  return selected;
}

// Returns the filename stem from a UTF-8 path for fallback truss names.
std::string GetPathStem(const std::string &path) {
  return std::filesystem::path(path).stem().string();
}

// Prompts for a truss definition source and returns the selected path.
std::optional<std::pair<std::string, std::string>>
ChooseReplacementTrussPath(wxWindow *parent, const MvrScene &scene) {
  wxArrayString sourceChoices;
  sourceChoices.push_back(_("Truss from scene"));
  sourceChoices.push_back(_("Truss from dictionary"));
  sourceChoices.push_back(_("Truss file"));
  wxSingleChoiceDialog sourceDlg(
      parent, _("Choose the source for the replacement truss:"), _("Replace Trusses"),
      sourceChoices);
  if (sourceDlg.ShowModal() != wxID_OK)
    return std::nullopt;

  const int sourceSelection = sourceDlg.GetSelection();
  if (sourceSelection == 0) {
    const std::vector<gui::TrussCreationSource> trussSources =
        gui::CollectTrussCreationSources(scene.trusses, scene.basePath);
    if (trussSources.empty()) {
      wxMessageBox(_("There are no reusable truss definitions in the scene."),
                   _("Replace Trusses"), wxOK | wxICON_WARNING, parent);
      return std::nullopt;
    }

    wxArrayString choices;
    for (const gui::TrussCreationSource &source : trussSources)
      choices.push_back(wxString::FromUTF8(source.displayName));
    wxSingleChoiceDialog pickDlg(
        parent, _("Choose a truss from the scene:"), _("Replace Trusses"), choices);
    if (pickDlg.ShowModal() != wxID_OK)
      return std::nullopt;
    const int idx = pickDlg.GetSelection();
    if (idx < 0 || idx >= static_cast<int>(trussSources.size()))
      return std::nullopt;
    const gui::TrussCreationSource &source =
        trussSources[static_cast<size_t>(idx)];
    return std::make_pair(source.definitionPath, source.displayName);
  }

  if (sourceSelection == 1) {
    auto dict = TrussDictionary::Load();
    if (!dict || dict->empty()) {
      wxMessageBox(_("The truss dictionary is empty."), _("Replace Trusses"),
                   wxOK | wxICON_WARNING, parent);
      return std::nullopt;
    }

    wxArrayString choices;
    std::vector<std::pair<std::string, std::string>> entries;
    for (const auto &[modelName, filePath] : *dict) {
      if (filePath.empty())
        continue;
      choices.push_back(wxString::FromUTF8(modelName));
      entries.emplace_back(modelName, filePath);
    }
    if (entries.empty()) {
      wxMessageBox(_("No dictionary entries contain a truss file path."),
                   _("Replace Trusses"), wxOK | wxICON_WARNING, parent);
      return std::nullopt;
    }

    wxSingleChoiceDialog pickDlg(parent, _("Choose a truss from the dictionary:"),
                                 _("Replace Trusses"), choices);
    if (pickDlg.ShowModal() != wxID_OK)
      return std::nullopt;
    const int idx = pickDlg.GetSelection();
    if (idx < 0 || idx >= static_cast<int>(entries.size()))
      return std::nullopt;
    const auto &entry = entries[static_cast<size_t>(idx)];
    return std::make_pair(entry.second, entry.first);
  }

  wxString trussDir =
      wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("trusses"));
  wxFileDialog fdlg(parent, _("Select Truss file"), trussDir, wxEmptyString,
                    wxString::FromUTF8(GetTrussDefinitionFileDialogWildcard()),
                    wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (fdlg.ShowModal() != wxID_OK)
    return std::nullopt;
  const std::string path = std::string(fdlg.GetPath().ToUTF8());
  wxFileName fn(fdlg.GetPath());
  return std::make_pair(path, std::string(fn.GetName().ToUTF8()));
}

// Copies type and geometry data from a replacement truss while preserving
// instance state.
void ApplyTrussReplacement(Truss &target, const Truss &replacement) {
  const std::string keepUuid = target.uuid;
  const std::string keepName = target.name;
  const std::string keepLayer = target.layer;
  const std::string keepPosition = target.position;
  const std::string keepPositionName = target.positionName;
  const int keepUnitNumber = target.unitNumber;
  const int keepCustomId = target.customId;
  const int keepCustomIdType = target.customIdType;
  const Matrix keepTransform = target.transform;
  const Matrix keepLocalTransform = target.localTransform;
  const bool keepHasLocalTransform = target.hasLocalTransform;
  const std::string keepParentGroupUuid = target.parentGroupUuid;
  const float keepCalculatedLoadKg = target.calculatedLoadKg;
  const float keepManualLoadKg = target.manualLoadKg;
  const bool keepHasManualLoadOverride = target.hasManualLoadOverride;

  target = replacement;
  target.uuid = keepUuid;
  target.name = keepName;
  target.layer = keepLayer;
  target.position = keepPosition;
  target.positionName = keepPositionName;
  target.unitNumber = keepUnitNumber;
  target.customId = keepCustomId;
  target.customIdType = keepCustomIdType;
  target.transform = keepTransform;
  target.localTransform = keepLocalTransform;
  target.hasLocalTransform = keepHasLocalTransform;
  target.parentGroupUuid = keepParentGroupUuid;
  target.calculatedLoadKg = keepCalculatedLoadKg;
  target.manualLoadKg = keepManualLoadKg;
  target.hasManualLoadOverride = keepHasManualLoadOverride;
}

} // namespace

// Replaces selected trusses with a truss chosen from scene, dictionary, or file
// source.
void MainWindow::OnReplaceSelectedTrusses(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto selectedUuids = GetSelectedExistingTrussUuids(cfg);
  if (selectedUuids.empty()) {
    wxMessageBox(_("Select at least one truss before running Replace Trusses."),
                 _("Replace Trusses"), wxOK | wxICON_INFORMATION, this);
    return;
  }

  auto selectedPath = ChooseReplacementTrussPath(this, cfg.GetScene());
  if (!selectedPath.has_value())
    return;

  Truss replacement;
  if (!LoadTrussDefinition(selectedPath->first, replacement)) {
    wxMessageBox(_("Unsupported or unreadable truss file. Supported formats are "
                 "GDTF, GTruss, GLB, and 3DS."),
                 _("Replace Trusses"), wxOK | wxICON_ERROR, this);
    return;
  }

  if (replacement.name.empty())
    replacement.name = selectedPath->second.empty()
                           ? GetPathStem(selectedPath->first)
                           : selectedPath->second;

  cfg.PushUndoState("replace selected trusses");
  auto &scene = cfg.GetScene();
  const std::string base = scene.basePath;
  replacement.gdtfSpec = gui::MakeSceneRelativeResourcePathOrOriginal(
      base, replacement.gdtfSpec, "Replace truss GDTF path");
  replacement.symbolFile = gui::MakeSceneRelativeResourcePathOrOriginal(
      base, replacement.symbolFile, "Replace truss symbol path");
  if (!replacement.modelFile.empty()) {
    replacement.modelFile = gui::MakeSceneRelativeResourcePathOrOriginal(
        base, replacement.modelFile, "Replace truss model path");
  }

  for (const std::string &uuid : selectedUuids) {
    auto it = scene.trusses.find(uuid);
    if (it == scene.trusses.end())
      continue;
    ApplyTrussReplacement(it->second, replacement);
  }

  cfg.SetSelectedTrusses(selectedUuids);
  if (trussPanel) {
    trussPanel->ReloadData();
    trussPanel->SelectByUuid(selectedUuids, false);
  }

  RefreshAfterSceneChange();
}
