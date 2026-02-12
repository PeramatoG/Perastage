#include "configservices.h"

#include "json.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <memory>
#include <sstream>
#include <string_view>

#include <wx/stdpaths.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace {
std::vector<std::string> SplitCSV(const std::string &s) {
  std::vector<std::string> result;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    size_t start = item.find_first_not_of(" \t");
    size_t end = item.find_last_not_of(" \t");
    if (start != std::string::npos)
      result.push_back(item.substr(start, end - start + 1));
  }
  return result;
}

std::string JoinCSV(const std::vector<std::string> &items) {
  std::string out;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0)
      out += ',';
    out += items[i];
  }
  return out;
}

bool TryParseFloat(const std::string &text, float &out) {
  if (text.empty())
    return false;

  const auto first =
      std::find_if_not(text.begin(), text.end(), [](unsigned char c) {
        return std::isspace(c);
      });
  if (first == text.end())
    return false;
  const auto last =
      std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) {
        return std::isspace(c);
      }).base();
  std::string_view trimmed(&(*first), static_cast<size_t>(last - first));

  auto *begin = trimmed.data();
  auto *end = trimmed.data() + trimmed.size();

  auto result = std::from_chars(begin, end, out);
  return result.ec == std::errc{} && result.ptr == end;
}

class TempDir {
public:
  explicit TempDir(const std::string &prefix) {
    namespace fs = std::filesystem;
    auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    path = fs::temp_directory_path() / (prefix + std::to_string(stamp));
    created = std::filesystem::create_directory(path);
  }

  ~TempDir() {
    if (!created)
      return;
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }

  bool Valid() const { return created; }
  const std::filesystem::path &Path() const { return path; }

private:
  std::filesystem::path path;
  bool created = false;
};

bool LooksLikeZipFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return false;
  unsigned char signature[2] = {};
  file.read(reinterpret_cast<char *>(signature), sizeof(signature));
  return file.gcount() == static_cast<std::streamsize>(sizeof(signature)) &&
         signature[0] == 'P' && signature[1] == 'K';
}

bool LooksLikeJsonFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return false;
  char ch = '\0';
  while (file.get(ch)) {
    if (!std::isspace(static_cast<unsigned char>(ch)))
      return ch == '{' || ch == '[';
  }
  return false;
}

std::string ToLowerCopy(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}
} // namespace

void UserPreferencesStore::SetValue(const std::string &key,
                                    const std::string &value) {
  std::string newValue = value;

  auto var = variables.find(key);
  if (var != variables.end() && var->second.type == "float") {
    float parsed = 0.0f;
    if (TryParseFloat(value, parsed)) {
      parsed = std::clamp(parsed, var->second.minValue, var->second.maxValue);
      var->second.value = parsed;
      newValue = std::to_string(parsed);
    }
  }

  configData[key] = newValue;
}

std::optional<std::string>
UserPreferencesStore::GetValue(const std::string &key) const {
  auto it = configData.find(key);
  if (it != configData.end())
    return it->second;
  return std::nullopt;
}

bool UserPreferencesStore::HasKey(const std::string &key) const {
  return configData.find(key) != configData.end();
}

void UserPreferencesStore::RemoveKey(const std::string &key) {
  configData.erase(key);
}

void UserPreferencesStore::ClearValues() { configData.clear(); }

void UserPreferencesStore::RegisterVariable(const std::string &name,
                                            const std::string &type,
                                            float defVal, float minVal,
                                            float maxVal,
                                            std::vector<std::string> legacyNames) {
  VariableInfo info;
  info.type = type;
  info.defaultValue = defVal;
  info.value = defVal;
  info.minValue = minVal;
  info.maxValue = maxVal;
  info.legacyNames = std::move(legacyNames);
  variables[name] = info;
}

float UserPreferencesStore::GetFloat(const std::string &name) const {
  auto it = variables.find(name);
  float defVal = 0.0f;
  if (it != variables.end())
    defVal = it->second.defaultValue;

  auto valStr = GetValue(name);
  if (valStr) {
    float parsed = 0.0f;
    if (TryParseFloat(*valStr, parsed))
      return parsed;
    return defVal;
  }
  return defVal;
}

void UserPreferencesStore::SetFloat(const std::string &name, float v) {
  auto it = variables.find(name);
  if (it != variables.end()) {
    v = std::clamp(v, it->second.minValue, it->second.maxValue);
    it->second.value = v;
  }
  SetValue(name, std::to_string(v));
}

