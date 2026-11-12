#include "services/fixture_symbol_preparation_service.h"
#include "guiconfigservices.h"
#include "mainwindow.h"
#include "projectutils.h"
#include "symbols/fixture_symbol_application.h"
#include "symbols/fixture_symbol_operation_counters.h"
#include "symbols/fixture_symbol_preparation_requests.h"
#include "tools/scene_model_symbol_capture_service.h"
#include "tools/symbol_physical_calibration.h"
#include "diagnostics/DiagnosticLogger.h"

#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <wx/app.h>
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {
std::mutex mutex;
std::condition_variable condition;
bool released = false;
bool entered = false;
int captures = 0;
int inspections = 0;
int publications = 0;
std::atomic<int> processing = 0;
TestGuiConfigServices config;
}
TestGuiConfigServices &GetDefaultGuiConfigServices() { return config; }
namespace ProjectUtils {
std::string GetDefaultLibraryPath(const std::string &) { return "/missing-library"; }
}
void diagnostics::DiagnosticLogger::Info(const std::string &) {}
void diagnostics::DiagnosticLogger::Warning(const std::string &) {}

namespace symbols {
const ProjectFixtureSymbolBundle *ProjectFixtureSymbolStore::FindForFixture(
    const Fixture &fixture) const {
  static const ProjectFixtureSymbolBundle persisted;
  return fixture.projectSymbolDefinitionId.empty() ? nullptr : &persisted;
}
FixtureSymbolApplicationResult InheritFixtureProjectSymbols(
    MvrScene &, ProjectFixtureSymbolStore &, const std::string &) {
  FixtureSymbolApplicationResult result;
  result.success = true;
  return result;
}
bool NeedsAutomaticFixtureSymbolPreparation(const Fixture &fixture,
    const ProjectFixtureSymbolStore &, const std::string &, gdtf::MutationPolicy) {
  return fixture.projectSymbolDefinitionId.empty();
}
FixtureSymbolApplicationResult PrepareFixtureSymbols(MvrScene &scene,
    ProjectFixtureSymbolStore &, const std::string &uuid, const std::string &,
    const std::vector<Symbol2D> &, gdtf::MutationPolicy) {
  FixtureSymbolApplicationResult result;
  result.success = true;
  if (scene.fixtures.at(uuid).projectSymbolDefinitionId.empty()) {
    ++publications;
    scene.fixtures.at(uuid).projectSymbolDefinitionId = "generated";
    result.projectSymbolsUpdated = true;
  }
  return result;
}
FixtureSymbolApplicationResult ApplyRuntimeFixtureSymbolPlaceholder(MvrScene &,
    ProjectFixtureSymbolStore &, const std::string &, const std::string &) { return {}; }
}
namespace tools {
bool ComputeFixtureGeometryBoundsMm(const std::string &, const std::string &,
                                    FixtureGeometryBounds &bounds, std::string &) {
  ++inspections;
  symbols::diagnostics::Count(symbols::diagnostics::Operation::GeometryInspection);
  bounds.valid = true;
  return true;
}
SceneModelSymbolCaptureOptions BuildFixtureTypeSymbolCaptureOptions(std::string) { return {}; }
SceneModelSymbolRenderResult CaptureSceneModelOrthographicRenders(
    Viewer2DOffscreenRenderer &, ConfigManager &, const SceneModelSymbolTarget &,
    const SceneModelSymbolCaptureOptions &) {
  ++captures;
  SceneModelSymbolRenderResult result;
  result.ok = true;
  result.fixtureBoundsMm.valid = true;
  return result;
}
SceneModelSymbolCaptureResult ProcessSceneModelOrthographicRenders(
    std::vector<symbols::RenderedSymbolImage>, const FixtureGeometryBounds &bounds,
    symbols::FixtureSymbolTimings *) {
  std::unique_lock lock(mutex);
  ++processing;
  entered = true;
  condition.notify_all();
  condition.wait(lock, [] { return released; });
  SceneModelSymbolCaptureResult result;
  result.ok = true;
  result.fixtureBoundsMm = bounds;
  return result;
}
bool CalibrateFixtureSymbolsToPhysicalUnits(const FixtureGeometryBounds &,
    std::vector<symbols::Symbol2D> &, std::string &) { return true; }
bool CalibrateFixtureSymbolsToPhysicalUnits(ConfigManager &, const std::string &,
    std::vector<symbols::Symbol2D> &, std::string &) { return true; }
}

#ifndef PERASTAGE_MEASURE_BASELINE
namespace gui {
struct FixtureSymbolPreparationServiceTestAccess {
  static bool Waiting(const FixtureSymbolPreparationService &service) {
    return service.workerPending_;
  }
};
}
#endif

