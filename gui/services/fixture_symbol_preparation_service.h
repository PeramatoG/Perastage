#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "symbols/FixtureSymbolDiagnostics.h"
#include "symbols/Symbol2D.h"
#include "symbols/Symbol2DImageBuilder.h"
#include "symbols/fixture_symbol_preparation_coordinator.h"
#include "scenedatamanager.h"
#include "services/fixture_symbol_processing_worker.h"
#include "tools/fixture_geometry_bounds.h"

class MainWindow;
class wxIdleEvent;

namespace gui {

class FixtureSymbolPreparationService {
public:
  explicit FixtureSymbolPreparationService(MainWindow &window);
  ~FixtureSymbolPreparationService();

  void BeginProjectEpoch(bool scheduleScan);
  void ScheduleScan();
  void Request(const std::string &effectiveGdtfPath,
               const std::string &exactGdtfMode,
               symbols::FixtureSymbolPreparationPriority priority =
                   symbols::FixtureSymbolPreparationPriority::Automatic);
  void PromoteManualFixture(const std::string &fixtureUuid);
  void CompleteManualFixture(const std::string &fixtureUuid, bool applied);

private:
  enum class WorkStage { Capturing, Processing, Publishing, Finalizing };

  struct WorkContext {
    std::string fixtureUuid;
    std::string displayLabel;
    std::string sourceFingerprint;
    std::optional<SceneDataManager::SceneSnapshot> captureSnapshot;
    std::vector<symbols::RenderedSymbolImage> renders;
    std::vector<symbols::Symbol2D> processedSymbols;
    tools::FixtureGeometryBounds bounds;
    std::size_t nextCaptureStep = 0;
    WorkStage stage = WorkStage::Capturing;
    bool sceneUpdated = false;
    bool processingSubmitted = false;
  };

  void ScanCurrentProject();
  void ScheduleNextStep();
  void OnIdle(wxIdleEvent &event);
  void RunNextStep();
  void FailCurrent(const std::string &diagnostic);
  void UpdateStatus();
  std::string
  FindFixtureUuid(const symbols::FixtureSymbolPreparationKey &key) const;
  bool IsWorkIdentityCurrent(
      const symbols::FixtureSymbolPreparationKey &key,
      const WorkContext &work) const;

  MainWindow &window_;
  symbols::FixtureSymbolPreparationCoordinator coordinator_;
  FixtureSymbolProcessingWorker processingWorker_;
  std::unordered_map<symbols::FixtureSymbolPreparationKey, WorkContext,
                     symbols::FixtureSymbolPreparationKeyHash>
      work_;
  std::unordered_map<std::string, symbols::FixtureSymbolPreparationKey>
      manualWork_;
  std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
  std::uint64_t epoch_ = 0;
  bool stepScheduled_ = false;
  std::optional<symbols::FixtureSymbolPreparationKey> currentKey_;
};

} // namespace gui