void UserPreferencesStore::ApplyDefaults() {
  for (const auto &[name, info] : variables) {
    float value = info.defaultValue;
    auto raw = GetValue(name);
    if (raw) {
      float parsed = 0.0f;
      if (TryParseFloat(*raw, parsed))
        value = std::clamp(parsed, info.minValue, info.maxValue);
    } else {
      for (const auto &legacy : info.legacyNames) {
        auto legacyRaw = GetValue(legacy);
        if (!legacyRaw)
          continue;
        float parsed = 0.0f;
        if (TryParseFloat(*legacyRaw, parsed)) {
          value = std::clamp(parsed, info.minValue, info.maxValue);
          break;
        }
      }
    }
    SetValue(name, std::to_string(value));
  }
}

void UserPreferencesStore::ApplyColumnDefaults() {
  if (!HasKey("fixture_print_columns"))
    SetValue("fixture_print_columns", "position,id,type");
  if (!HasKey("truss_print_columns"))
    SetValue("truss_print_columns", "position,type,length");
  if (!HasKey("support_print_columns"))
    SetValue("support_print_columns", "position,type,height");
  if (!HasKey("sceneobject_print_columns"))
    SetValue("sceneobject_print_columns", "position,name,type");
}

std::vector<std::string> UserPreferencesStore::GetFixturePrintColumns() const {
  auto val = GetValue("fixture_print_columns");
  if (val)
    return SplitCSV(*val);
  return {};
}

void UserPreferencesStore::SetFixturePrintColumns(
    const std::vector<std::string> &cols) {
  SetValue("fixture_print_columns", JoinCSV(cols));
}

std::vector<std::string> UserPreferencesStore::GetTrussPrintColumns() const {
  auto val = GetValue("truss_print_columns");
  if (val)
    return SplitCSV(*val);
  return {};
}

void UserPreferencesStore::SetTrussPrintColumns(
    const std::vector<std::string> &cols) {
  SetValue("truss_print_columns", JoinCSV(cols));
}

std::vector<std::string> UserPreferencesStore::GetSupportPrintColumns() const {
  auto val = GetValue("support_print_columns");
  if (val)
    return SplitCSV(*val);
  return {};
}

void UserPreferencesStore::SetSupportPrintColumns(
    const std::vector<std::string> &cols) {
  SetValue("support_print_columns", JoinCSV(cols));
}

std::vector<std::string>
UserPreferencesStore::GetSceneObjectPrintColumns() const {
  auto val = GetValue("sceneobject_print_columns");
  if (val)
    return SplitCSV(*val);
  return {};
}

void UserPreferencesStore::SetSceneObjectPrintColumns(
    const std::vector<std::string> &cols) {
  SetValue("sceneobject_print_columns", JoinCSV(cols));
}

bool UserPreferencesStore::LoadFromFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return false;

  nlohmann::json j;
  try {
    file >> j;
  } catch (...) {
    return false;
  }
  if (!j.is_object())
    return false;

  try {
    configData = j.get<std::unordered_map<std::string, std::string>>();
  } catch (...) {
    return false;
  }
  ApplyColumnDefaults();
  ApplyDefaults();
  return true;
}

bool UserPreferencesStore::SaveToFile(const std::string &path) const {
  std::ofstream file(path, std::ios::binary);
  if (!file.is_open())
    return false;

  nlohmann::json j(configData);
  file << j.dump(4);
  return true;
}

std::string UserPreferencesStore::GetUserConfigFile() {
  wxString dir = wxStandardPaths::Get().GetUserDataDir();
  std::filesystem::path p = std::filesystem::path(dir.ToStdString());
  std::filesystem::create_directories(p);
  p /= "user_config.json";
  return p.string();
}

bool UserPreferencesStore::LoadUserConfig() {
  return LoadFromFile(GetUserConfigFile());
}

bool UserPreferencesStore::SaveUserConfig() const {
  return SaveToFile(GetUserConfigFile());
}

const std::vector<std::string> &SelectionState::GetSelectedFixtures() const {
  return selectedFixtures;
}

void SelectionState::SetSelectedFixtures(const std::vector<std::string> &uuids) {
  selectedFixtures = uuids;
}

const std::vector<std::string> &SelectionState::GetSelectedTrusses() const {
  return selectedTrusses;
}

void SelectionState::SetSelectedTrusses(const std::vector<std::string> &uuids) {
  selectedTrusses = uuids;
}

const std::vector<std::string> &SelectionState::GetSelectedSupports() const {
  return selectedSupports;
}

void SelectionState::SetSelectedSupports(const std::vector<std::string> &uuids) {
  selectedSupports = uuids;
}

