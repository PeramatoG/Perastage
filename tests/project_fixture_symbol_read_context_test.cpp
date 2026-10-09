#include "symbols/project_fixture_symbol_runtime.h"
#include "mvrscene.h"

#include <atomic>
#include <cassert>
#include <future>
#include <thread>

namespace {
using Store = symbols::ProjectFixtureSymbolStore;
using Snapshot = std::shared_ptr<const Store>;

symbols::ProjectFixtureSymbolBundle MakeBundle(int width) {
  symbols::ProjectFixtureSymbolBundle bundle;
  bundle.generatorVersion = "snapshot-test";
  for (auto &view : bundle.views) {
    view.svg = "<svg viewBox=\"0 0 " + std::to_string(width) +
               " 10\"><polygon points=\"0,0 5,0 5,5\"/></svg>";
    view.offsetXmm = width / 2.0;
  }
  return bundle;
}

void CheckSymbol(const std::string &source, double width) {
  const auto data = symbols::LoadProjectFixtureSymbolSource(
      source, SymbolViewKind::Bottom);
  assert(data && data->viewBoxWidth == width);
  assert(data->offsetXmm == width / 2.0);
}

struct Replacement {
  Snapshot snapshot;
  Fixture fixture;
  std::string source;
};
} // namespace

int main() {
  MvrScene scene;
  scene.fixtures["fixture"].uuid = "12345678-1234-5678-90ab-1234567890ab";
  Store activeStore;
  std::string error;
  assert(activeStore.ApplyToFixtures(scene, {"fixture"}, MakeBundle(10), error));
  const auto ownerThread = std::this_thread::get_id();
  std::atomic<int> backgroundProviderCalls = 0;
  symbols::InstallProjectFixtureSymbolProvider([&] {
    if (std::this_thread::get_id() != ownerThread)
      ++backgroundProviderCalls;
    return &activeStore;
  });
  const auto capturedFixture = scene.fixtures.at("fixture");
  const auto capturedSource = symbols::BuildProjectFixtureSymbolSource(capturedFixture);
  const auto capturedStore = std::make_shared<const Store>(activeStore);
  CheckSymbol(capturedSource, 10);

  std::promise<void> readerReady;
  auto readerReadyFuture = readerReady.get_future();
  std::promise<Replacement> replacementReady;
  auto replacementFuture = replacementReady.get_future();
  auto reader = std::async(std::launch::async, [&] {
    Replacement replacement;
    {
      symbols::ScopedProjectFixtureSymbolReadContext exportContext(capturedStore);
      readerReady.set_value();
      replacement = replacementFuture.get();
      // The active project has discarded the definition and invalidated caches.
      // The export resolves only its immutable data, without invoking its owner.
      CheckSymbol(capturedSource, 10);
      assert(symbols::BuildProjectFixtureSymbolSource(capturedFixture) == capturedSource);
      assert(!symbols::LoadProjectFixtureSymbolSource(
          replacement.source, SymbolViewKind::Bottom));
      {
        symbols::ScopedProjectFixtureSymbolReadContext nestedContext(replacement.snapshot);
        CheckSymbol(replacement.source, 20);
        assert(symbols::BuildProjectFixtureSymbolSource(replacement.fixture) == replacement.source);
        assert(!symbols::LoadProjectFixtureSymbolSource(capturedSource, SymbolViewKind::Bottom));
      }
      CheckSymbol(capturedSource, 10);
      {
        symbols::ScopedProjectFixtureSymbolReadContext emptyContext(nullptr);
        assert(symbols::BuildProjectFixtureSymbolSource(capturedFixture).empty());
        assert(!symbols::LoadProjectFixtureSymbolSource(capturedSource, SymbolViewKind::Bottom));
      }
      CheckSymbol(capturedSource, 10);
      assert(backgroundProviderCalls == 0);
    }
    // Leaving the scope restores normal active-project resolution on this thread.
    CheckSymbol(replacement.source, 20);
    assert(!symbols::LoadProjectFixtureSymbolSource(capturedSource, SymbolViewKind::Bottom));
    assert(backgroundProviderCalls > 0);
  });

  readerReadyFuture.get();
  activeStore.Clear();
  assert(activeStore.ApplyToFixtures(scene, {"fixture"}, MakeBundle(20), error));
  symbols::ClearProjectFixtureSymbolRuntimeCache();
  const auto activeFixture = scene.fixtures.at("fixture");
  const auto activeSource = symbols::BuildProjectFixtureSymbolSource(activeFixture);
  assert(activeSource != capturedSource);
  // The worker scope does not change which source is available to the UI.
  assert(!symbols::LoadProjectFixtureSymbolSource(capturedSource, SymbolViewKind::Bottom));
  CheckSymbol(activeSource, 20);
  replacementReady.set_value(
      {std::make_shared<const Store>(activeStore), activeFixture, activeSource});
  reader.get();
  CheckSymbol(activeSource, 20);
  assert(!symbols::LoadProjectFixtureSymbolSource(capturedSource, SymbolViewKind::Bottom));
  symbols::InstallProjectFixtureSymbolProvider({});
}