int main(int argc, char **argv) {
  wxInitializer wx;
  assert(wx.IsOk());
  const std::string scenario = argc > 1 ? argv[1] : "complete";
  const auto path = std::filesystem::temp_directory_path() / ("symbol-scheduling-" + scenario + ".gdtf");
  {
    wxFileOutputStream file(path.string());
    wxZipOutputStream zip(file);
    zip.PutNextEntry("description.xml");
    const std::string xml = "<GDTF/>";
    zip.Write(xml.data(), xml.size());
  }
  auto &scene = config.config.scene;
  Fixture fixture;
  fixture.uuid = "fixture";
  fixture.gdtfSpec = path.string();
  fixture.gdtfMode = "Exact Mode";
  scene.fixtures[fixture.uuid] = fixture;
  MainWindow window;
  auto service = std::make_unique<gui::FixtureSymbolPreparationService>(window);
  if (scenario == "reopen") {
    scene.fixtures.at(fixture.uuid).projectSymbolDefinitionId = "persisted";
    service->BeginProjectEpoch(true);
    service->ScheduleScan();
    service->ScheduleScan();
    wxTheApp->ProcessPendingEvents();
    for (int i = 0; i < 10; ++i) {
      wxIdleEvent idle;
      window.ProcessEvent(idle);
      if (!idle.MoreRequested()) break;
    }
    assert(inspections == 0 && captures == 0 && processing == 0);
    assert(config.config.dirtyUpdates == 0);
    service.reset();
    std::filesystem::remove(path);
    return 0;
  }
  for (int i = 0; i < 100; ++i)
    symbols::RequestFixtureSymbolPreparation(path.string(), fixture.gdtfMode);
  wxTheApp->ProcessPendingEvents();
  int slices = 0;
  for (; slices < 100; ++slices) {
    wxIdleEvent idle;
    window.ProcessEvent(idle);
    if (!idle.MoreRequested()) { ++slices; break; }
  }
  {
    std::unique_lock lock(mutex);
    assert(condition.wait_for(lock, std::chrono::seconds(10), [] { return entered; }));
  }
  for (int i = 0; i < 100; ++i)
    symbols::RequestFixtureSymbolPreparation(path.string(), fixture.gdtfMode);
  wxTheApp->ProcessPendingEvents();
  int waitingSlices = 0;
  for (; waitingSlices < 100; ++waitingSlices) {
    wxIdleEvent idle;
    window.ProcessEvent(idle);
    if (!idle.MoreRequested()) break;
  }
#ifndef PERASTAGE_MEASURE_BASELINE
  assert(slices <= 3 && waitingSlices == 0);
  assert(inspections == 1 && captures == 1 && processing == 1);
#endif
  std::cout << "scenario=" << scenario << " initial_idle_slices=" << slices
            << " waiting_idle_reschedules=" << waitingSlices
            << " inspections=" << inspections << " captures=" << captures << '\n';
  if (scenario == "epoch") service->BeginProjectEpoch(false);
  if (scenario == "retry") {
    service->PromoteManualFixture(fixture.uuid);
    service->CompleteManualFixture(fixture.uuid, false);
  }
  if (scenario == "modes") {
    Fixture other = fixture;
    other.uuid = "second-fixture";
    other.gdtfMode = "Other Mode";
    scene.fixtures[other.uuid] = other;
    service->Request(path.string(), other.gdtfMode);
  }
  if (scenario == "manual") {
    service->PromoteManualFixture(fixture.uuid);
    scene.fixtures.at(fixture.uuid).projectSymbolDefinitionId = "manual";
  }
  if (scenario == "touch") {
    std::filesystem::last_write_time(path,
        std::filesystem::last_write_time(path) + std::chrono::seconds(1));
    service->Request(path.string(), fixture.gdtfMode);
    assert(inspections == 1 && captures == 1);
  }
  if (scenario == "source") {
    wxFileOutputStream file(path.string());
    wxZipOutputStream zip(file);
    zip.PutNextEntry("description.xml");
    const std::string xml = "<GDTF changed=\"true\"/>";
    zip.Write(xml.data(), xml.size());
  }
  if (scenario == "destroy") {
    // Teardown may overlap the worker's last post, but must join safely.
    { std::lock_guard lock(mutex); released = true; condition.notify_all(); }
    service.reset();
  } else {
    { std::lock_guard lock(mutex); released = true; condition.notify_all(); }
    // Pump actual posted completion notifications with a bounded deadline.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
      wxTheApp->ProcessPendingEvents();
      wxIdleEvent idle;
      window.ProcessEvent(idle);
      bool done = publications != 0;
#ifndef PERASTAGE_MEASURE_BASELINE
      done = !gui::FixtureSymbolPreparationServiceTestAccess::Waiting(*service);
#endif
      if (scenario == "retry") done = publications == 1;
      if (scenario == "modes") done = publications == 2;
      if (scenario == "source") {
        if (inspections > 1) {
          service->BeginProjectEpoch(false);
          break;
        }
        done = false;
      }
      if (done) {
        // Allow publication/finalization or stale-result discard to finish.
        for (int i = 0; i < 10; ++i) {
          wxTheApp->ProcessPendingEvents();
          wxIdleEvent next;
          window.ProcessEvent(next);
        }
        break;
      }
      std::this_thread::yield();
    }
    service.reset();
  }
  wxTheApp->ProcessPendingEvents();
  if (scenario == "complete" || scenario == "retry" || scenario == "touch") {
    assert(publications == 1 && config.config.dirtyUpdates == 1);
    assert(processing == (scenario == "retry" ? 2 : 1));
  } else if (scenario == "modes") {
    assert(publications == 2 && config.config.dirtyUpdates == 2);
    assert(processing == 2 && captures == 2 && inspections == 2);
  } else {
    assert(publications == 0 && config.config.dirtyUpdates == 0);
  }
  std::filesystem::remove(path);
}