const std::vector<std::string> &SelectionState::GetSelectedSceneObjects() const {
  return selectedSceneObjects;
}

void SelectionState::SetSelectedSceneObjects(
    const std::vector<std::string> &uuids) {
  selectedSceneObjects = uuids;
}

void SelectionState::Clear() {
  selectedFixtures.clear();
  selectedTrusses.clear();
  selectedSupports.clear();
  selectedSceneObjects.clear();
}

void HistoryManager::PushUndoState(const MvrScene &scene,
                                   const SelectionState &selection,
                                   const std::string &description) {
  Snapshot snap{scene,
                selection.GetSelectedFixtures(),
                selection.GetSelectedTrusses(),
                selection.GetSelectedSupports(),
                selection.GetSelectedSceneObjects(),
                description};
  undoStack.push_back(std::move(snap));
  if (undoStack.size() > maxHistory)
    undoStack.erase(undoStack.begin());
  redoStack.clear();
}

bool HistoryManager::CanUndo() const { return !undoStack.empty(); }

bool HistoryManager::CanRedo() const { return !redoStack.empty(); }

std::string HistoryManager::Undo(MvrScene &scene, SelectionState &selection) {
  if (undoStack.empty())
    return {};
  const Snapshot snap = undoStack.back();
  redoStack.push_back({scene,
                       selection.GetSelectedFixtures(),
                       selection.GetSelectedTrusses(),
                       selection.GetSelectedSupports(),
                       selection.GetSelectedSceneObjects(),
                       snap.description});
  scene = snap.scene;
  selection.SetSelectedFixtures(snap.selFixtures);
  selection.SetSelectedTrusses(snap.selTrusses);
  selection.SetSelectedSupports(snap.selSupports);
  selection.SetSelectedSceneObjects(snap.selSceneObjects);
  undoStack.pop_back();
  return snap.description;
}

std::string HistoryManager::Redo(MvrScene &scene, SelectionState &selection) {
  if (redoStack.empty())
    return {};
  const Snapshot snap = redoStack.back();
  undoStack.push_back({scene,
                       selection.GetSelectedFixtures(),
                       selection.GetSelectedTrusses(),
                       selection.GetSelectedSupports(),
                       selection.GetSelectedSceneObjects(),
                       snap.description});
  scene = snap.scene;
  selection.SetSelectedFixtures(snap.selFixtures);
  selection.SetSelectedTrusses(snap.selTrusses);
  selection.SetSelectedSupports(snap.selSupports);
  selection.SetSelectedSceneObjects(snap.selSceneObjects);
  redoStack.pop_back();
  return snap.description;
}

void HistoryManager::ClearHistory() {
  undoStack.clear();
  redoStack.clear();
}

std::unordered_set<std::string> LayerVisibilityState::GetHiddenLayers() const {
  return hiddenLayers;
}

void LayerVisibilityState::SetHiddenLayers(
    const std::unordered_set<std::string> &layers) {
  hiddenLayers = layers;
}

bool LayerVisibilityState::IsLayerVisible(const std::string &layer) const {
  std::string name = layer.empty() ? DEFAULT_LAYER_NAME : layer;
  return hiddenLayers.find(name) == hiddenLayers.end();
}

void LayerVisibilityState::SetLayerColor(MvrScene &scene, const std::string &layer,
                                         const std::string &color) {
  std::string name = layer.empty() ? DEFAULT_LAYER_NAME : layer;
  std::string layerUuid;
  for (auto &[uuid, l] : scene.layers) {
    if (l.name == name) {
      layerUuid = uuid;
      break;
    }
  }
  if (layerUuid.empty()) {
    Layer l;
    l.name = name;
    l.color = color;
    l.uuid = "layer_" + std::to_string(scene.layers.size() + 1);
    scene.layers[l.uuid] = l;
  } else {
    scene.layers[layerUuid].color = color;
  }
}

std::optional<std::string>
LayerVisibilityState::GetLayerColor(const MvrScene &scene,
                                    const std::string &layer) const {
  std::string name = layer.empty() ? DEFAULT_LAYER_NAME : layer;
  for (const auto &[uuid, l] : scene.layers) {
    if (l.name == name && !l.color.empty())
      return l.color;
  }
  return std::nullopt;
}

