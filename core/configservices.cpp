#include "configservices.h"
#include "filesystem_path_utils.h"
#include "apppaths.h"
#include "logger.h"

#include "json.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <memory>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <iterator>

#include <wx/stdpaths.h>
#include <wx/wfstream.h>
class wxZipStreamLink;
#include <wx/zipstrm.h>

namespace {
namespace fs = std::filesystem;

std::string Utf8StringFromPath(const fs::path &path) {
  const auto utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

// Creates a filesystem path from UTF-8 configuration text.
fs::path PathFromUtf8(const std::string &path) {
  if (path.empty())
    return {};
  return PathUtils::PathFromUtf8(path);
}

wxString WxStringFromPath(const fs::path &path) {
#ifdef _WIN32
  return wxString(path.wstring());
#else
  return wxString::FromUTF8(path.string());
#endif
}

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

// Parses a trimmed string into a float and requires full consumption without range errors.
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

  errno = 0;
  std::string trimmedText(trimmed);
  char *endPtr = nullptr;
  const double parsed = std::strtod(trimmedText.c_str(), &endPtr);
  if (endPtr == trimmedText.c_str() + trimmedText.size() && errno != ERANGE) {
    out = static_cast<float>(parsed);
    return true;
  }
  return false;
}

class TempDir {
public:
  explicit TempDir(const std::string &prefix) {
    auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    std::error_code ec;
    const fs::path base = fs::temp_directory_path(ec);
    if (ec)
      return;
    path = base / (prefix + std::to_string(stamp));
    created = fs::create_directory(path, ec);
    if (ec)
      created = false;
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
  std::ifstream file(PathFromUtf8(path), std::ios::binary);
  if (!file.is_open())
    return false;
  unsigned char signature[2] = {};
  file.read(reinterpret_cast<char *>(signature), sizeof(signature));
  return file.gcount() == static_cast<std::streamsize>(sizeof(signature)) &&
         signature[0] == 'P' && signature[1] == 'K';
}

bool LooksLikeJsonFile(const std::string &path) {
  std::ifstream file(PathFromUtf8(path), std::ios::binary);
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

// Selects the ZIP compression method for a project archive entry.
int SelectZipCompressionMethod(const std::string &entryName, size_t payloadSize);

// Returns true when a ZIP entry name is a packaged layout image resource.
bool IsLayoutImageResourceEntry(const std::string &entryName) {
  const std::string normalized = fs::path(entryName).generic_string();
  return normalized.rfind("resources/layout_images/", 0) == 0;
}

// Rejects archive paths that could escape the project resource extraction directory.
bool IsSafeArchiveRelativePath(const std::string &entryName) {
  if (entryName.empty() || entryName.find('\\') != std::string::npos ||
      (entryName.size() >= 2 && entryName[1] == ':'))
    return false;
  const fs::path path(entryName);
  if (path.empty() || path.is_absolute())
    return false;
  for (const auto &part : path) {
    if (part == "..")
      return false;
  }
  return true;
}

// Reads the active ZIP entry payload into the requested output file path.
bool ExtractCurrentZipEntry(wxZipInputStream &zip, const fs::path &outPath) {
  std::error_code ec;
  fs::create_directories(outPath.parent_path(), ec);
  if (ec)
    return false;

  std::ofstream out(outPath, std::ios::binary);
  if (!out.is_open())
    return false;
  char buf[4096];
  while (true) {
    zip.Read(buf, sizeof(buf));
    const size_t bytes = zip.LastRead();
    if (bytes == 0)
      break;
    out.write(buf, bytes);
  }
  return out.good();
}

// Writes one project resource payload to a ZIP entry with the selected compression method.
bool WriteZipBytes(wxZipOutputStream &zip, const std::string &entryName,
                   const std::vector<std::uint8_t> &bytes) {
  if (entryName.empty() || !IsSafeArchiveRelativePath(entryName))
    return false;
  auto *entry = new wxZipEntry();
  entry->SetName(wxString::FromUTF8(entryName.c_str()), wxPATH_UNIX);
  entry->SetMethod(SelectZipCompressionMethod(entryName, bytes.size()));
  if (!zip.PutNextEntry(entry))
    return false;
  if (!bytes.empty()) {
    zip.Write(bytes.data(), bytes.size());
    if (!zip.IsOk()) {
      zip.CloseEntry();
      return false;
    }
  }
  return zip.CloseEntry();
}

// Logs a project save failure with a consistent stage and optional entry name.
bool LogProjectSaveFailure(const std::string &stage, const std::string &message,
                           const std::string &entryName = {}) {
  std::ostringstream out;
  out << "Project save failed at " << stage << ": " << message;
  if (!entryName.empty())
    out << " (entry=" << entryName << ")";
  Logger::Instance().Log(Logger::Level::Error, out.str());
  return false;
}

// Updates layout image paths inside config.json to point at extracted packaged resources.
bool PatchExtractedProjectResourcePaths(const fs::path &configPath,
                                        const fs::path &resourceDir) {
  std::ifstream in(configPath, std::ios::binary);
  if (!in.is_open())
    return false;

  nlohmann::json config;
  try {
    in >> config;
  } catch (...) {
    return false;
  }
  if (!config.is_object())
    return false;

  auto layoutsIt = config.find("layouts_collection");
  if (layoutsIt == config.end() || !layoutsIt->is_string())
    return true;

  nlohmann::json layoutDoc;
  try {
    layoutDoc = nlohmann::json::parse(layoutsIt->get<std::string>());
  } catch (...) {
    return true;
  }

  bool changed = false;
  auto layoutsItDoc = layoutDoc.find("layouts");
  if (layoutsItDoc != layoutDoc.end() && layoutsItDoc->is_array()) {
    for (auto &layout : *layoutsItDoc) {
      auto imagesIt = layout.find("imageViews");
      if (imagesIt == layout.end() || !imagesIt->is_array())
        continue;
      for (auto &image : *imagesIt) {
        auto resourceIt = image.find("projectResource");
        if (resourceIt == image.end() || !resourceIt->is_string())
          continue;
        const std::string archivePath = resourceIt->get<std::string>();
        if (!IsSafeArchiveRelativePath(archivePath))
          continue;
        image["path"] = Utf8StringFromPath(resourceDir / fs::path(archivePath));
        changed = true;
      }
    }
  }

  if (!changed)
    return true;

  config["layouts_collection"] = layoutDoc.dump();
  std::ofstream out(configPath, std::ios::binary | std::ios::trunc);
  if (!out.is_open())
    return false;
  out << config.dump(2);
  return out.good();
}

// Chooses ZIP compression mode from entry extension and payload size heuristics.
int SelectZipCompressionMethod(const std::string &entryName, size_t payloadSize) {
  const std::string ext = fs::path(ToLowerCopy(entryName)).extension().string();
  if (ext == ".mvr" || ext == ".gdtf" || ext == ".png" || ext == ".jpg" ||
      ext == ".jpeg" || ext == ".zip" || ext == ".gz" || ext == ".7z" ||
      ext == ".pdf")
    return wxZIP_METHOD_STORE;
  if (payloadSize < 96)
    return wxZIP_METHOD_STORE;
  return wxZIP_METHOD_DEFLATE;
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
  std::ifstream file(PathFromUtf8(path), std::ios::binary);
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

// Saves the current preference key-value map to a JSON file on disk.
bool UserPreferencesStore::SaveToFile(const std::string &path) const {
  std::ofstream file(PathFromUtf8(path), std::ios::binary);
  if (!file.is_open())
    return false;

  return SaveToStream(file);
}

// Serializes the current preference key-value map as pretty JSON into a stream.
bool UserPreferencesStore::SaveToStream(std::ostream &out) const {
  nlohmann::json j(configData);
  out << j.dump(4);
  return out.good();
}

std::string UserPreferencesStore::GetUserConfigFile() {
  // Builds the user preferences file path and falls back to a temp directory when needed.
  std::filesystem::path p = AppPaths::GetUserDataDir();
  std::error_code ec;
  std::filesystem::create_directories(p, ec);
  if (ec) {
    ec.clear();
    p = AppPaths::GetUserDataTempFallbackDir();
    std::filesystem::create_directories(p, ec);
    if (ec)
      return {};
  }
  p /= "user_config.json";
  return Utf8StringFromPath(p);
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

void HistoryManager::PushUndoState(
    const MvrScene &scene, const SelectionState &selection,
    const std::string &description,
    const std::optional<std::string> &layoutsCollection,
    const LayerVisibilityState *layerState,
    const std::optional<std::string> &fixtureLabelOverrides) {
  Snapshot snap{scene,
                selection.GetSelectedFixtures(),
                selection.GetSelectedTrusses(),
                selection.GetSelectedSupports(),
                selection.GetSelectedSceneObjects(),
                description,
                layoutsCollection,
                fixtureLabelOverrides,
                layerState ? layerState->GetHiddenLayers() : std::unordered_set<std::string>{},
                layerState ? layerState->GetCurrentLayer() : std::string{}};
  undoStack.push_back(std::move(snap));
  if (undoStack.size() > maxHistory)
    undoStack.erase(undoStack.begin());
  redoStack.clear();
}

bool HistoryManager::CanUndo() const { return !undoStack.empty(); }

bool HistoryManager::CanRedo() const { return !redoStack.empty(); }

std::string HistoryManager::Undo(
    MvrScene &scene, SelectionState &selection,
    std::optional<std::string> *layoutsCollection,
    LayerVisibilityState *layerState,
    std::optional<std::string> *fixtureLabelOverrides) {
  if (undoStack.empty())
    return {};
  const Snapshot snap = undoStack.back();
  redoStack.push_back({scene,
                       selection.GetSelectedFixtures(),
                       selection.GetSelectedTrusses(),
                       selection.GetSelectedSupports(),
                       selection.GetSelectedSceneObjects(),
                       snap.description,
                       snap.layoutsCollection,
                       fixtureLabelOverrides ? *fixtureLabelOverrides
                                             : snap.fixtureLabelOverrides,
                       layerState ? layerState->GetHiddenLayers() : std::unordered_set<std::string>{},
                       layerState ? layerState->GetCurrentLayer() : std::string{}});
  scene = snap.scene;
  if (layoutsCollection)
    *layoutsCollection = snap.layoutsCollection;
  if (fixtureLabelOverrides)
    *fixtureLabelOverrides = snap.fixtureLabelOverrides;
  selection.SetSelectedFixtures(snap.selFixtures);
  selection.SetSelectedTrusses(snap.selTrusses);
  selection.SetSelectedSupports(snap.selSupports);
  selection.SetSelectedSceneObjects(snap.selSceneObjects);
  if (layerState) {
    layerState->SetHiddenLayers(snap.hiddenLayers);
    if (!snap.currentLayer.empty())
      layerState->SetCurrentLayer(snap.currentLayer);
  }
  undoStack.pop_back();
  return snap.description;
}

std::string HistoryManager::Redo(
    MvrScene &scene, SelectionState &selection,
    std::optional<std::string> *layoutsCollection,
    LayerVisibilityState *layerState,
    std::optional<std::string> *fixtureLabelOverrides) {
  if (redoStack.empty())
    return {};
  const Snapshot snap = redoStack.back();
  undoStack.push_back({scene,
                       selection.GetSelectedFixtures(),
                       selection.GetSelectedTrusses(),
                       selection.GetSelectedSupports(),
                       selection.GetSelectedSceneObjects(),
                       snap.description,
                       snap.layoutsCollection,
                       fixtureLabelOverrides ? *fixtureLabelOverrides
                                             : snap.fixtureLabelOverrides,
                       layerState ? layerState->GetHiddenLayers() : std::unordered_set<std::string>{},
                       layerState ? layerState->GetCurrentLayer() : std::string{}});
  scene = snap.scene;
  if (layoutsCollection)
    *layoutsCollection = snap.layoutsCollection;
  if (fixtureLabelOverrides)
    *fixtureLabelOverrides = snap.fixtureLabelOverrides;
  selection.SetSelectedFixtures(snap.selFixtures);
  selection.SetSelectedTrusses(snap.selTrusses);
  selection.SetSelectedSupports(snap.selSupports);
  selection.SetSelectedSceneObjects(snap.selSceneObjects);
  if (layerState) {
    layerState->SetHiddenLayers(snap.hiddenLayers);
    if (!snap.currentLayer.empty())
      layerState->SetCurrentLayer(snap.currentLayer);
  }
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
  for (auto &[uuid, l] : scene.layers) {
    (void)uuid;
    if (l.name == name) {
      l.color = color;
      return;
    }
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
  for (const auto &[u, g] : scene.groupObjects)
    collect(g.layer);
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

// Removes the resource extraction directory owned by this project session.
ProjectSession::~ProjectSession() { ClearExtractedResourceDirectory(); }

// Deletes any previously extracted packaged resources for this session.
void ProjectSession::ClearExtractedResourceDirectory() {
  if (extractedResourceDirectory.empty())
    return;
  std::error_code ec;
  fs::remove_all(PathFromUtf8(extractedResourceDirectory), ec);
  extractedResourceDirectory.clear();
}

// Creates a persistent temporary directory for resources extracted from a project package.
bool ProjectSession::CreateExtractedResourceDirectory() {
  ClearExtractedResourceDirectory();
  auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
  std::error_code ec;
  const fs::path base = fs::temp_directory_path(ec);
  if (ec)
    return false;
  const fs::path path = base / ("psp_resources_" + std::to_string(stamp));
  if (!fs::create_directories(path, ec) || ec)
    return false;
  extractedResourceDirectory = Utf8StringFromPath(path);
  return true;
}

MvrScene &ProjectSession::GetScene() { return scene; }

const MvrScene &ProjectSession::GetScene() const { return scene; }

// Saves a project package by serializing config and scene directly into ZIP entries.
bool ProjectSession::SaveProject(
    const std::string &path, const SaveConfigToBufferFn &saveConfigToBuffer,
    const SaveSceneToBufferFn &saveSceneToBuffer) const {
  return SaveProject(path, saveConfigToBuffer, saveSceneToBuffer,
                     CollectArchiveResourcesFromSceneFn{});
}

// Saves a project package with additional resource entries supplied by the caller.
bool ProjectSession::SaveProject(
    const std::string &path, const SaveConfigToBufferFn &saveConfigToBuffer,
    const SaveSceneToBufferFn &saveSceneToBuffer,
    const CollectArchiveResourcesFn &collectResources) const {
  CollectArchiveResourcesFromSceneFn adapter;
  if (collectResources) {
    adapter = [collectResources](const std::vector<std::uint8_t> &,
                                 std::vector<ArchiveResource> &resources,
                                 std::string &) {
      try {
        resources = collectResources();
      } catch (const std::exception &error) {
        Logger::Instance().Log(Logger::Level::Warn,
                               std::string("Optional project resources skipped: ") +
                                   error.what());
        resources.clear();
      } catch (...) {
        Logger::Instance().Log(Logger::Level::Warn,
                               "Optional project resources skipped: unknown error");
        resources.clear();
      }
      return true;
    };
  }
  return SaveProject(path, saveConfigToBuffer, saveSceneToBuffer, adapter);
}

// Saves a project package with resources transactionally derived from scene.mvr bytes.
bool ProjectSession::SaveProject(
    const std::string &path, const SaveConfigToBufferFn &saveConfigToBuffer,
    const SaveSceneToBufferFn &saveSceneToBuffer,
    const CollectArchiveResourcesFromSceneFn &collectResources) const {
  if (!saveConfigToBuffer || !saveSceneToBuffer)
    return LogProjectSaveFailure("ValidateCallbacks",
                                 "missing config or scene serializer");

  const auto stageStart = std::chrono::steady_clock::now();
  const fs::path targetPath = PathFromUtf8(path);
  const fs::path tempPath = targetPath.string() + ".tmp";
  std::error_code cleanupEc;
  fs::remove(tempPath, cleanupEc);
  struct TempArchiveCleanup {
    fs::path path;
    bool keep = false;
    ~TempArchiveCleanup() {
      if (!keep) {
        std::error_code ec;
        fs::remove(path, ec);
      }
    }
  } tempCleanup{tempPath, false};
  wxFileOutputStream out(WxStringFromPath(tempPath));
  if (!out.IsOk())
    return LogProjectSaveFailure("OpenOutput", "could not open temporary project archive",
                                 tempPath.string());
  wxZipOutputStream zip(out);

  const auto configStart = std::chrono::steady_clock::now();
  std::vector<uint8_t> configBytes;
  if (!saveConfigToBuffer(configBytes))
    return LogProjectSaveFailure("SerializeConfig", "config serialization failed");
  if (configBytes.empty())
    return LogProjectSaveFailure("SerializeConfig", "config.json payload is empty");
  auto *configEntry = new wxZipEntry("config.json");
  configEntry->SetMethod(
      SelectZipCompressionMethod("config.json", configBytes.size()));
  if (!zip.PutNextEntry(configEntry))
    return LogProjectSaveFailure("WriteConfigEntry", "could not create ZIP entry",
                                 "config.json");
  if (!configBytes.empty()) {
    zip.Write(configBytes.data(), configBytes.size());
    if (!zip.IsOk()) {
      zip.CloseEntry();
      return LogProjectSaveFailure("WriteConfigEntry", "could not write entry bytes",
                                   "config.json");
    }
  }
  if (!zip.CloseEntry())
    return LogProjectSaveFailure("WriteConfigEntry", "could not close ZIP entry",
                                 "config.json");
  const auto configEnd = std::chrono::steady_clock::now();

  const auto sceneStart = std::chrono::steady_clock::now();
  std::vector<uint8_t> sceneBytes;
  if (!saveSceneToBuffer(sceneBytes))
    return LogProjectSaveFailure("SerializeScene", "scene MVR serialization failed");
  if (sceneBytes.empty())
    return LogProjectSaveFailure("SerializeScene", "scene.mvr payload is empty");
  auto *sceneEntry = new wxZipEntry("scene.mvr");
  sceneEntry->SetMethod(
      SelectZipCompressionMethod("scene.mvr", sceneBytes.size()));
  if (!zip.PutNextEntry(sceneEntry))
    return LogProjectSaveFailure("WriteSceneEntry", "could not create ZIP entry",
                                 "scene.mvr");
  if (!sceneBytes.empty()) {
    zip.Write(sceneBytes.data(), sceneBytes.size());
    if (!zip.IsOk()) {
      zip.CloseEntry();
      return LogProjectSaveFailure("WriteSceneEntry", "could not write entry bytes",
                                   "scene.mvr");
    }
  }
  if (!zip.CloseEntry())
    return LogProjectSaveFailure("WriteSceneEntry", "could not close ZIP entry",
                                 "scene.mvr");
  const auto sceneEnd = std::chrono::steady_clock::now();

  const auto resourcesStart = std::chrono::steady_clock::now();
  std::vector<ArchiveResource> resourceEntries;
  if (collectResources) {
    std::string resourceError;
    try {
      if (!collectResources(sceneBytes, resourceEntries, resourceError)) {
        return LogProjectSaveFailure(
            "CollectRequiredResources",
            resourceError.empty() ? "transactional resource collection failed"
                                  : resourceError);
      }
    } catch (const std::exception &e) {
      return LogProjectSaveFailure("CollectRequiredResources", e.what());
    } catch (...) {
      return LogProjectSaveFailure("CollectRequiredResources", "unknown error");
    }
  }
  std::unordered_set<std::string> writtenResourceNames = {
      "config.json", "scene.mvr"};
  for (const auto &resource : resourceEntries) {
    const std::string resourceIdentity = ToLowerCopy(resource.entryName);
    if (!writtenResourceNames.insert(resourceIdentity).second) {
      if (resource.required) {
        return LogProjectSaveFailure("WriteRequiredResource",
                                     "transactional resource path is duplicated",
                                     resource.entryName);
      }
      Logger::Instance().Log(Logger::Level::Warn,
                             "Skipping duplicate optional project resource '" +
                                 resource.entryName + "'.");
      continue;
    }
    if (!WriteZipBytes(zip, resource.entryName, resource.bytes)) {
      if (resource.required) {
        return LogProjectSaveFailure("WriteRequiredResource",
                                     "could not write transactional resource",
                                     resource.entryName);
      }
      Logger::Instance().Log(Logger::Level::Warn,
                             "Skipping optional project resource '" +
                                 resource.entryName + "' because it could not be written.");
    }
  }
  const auto resourcesEnd = std::chrono::steady_clock::now();

  const auto finalizeStart = std::chrono::steady_clock::now();
  if (!zip.Close())
    return LogProjectSaveFailure("FinalizeArchive", "could not close project ZIP",
                                 tempPath.string());
  std::error_code replaceEc;
  fs::rename(tempPath, targetPath, replaceEc);
  if (replaceEc) {
    replaceEc.clear();
    fs::copy_file(tempPath, targetPath, fs::copy_options::overwrite_existing,
                  replaceEc);
    fs::remove(tempPath, cleanupEc);
    if (replaceEc)
      return LogProjectSaveFailure("FinalizeArchive",
                                   "could not replace target project archive",
                                   targetPath.string());
  }
  tempCleanup.keep = true;
  const auto finalizeEnd = std::chrono::steady_clock::now();
  const std::uintmax_t archiveBytes = fs::file_size(targetPath);

  auto elapsedMs = [](const auto &start, const auto &end) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
  };
  std::ostringstream timing;
  timing << "ProjectSession::SaveProject timings [ms] config="
         << elapsedMs(configStart, configEnd)
         << ", scene=" << elapsedMs(sceneStart, sceneEnd)
         << ", resources=" << elapsedMs(resourcesStart, resourcesEnd)
         << ", finalize=" << elapsedMs(finalizeStart, finalizeEnd)
         << ", total=" << elapsedMs(stageStart, finalizeEnd)
         << " | sizes [bytes] config=" << configBytes.size()
         << ", scene=" << sceneBytes.size()
         << ", resources=" << resourceEntries.size()
         << ", archive=" << archiveBytes;
  Logger::Instance().Log(Logger::Level::Info, timing.str());

  return true;
}

// Saves a project package using legacy file-path callbacks for backward compatibility.
bool ProjectSession::SaveProject(const std::string &path,
                                 const SaveConfigFn &saveConfig,
                                 const SaveSceneFn &saveScene) const {
  if (!saveConfig || !saveScene)
    return false;

  TempDir tempDir("psp_");
  if (!tempDir.Valid())
    return false;

  return SaveProject(
      path,
      [&](std::vector<uint8_t> &out) {
        const fs::path configPath = tempDir.Path() / "config.json";
        if (!saveConfig(configPath.string()))
          return false;
        std::ifstream in(configPath, std::ios::binary);
        if (!in.is_open())
          return false;
        out.assign(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());
        return in.good() || in.eof();
      },
      [&](std::vector<uint8_t> &out) {
        const fs::path scenePath = tempDir.Path() / "scene.mvr";
        if (!saveScene(scenePath.string()))
          return false;
        std::ifstream in(scenePath, std::ios::binary);
        if (!in.is_open())
          return false;
        out.assign(std::istreambuf_iterator<char>(in),
                   std::istreambuf_iterator<char>());
        return in.good() || in.eof();
      });
}

bool ProjectSession::LoadProject(const std::string &path,
                                 const LoadConfigFn &loadConfig,
                                 const LoadSceneFn &loadScene,
                                 const LoadProgressFn &progress) {
  if (!loadConfig || !loadScene)
    return false;
  loadedArchiveResources.clear();

  auto reportProgress = [&](const std::string &stage, int completed = 0,
                            int total = 0) {
    if (progress)
      progress(stage, completed, total);
  };

  reportProgress("Opening project package...");

  if (!LooksLikeZipFile(path)) {
    if (LooksLikeJsonFile(path))
      return loadConfig(path);
    return false;
  }

  wxFileInputStream in(WxStringFromPath(PathFromUtf8(path)));
  if (!in.IsOk())
    return false;
  wxZipInputStream zip(in);

  TempDir tempDir("psp_");
  if (!tempDir.Valid())
    return false;
  if (!CreateExtractedResourceDirectory())
    return false;
  const fs::path resourceDir = PathFromUtf8(extractedResourceDirectory);

  std::unique_ptr<wxZipEntry> entry;
  fs::path configPath;
  fs::path scenePath;
  bool hasMvrSceneXml = false;
  int extractedRelevantEntries = 0;
  size_t transferredCacheBytes = 0;

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
      const std::string entryName = fs::path(entry->GetName().ToStdString()).generic_string();
      if (baseName == "generalscenedescription.xml")
        hasMvrSceneXml = true;
      if (IsLayoutImageResourceEntry(entryName) &&
          IsSafeArchiveRelativePath(entryName)) {
        ExtractCurrentZipEntry(zip, resourceDir / fs::path(entryName));
      } else if (entryName.rfind("resources/layout_view_cache/", 0) == 0 &&
                 IsSafeArchiveRelativePath(entryName)) {
        constexpr size_t kMaxTransferredCacheEntryBytes = 8 * 1024 * 1024;
        std::vector<std::uint8_t> bytes;
        std::array<char, 4096> buffer{};
        while (bytes.size() <= kMaxTransferredCacheEntryBytes) {
          zip.Read(buffer.data(), buffer.size());
          const size_t count = zip.LastRead();
          if (count == 0)
            break;
          bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + count);
        }
        constexpr size_t kMaxTransferredCacheBytes = 24 * 1024 * 1024;
        if (bytes.size() <= kMaxTransferredCacheEntryBytes &&
            transferredCacheBytes + bytes.size() <=
                kMaxTransferredCacheBytes) {
          transferredCacheBytes += bytes.size();
          loadedArchiveResources.push_back({entryName, std::move(bytes)});
        }
      }
      continue;
    }

    if (!ExtractCurrentZipEntry(zip, outPath))
      return false;

    if (baseName == "config.json")
      configPath = outPath;
    else
      scenePath = outPath;

    ++extractedRelevantEntries;
    reportProgress("Extracting project package...", extractedRelevantEntries,
                   2);
  }

  if (configPath.empty() && scenePath.empty()) {
    if (hasMvrSceneXml)
      return loadScene(path);
    if (LooksLikeJsonFile(path))
      return loadConfig(path);
    return false;
  }

  bool ok = true;
  if (!scenePath.empty()) {
    reportProgress("Importing project scene...");
    const bool sceneOk = loadScene(scenePath.string());
    if (!sceneOk) {
      std::cerr << "ProjectSession::LoadProject failed while loading scene.mvr from extracted project package." << std::endl;
    }
    ok &= sceneOk;
  }
  if (!configPath.empty()) {
    if (!PatchExtractedProjectResourcePaths(configPath, resourceDir))
      return false;
    reportProgress("Loading project configuration...");
    const bool configOk = loadConfig(configPath.string());
    if (!configOk) {
      std::cerr << "ProjectSession::LoadProject failed while loading config.json from extracted project package." << std::endl;
    }
    ok &= configOk;
  }
  return ok;
}

// Returns optional project-owned resources captured during the primary archive traversal.
const std::vector<ProjectSession::ArchiveResource> &
ProjectSession::GetLoadedArchiveResources() const {
  return loadedArchiveResources;
}

bool ProjectSession::IsDirty() const { return revision != savedRevision; }

// Captures the project revision counters so rollback can preserve dirty state.
ProjectSession::DirtyState ProjectSession::CaptureDirtyState() const {
  return DirtyState{revision, savedRevision};
}

// Restores project revision counters after a cancelled or failed operation.
void ProjectSession::RestoreDirtyState(const DirtyState &state) {
  revision = state.revision;
  savedRevision = state.savedRevision;
}

void ProjectSession::Touch() { ++revision; }

void ProjectSession::MarkSaved() { savedRevision = revision; }

void ProjectSession::ResetDirty() {
  revision = 0;
  savedRevision = 0;
}
