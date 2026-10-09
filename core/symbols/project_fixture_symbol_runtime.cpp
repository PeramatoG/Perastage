#include "project_fixture_symbol_runtime.h"

#include "fixture.h"

#include <map>
#include <mutex>
#include <utility>

namespace symbols {
namespace {
constexpr std::string_view kSourcePrefix = "pstg-symbol:";
constexpr size_t kMaxCachedViews = 256;
thread_local std::shared_ptr<const ProjectFixtureSymbolStore> readSnapshot;
thread_local bool readContextActive = false;
struct RuntimeState {
  std::recursive_mutex mutex;
  ProjectFixtureSymbolProvider provider;
  std::map<std::pair<std::string, SymbolViewKind>,
           std::shared_ptr<const PerastageSvgSymbolData>> cache;
};
RuntimeState &State() {
  static RuntimeState state;
  return state;
}

const ProjectFixtureSymbolStore *CurrentStore() {
  if (readContextActive)
    return readSnapshot.get();
  const auto &provider = State().provider;
  return provider ? provider() : nullptr;
}

const ProjectFixtureSymbolBundle *FindBundle(const std::string &source) {
  const auto separator = source.rfind(':');
  if (!IsProjectFixtureSymbolSource(source) ||
      separator <= kSourcePrefix.size() || separator + 1 >= source.size())
    return nullptr;
  const auto *store = CurrentStore();
  if (!store)
    return nullptr;
  const auto definition = source.substr(kSourcePrefix.size(),
                                        separator - kSourcePrefix.size());
  if (store->ContentIdForDefinition(definition) != source.substr(separator + 1))
    return nullptr;
  return store->FindForDefinition(definition);
}

const char *ViewName(SymbolViewKind view) {
  switch (view) {
  case SymbolViewKind::Bottom: return "bottom";
  case SymbolViewKind::Front: return "front";
  case SymbolViewKind::Left:
  case SymbolViewKind::Right: return "side";
  default: return "top";
  }
}
} // namespace

ScopedProjectFixtureSymbolReadContext::ScopedProjectFixtureSymbolReadContext(
    std::shared_ptr<const ProjectFixtureSymbolStore> snapshot)
    : previousSnapshot_(std::move(readSnapshot)),
      previousContextActive_(readContextActive) {
  readSnapshot = std::move(snapshot);
  readContextActive = true;
}

ScopedProjectFixtureSymbolReadContext::~ScopedProjectFixtureSymbolReadContext() {
  readSnapshot = std::move(previousSnapshot_);
  readContextActive = previousContextActive_;
}

void InstallProjectFixtureSymbolProvider(ProjectFixtureSymbolProvider provider) {
  auto &state = State();
  std::lock_guard lock(state.mutex);
  state.provider = std::move(provider);
  state.cache.clear();
}

bool IsProjectFixtureSymbolSource(std::string_view source) {
  return source.starts_with(kSourcePrefix);
}

std::string BuildProjectFixtureSymbolSource(const Fixture &fixture) {
  if (fixture.projectSymbolDefinitionId.empty())
    return {};
  auto &state = State();
  std::lock_guard lock(state.mutex);
  const auto *store = CurrentStore();
  if (!store || !store->FindForFixture(fixture))
    return {};
  const auto content = store->ContentIdForDefinition(fixture.projectSymbolDefinitionId);
  return std::string(kSourcePrefix) + fixture.projectSymbolDefinitionId + ":" + content;
}

std::shared_ptr<const PerastageSvgSymbolData> LoadProjectFixtureSymbolSource(
    const std::string &source, SymbolViewKind view, std::string *errorDetails) {
  auto &state = State();
  std::lock_guard lock(state.mutex);
  const auto *bundle = FindBundle(source);
  if (!bundle) {
    if (errorDetails)
      *errorDetails = "The persisted project fixture symbol binding is unavailable.";
    return {};
  }
  const auto key = std::pair{source, view};
  if (const auto cached = state.cache.find(key); cached != state.cache.end()) {
    if (errorDetails)
      errorDetails->clear();
    return cached->second;
  }
  const auto storedView = view == SymbolViewKind::Right ? SymbolViewKind::Left : view;
  const auto *payload = view == SymbolViewKind::Back ? nullptr : bundle->FindView(storedView);
  bool fallback = false;
  if (!payload) {
    payload = bundle->FindView(SymbolViewKind::Top);
    fallback = true;
  }
  auto parsed = std::make_shared<PerastageSvgSymbolData>();
  if (!payload || !ParsePerastageProjectSvgSymbol(payload->svg, *parsed, errorDetails))
    return {};
  parsed->sourcePath = source + "/" + ViewName(payload->viewKind) + ".svg";
  parsed->viewKind = view == SymbolViewKind::Right && storedView == payload->viewKind
                         ? view : payload->viewKind;
  parsed->resourceSet = FixtureSymbolResourceSet::InternalRendering;
  parsed->provenance = bundle->kind == ProjectFixtureSymbolKind::UserOverride
                          ? FixtureSymbolProvenance::ProjectUserOverride
                          : FixtureSymbolProvenance::GeneratedPerastage;
  parsed->offsetXmm = payload->offsetXmm;
  parsed->offsetYmm = payload->offsetYmm;
  parsed->usedViewFallback = fallback;
  if (state.cache.size() >= kMaxCachedViews)
    state.cache.erase(state.cache.begin());
  state.cache.emplace(key, parsed);
  return parsed;
}

bool InspectProjectFixtureSymbolSource(
    const std::string &source, FixtureSymbolResourceInspection &inspection) {
  auto &state = State();
  std::lock_guard lock(state.mutex);
  const auto *bundle = FindBundle(source);
  inspection = {};
  bool complete = true;
  for (auto &resource : inspection.perastageViews) {
    std::string error;
    const auto *payload = bundle ? bundle->FindView(resource.viewKind) : nullptr;
    const auto parsed = LoadProjectFixtureSymbolSource(source, resource.viewKind, &error);
    resource.resourceSet = FixtureSymbolResourceSet::InternalRendering;
    resource.standardGdtf = false;
    resource.exists = payload != nullptr;
    resource.usable = parsed && !parsed->usedViewFallback;
    resource.diagnostic = error;
    if (payload)
      resource.archivePath = source + "/" + ViewName(payload->viewKind) + ".svg";
    if (parsed && resource.exists) {
      resource.archivePath = parsed->sourcePath;
      resource.provenance = parsed->provenance;
      resource.offsetXmm = parsed->offsetXmm;
      resource.offsetYmm = parsed->offsetYmm;
    }
    complete = complete && resource.usable;
  }
  inspection.perastageViewsUsable = complete;
  if (!complete)
    inspection.diagnostic = "The persisted project fixture symbol is unavailable or unusable.";
  return complete;
}

void ClearProjectFixtureSymbolRuntimeCache() {
  auto &state = State();
  std::lock_guard lock(state.mutex);
  state.cache.clear();
}
} // namespace symbols