std::vector<std::string>
LayerVisibilityState::GetLayerNames(const MvrScene &scene) const {
  std::set<std::string> names;
  for (const auto &[uuid, layer] : scene.layers)
    names.insert(layer.name);
  auto collect = [&](const std::string &ln) {
    if (!ln.empty())
      names.insert(ln);
  };
  for (const auto &[u, f] : scene.fixtures)
    collect(f.layer);
  for (const auto &[u, t] : scene.trusses)
    collect(t.layer);
  for (const auto &[u, s] : scene.supports)
    collect(s.layer);
  for (const auto &[u, o] : scene.sceneObjects)
    collect(o.layer);
  names.insert(DEFAULT_LAYER_NAME);
  return {names.begin(), names.end()};
}

const std::string &LayerVisibilityState::GetCurrentLayer() const {
  return currentLayer;
}

void LayerVisibilityState::SetCurrentLayer(const std::string &name) {
  if (name.empty())
    currentLayer = DEFAULT_LAYER_NAME;
  else
    currentLayer = name;
}

MvrScene &ProjectSession::GetScene() { return scene; }

const MvrScene &ProjectSession::GetScene() const { return scene; }

bool ProjectSession::SaveProject(const std::string &path,
                                 const SaveConfigFn &saveConfig,
                                 const SaveSceneFn &saveScene) const {
  namespace fs = std::filesystem;
  if (!saveConfig || !saveScene)
    return false;

  TempDir tempDir("PerastageProj_");
  if (!tempDir.Valid())
    return false;

  fs::path configPath = tempDir.Path() / "config.json";
  fs::path scenePath = tempDir.Path() / "scene.mvr";
  if (!saveConfig(configPath.string()) || !saveScene(scenePath.string()))
    return false;

  wxFileOutputStream out(path);
  if (!out.IsOk())
    return false;
  wxZipOutputStream zip(out);

  auto addFile = [&](const fs::path &source, const std::string &entryName) {
    auto *entry = new wxZipEntry(entryName);
    entry->SetMethod(wxZIP_METHOD_DEFLATE);
    zip.PutNextEntry(entry);
    std::ifstream in(source, std::ios::binary);
    char buf[4096];
    while (in.good()) {
      in.read(buf, sizeof(buf));
      std::streamsize s = in.gcount();
      if (s > 0)
        zip.Write(buf, s);
    }
    zip.CloseEntry();
  };

  addFile(configPath, "config.json");
  addFile(scenePath, "scene.mvr");
  zip.Close();
  return true;
}

bool ProjectSession::LoadProject(const std::string &path,
                                 const LoadConfigFn &loadConfig,
                                 const LoadSceneFn &loadScene) {
  namespace fs = std::filesystem;
  if (!loadConfig || !loadScene)
    return false;

  if (!LooksLikeZipFile(path)) {
    if (LooksLikeJsonFile(path))
      return loadConfig(path);
    return false;
  }

  wxFileInputStream in(path);
  if (!in.IsOk())
    return false;
  wxZipInputStream zip(in);

  TempDir tempDir("PerastageProj_");
  if (!tempDir.Valid())
    return false;

  std::unique_ptr<wxZipEntry> entry;
  fs::path configPath;
  fs::path scenePath;
  bool hasMvrSceneXml = false;

  while ((entry.reset(zip.GetNextEntry())), entry) {
    if (entry->IsDir())
      continue;
    std::string baseName =
        ToLowerCopy(fs::path(entry->GetName().ToStdString()).filename().string());
    fs::path outPath;
    if (baseName == "config.json")
      outPath = tempDir.Path() / "config.json";
    else if (baseName == "scene.mvr")
      outPath = tempDir.Path() / "scene.mvr";
    else {
      if (baseName == "generalscenedescription.xml")
        hasMvrSceneXml = true;
      continue;
    }

    std::ofstream out(outPath, std::ios::binary);
    char buf[4096];
    while (true) {
      zip.Read(buf, sizeof(buf));
      size_t bytes = zip.LastRead();
      if (bytes == 0)
        break;
      out.write(buf, bytes);
    }
    out.close();

    if (baseName == "config.json")
      configPath = outPath;
    else
      scenePath = outPath;
  }

  if (configPath.empty() && scenePath.empty()) {
    if (hasMvrSceneXml)
      return loadScene(path);
    if (LooksLikeJsonFile(path))
      return loadConfig(path);
    return false;
  }

  bool ok = true;
  if (!scenePath.empty())
    ok &= loadScene(scenePath.string());
  if (!configPath.empty())
    ok &= loadConfig(configPath.string());
  return ok;
}

bool ProjectSession::IsDirty() const { return revision != savedRevision; }

void ProjectSession::Touch() { ++revision; }

void ProjectSession::MarkSaved() { savedRevision = revision; }

void ProjectSession::ResetDirty() {
  revision = 0;
  savedRevision = 0;
}
