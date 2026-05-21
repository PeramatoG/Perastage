#include "mainwindow.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include <wx/choicdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>

#include "configmanager.h"
#include "fixture.h"
#include "fixturetablepanel.h"
#include "gdtfdictionary.h"
#include "gdtfloader.h"
#include "guiconfigservices.h"
#include "projectutils.h"

namespace {

// Stores a replacement fixture template selected by the user.
struct ReplacementFixtureTemplate {
  std::string typeName;
  std::string gdtfSpec;
  std::string gdtfMode;
  float weightKg = 0.0f;
  float powerConsumptionW = 0.0f;
  std::string color;
};


// Normalizes a GDTF path into a comparable lowercase filename token.
std::string BuildGdtfFileKey(const std::string &gdtfSpec) {
  if (gdtfSpec.empty())
    return {};
  std::string key = std::filesystem::path(gdtfSpec).filename().string();
  std::transform(key.begin(), key.end(), key.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return key;
}

// Generates a random color in #RRGGBB format for assigning new fixture groups.
std::string GenerateAutoColor() {
  static std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(0, 255);
  return wxString::Format("#%02X%02X%02X", dist(rng), dist(rng), dist(rng))
      .ToStdString();
}

// Resolves the replacement color by reusing existing fixture-file color or creating a new group color.
std::string ResolveReplacementColor(const Scene &scene,
                                    const std::vector<std::string> &selectedUuids,
                                    const std::string &replacementSpec,
                                    const std::string &fallbackColor) {
  const std::string replacementKey = BuildGdtfFileKey(replacementSpec);
  if (replacementKey.empty())
    return fallbackColor.empty() ? GenerateAutoColor() : fallbackColor;

  std::unordered_set<std::string> selectedSet(selectedUuids.begin(), selectedUuids.end());
  for (const auto &[uuid, fixture] : scene.fixtures) {
    if (selectedSet.find(uuid) != selectedSet.end())
      continue;
    if (BuildGdtfFileKey(fixture.gdtfSpec) != replacementKey)
      continue;
    if (!fixture.color.empty())
      return fixture.color;
  }

  if (!fallbackColor.empty())
    return fallbackColor;
  return GenerateAutoColor();
}

// Extracts the list of selected fixture UUIDs that still exist in the current scene.
std::vector<std::string> GetSelectedExistingFixtureUuids(ConfigManager &cfg) {
  const auto &scene = cfg.GetScene();
  std::vector<std::string> selected;
  for (const std::string &uuid : cfg.GetSelectedFixtures()) {
    if (scene.fixtures.find(uuid) != scene.fixtures.end())
      selected.push_back(uuid);
  }
  return selected;
}

// Prompts the user to select a fixture mode when multiple modes exist.
std::optional<std::string> ChooseFixtureMode(wxWindow *parent,
                                             const std::string &gdtfPath,
                                             const std::string &preferred) {
  std::vector<std::string> modes = GetGdtfModes(gdtfPath);
  if (modes.empty())
    return std::nullopt;
  if (modes.size() == 1)
    return modes.front();

  wxArrayString choices;
  int initialSelection = wxNOT_FOUND;
  for (size_t i = 0; i < modes.size(); ++i) {
    choices.push_back(wxString::FromUTF8(modes[i]));
    if (modes[i] == preferred)
      initialSelection = static_cast<int>(i);
  }

  wxSingleChoiceDialog dlg(parent, "Select fixture mode", "Fixture mode", choices);
  if (initialSelection != wxNOT_FOUND)
    dlg.SetSelection(initialSelection);
  if (dlg.ShowModal() != wxID_OK)
    return std::nullopt;
  return std::string(dlg.GetStringSelection().ToUTF8());
}

} // namespace

// Replaces selected fixtures with a fixture chosen from scene, dictionary, or file source.
void MainWindow::OnReplaceSelectedFixtures(wxCommandEvent &WXUNUSED(event)) {
  ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
  auto selectedUuids = GetSelectedExistingFixtureUuids(cfg);
  if (selectedUuids.empty()) {
    wxMessageBox("Select at least one fixture before running Replace Fixtures.",
                 "Replace Fixtures", wxOK | wxICON_INFORMATION, this);
    return;
  }

  wxArrayString sourceChoices;
  sourceChoices.push_back("Fixture from scene");
  sourceChoices.push_back("Fixture from dictionary");
  sourceChoices.push_back("GDTF download/file");
  wxSingleChoiceDialog sourceDlg(
      this,
      "Choose the source for the replacement fixture:",
      "Replace Fixtures",
      sourceChoices);
  if (sourceDlg.ShowModal() != wxID_OK)
    return;

  ReplacementFixtureTemplate replacement;
  const int sourceSelection = sourceDlg.GetSelection();

  if (sourceSelection == 0) {
    const auto &scene = cfg.GetScene();
    std::vector<std::pair<std::string, const Fixture *>> sceneOptions;
    for (const auto &[uuid, fixture] : scene.fixtures) {
      if (fixture.gdtfSpec.empty())
        continue;
      const std::string modeLabel = fixture.gdtfMode.empty() ? "(no mode)" : fixture.gdtfMode;
      const std::string label = fixture.typeName + " | " + modeLabel + " | id " + std::to_string(fixture.fixtureId);
      sceneOptions.emplace_back(label, &fixture);
    }
    if (sceneOptions.empty()) {
      wxMessageBox("There are no fixtures with GDTF data in the scene.",
                   "Replace Fixtures", wxOK | wxICON_WARNING, this);
      return;
    }
    wxArrayString choices;
    std::vector<const Fixture *> ordered;
    for (const auto &[label, fixture] : sceneOptions) {
      choices.push_back(wxString::FromUTF8(label));
      ordered.push_back(fixture);
    }
    wxSingleChoiceDialog pickDlg(this, "Choose a fixture from the scene:",
                                 "Replace Fixtures", choices);
    if (pickDlg.ShowModal() != wxID_OK)
      return;
    const int idx = pickDlg.GetSelection();
    if (idx < 0 || idx >= static_cast<int>(ordered.size()))
      return;
    const Fixture &picked = *ordered[static_cast<size_t>(idx)];
    replacement.typeName = picked.typeName;
    replacement.gdtfSpec = picked.gdtfSpec;
    replacement.gdtfMode = picked.gdtfMode;
    replacement.weightKg = picked.weightKg;
    replacement.powerConsumptionW = picked.powerConsumptionW;
    replacement.color = picked.color;
  } else if (sourceSelection == 1) {
    auto dict = GdtfDictionary::Load();
    if (!dict || dict->empty()) {
      wxMessageBox("The fixture dictionary is empty.", "Replace Fixtures",
                   wxOK | wxICON_WARNING, this);
      return;
    }
    wxArrayString choices;
    std::vector<std::pair<std::string, GdtfDictionary::Entry>> entries;
    for (const auto &[typeName, entry] : *dict) {
      if (entry.path.empty())
        continue;
      choices.push_back(wxString::FromUTF8(typeName));
      entries.emplace_back(typeName, entry);
    }
    if (entries.empty()) {
      wxMessageBox("No dictionary entries contain a GDTF path.",
                   "Replace Fixtures", wxOK | wxICON_WARNING, this);
      return;
    }
    wxSingleChoiceDialog pickDlg(this, "Choose a fixture from the dictionary:",
                                 "Replace Fixtures", choices);
    if (pickDlg.ShowModal() != wxID_OK)
      return;
    const int idx = pickDlg.GetSelection();
    if (idx < 0 || idx >= static_cast<int>(entries.size()))
      return;
    replacement.typeName = entries[static_cast<size_t>(idx)].first;
    replacement.gdtfSpec = entries[static_cast<size_t>(idx)].second.path;
    replacement.gdtfMode = entries[static_cast<size_t>(idx)].second.mode;
    replacement.color = entries[static_cast<size_t>(idx)].second.color;
  } else {
    wxString fixDir = wxString::FromUTF8(ProjectUtils::GetWritableLibraryPath("fixtures"));
    wxFileDialog fdlg(this, "Select GDTF file", fixDir, wxEmptyString,
                      "*.gdtf", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (fdlg.ShowModal() != wxID_OK)
      return;
    replacement.gdtfSpec = std::string(fdlg.GetPath().ToUTF8());
    replacement.typeName = GetGdtfFixtureName(replacement.gdtfSpec);
    if (replacement.typeName.empty()) {
      wxFileName fn(fdlg.GetPath());
      replacement.typeName = std::string(fn.GetName().ToUTF8());
    }
  }

  if (replacement.gdtfSpec.empty()) {
    wxMessageBox("Invalid replacement fixture selection.", "Replace Fixtures",
                 wxOK | wxICON_ERROR, this);
    return;
  }

  const auto selectedMode = ChooseFixtureMode(this, replacement.gdtfSpec, replacement.gdtfMode);
  if (!selectedMode.has_value()) {
    wxMessageBox("Could not read fixture modes from the selected GDTF.",
                 "Replace Fixtures", wxOK | wxICON_ERROR, this);
    return;
  }
  replacement.gdtfMode = *selectedMode;

  float weight = 0.0f;
  float power = 0.0f;
  GetGdtfProperties(replacement.gdtfSpec, weight, power);
  replacement.weightKg = weight;
  replacement.powerConsumptionW = power;
  if (replacement.color.empty())
    replacement.color = GetGdtfModelColor(replacement.gdtfSpec);

  cfg.PushUndoState("replace selected fixtures");
  auto &scene = cfg.GetScene();
  const std::string replacementColor =
      ResolveReplacementColor(scene, selectedUuids, replacement.gdtfSpec, replacement.color);
  for (const std::string &uuid : selectedUuids) {
    auto it = scene.fixtures.find(uuid);
    if (it == scene.fixtures.end())
      continue;
    Fixture &target = it->second;
    const int keepFixtureId = target.fixtureId;
    const std::string keepName = target.instanceName;
    const std::string keepLayer = target.layer;
    const std::string keepPositionName = target.positionName;
    const std::string keepAddress = target.address;
    const Matrix keepTransform = target.transform;

    target.typeName = replacement.typeName;
    target.gdtfSpec = replacement.gdtfSpec;
    target.gdtfMode = replacement.gdtfMode;
    target.weightKg = replacement.weightKg;
    target.powerConsumptionW = replacement.powerConsumptionW;
    target.color = replacementColor;

    target.fixtureId = keepFixtureId;
    target.instanceName = keepName;
    target.layer = keepLayer;
    target.positionName = keepPositionName;
    target.address = keepAddress;
    target.transform = keepTransform;
  }

  // Restores the fixture selection so replacement keeps internal UUID targeting stable.
  cfg.SetSelectedFixtures(selectedUuids);
  if (fixturePanel)
    fixturePanel->SelectByUuid(selectedUuids, false);

  RefreshAfterSceneChange();
}
