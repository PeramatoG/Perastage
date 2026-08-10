#include "tools/scoped_scene_replacement_lifecycle.h"

#include <cassert>
#include <stdexcept>

namespace {

// Models the shared renderer state that determines whether geometry can render.
struct FakeSharedRenderer {
  enum class Source { Project, Snapshot };

  Source source = Source::Project;
  bool replacementActive = false;
  int geometryCommands = 3;
  int projectRestores = 0;

  // Clears geometry and blocks rendering before the data source changes.
  void Prepare() {
    replacementActive = true;
    geometryCommands = 0;
  }

  // Resumes rendering and rebuilds geometry for the installed data source.
  void Complete() {
    replacementActive = false;
    geometryCommands = source == Source::Project ? 3 : 7;
  }

  // Refreshes the active project after replacement has completed.
  void RestoreProject() {
    assert(source == Source::Project);
    assert(!replacementActive);
    geometryCommands = 3;
    ++projectRestores;
  }
};

// Restores the fake project data source before the outer lifecycle guard exits.
class ScopedSnapshotSource {
public:
  // Installs the isolated source for one simulated capture slice.
  explicit ScopedSnapshotSource(FakeSharedRenderer &renderer)
      : renderer_(renderer) {
    renderer_.source = FakeSharedRenderer::Source::Snapshot;
  }

  // Restores the project source as SceneDataManager::ScopedSnapshot would.
  ~ScopedSnapshotSource() {
    renderer_.source = FakeSharedRenderer::Source::Project;
  }

private:
  FakeSharedRenderer &renderer_;
};

// Executes one incremental slice and verifies normal geometry after yielding.
void RunCaptureSlice(FakeSharedRenderer &renderer) {
  tools::ScopedSceneReplacementLifecycle lifecycle(
      [&renderer]() { renderer.Prepare(); },
      [&renderer]() { renderer.Complete(); },
      [&renderer]() { renderer.RestoreProject(); });
  ScopedSnapshotSource snapshot(renderer);
  lifecycle.CompleteReplacement();
  auto prepareOnExit = lifecycle.PrepareOnScopeExit();
  assert(!renderer.replacementActive);
  assert(renderer.geometryCommands == 7);
}

// Verifies exceptional capture exits still pair replacement before project
// restore.
void RunFailingCaptureSlice(FakeSharedRenderer &renderer) {
  try {
    tools::ScopedSceneReplacementLifecycle lifecycle(
        [&renderer]() { renderer.Prepare(); },
        [&renderer]() { renderer.Complete(); },
        [&renderer]() { renderer.RestoreProject(); });
    ScopedSnapshotSource snapshot(renderer);
    lifecycle.CompleteReplacement();
    auto prepareOnExit = lifecycle.PrepareOnScopeExit();
    throw std::runtime_error("simulated render failure");
  } catch (const std::runtime_error &) {
  }
}

} // namespace

// Verifies every cooperative capture yield restores renderable project
// geometry.
int main() {
  FakeSharedRenderer renderer;
  for (int view = 0; view < 4; ++view) {
    RunCaptureSlice(renderer);
    assert(!renderer.replacementActive);
    assert(renderer.source == FakeSharedRenderer::Source::Project);
    assert(renderer.geometryCommands > 0);
  }
  RunFailingCaptureSlice(renderer);
  assert(!renderer.replacementActive);
  assert(renderer.source == FakeSharedRenderer::Source::Project);
  assert(renderer.geometryCommands > 0);
  assert(renderer.projectRestores == 5);
  return 0;
}
