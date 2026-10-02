#include "local_live_controller.h"

#include "configmanager.h"
#include "live/live_request_executor.h"
#include "local_ipc/local_ipc_contract.h"
#include "mainwindow.h"
#include "project_mutation_host.h"
#include "selection_movement_settings.h"

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <wx/app.h>

struct LocalLiveController::DispatchState {
  std::mutex mutex;
  LocalLiveController *owner = nullptr;
  std::atomic<bool> stopping{false};
};

// Retains the application refresh owner used by live mutations.
LocalLiveController::LocalLiveController(MainWindow &window)
    : window_(window), dispatchState_(std::make_shared<DispatchState>()) {
  dispatchState_->owner = this;
}

// Stops the endpoint before destroying its application callback.
LocalLiveController::~LocalLiveController() { Stop(); }

// Starts the local live endpoint on the fixed application port.
bool LocalLiveController::Start(std::string &error) {
  return server_.Start(
      perastage::local_ipc::kDefaultPort,
      [this](const std::string &request) {
        return DispatchOnMainThread(request);
      },
      error);
}

// Stops the endpoint before application services are torn down.
void LocalLiveController::Stop() {
  if (dispatchState_) {
    dispatchState_->stopping = true;
    std::lock_guard lock(dispatchState_->mutex);
    dispatchState_->owner = nullptr;
  }
  server_.Stop();
}

// Marshals a transport request to the GUI thread for active-project access.
std::string
LocalLiveController::DispatchOnMainThread(const std::string &wireRequest) {
  auto promise = std::make_shared<std::promise<std::string>>();
  std::future<std::string> response = promise->get_future();
  std::weak_ptr<DispatchState> weakState = dispatchState_;
  wxTheApp->CallAfter([weakState, promise, wireRequest]() {
    const auto state = weakState.lock();
    if (!state || state->stopping)
      return;
    try {
      std::lock_guard lock(state->mutex);
      if (!state->owner || state->stopping)
        return;
      promise->set_value(state->owner->HandleOnMainThread(wireRequest));
    } catch (...) {
      promise->set_value(perastage::local_ipc::ErrorResponse(
          {}, "internal_error", "The live request could not be completed."));
    }
  });
  while (response.wait_for(std::chrono::milliseconds(25)) !=
         std::future_status::ready) {
    if (dispatchState_->stopping)
      return perastage::local_ipc::ErrorResponse(
          {}, "server_stopping", "The local live server is stopping.");
  }
  return response.get();
}

// Executes one validated request against the active project.
std::string
LocalLiveController::HandleOnMainThread(const std::string &wireRequest) {
  ConfigManager &config = ConfigManager::Get();
  scene_grouping::ObjectSelection selection{
      config.GetSelectedFixtures(), config.GetSelectedTrusses(),
      config.GetSelectedSupports(), config.GetSelectedSceneObjects()};
  GuiProjectMutationHost host(config);
  perastage::command::ExecutionContext context{config.GetScene(), selection,
                                               host};
  const auto policy =
      selection_movement_settings::LoadInteractiveTransformPolicy(config);
  const auto execution =
      perastage::live::ExecuteRequest(wireRequest, context, policy);

  config.SetSelectedFixtures(context.selection.fixtures);
  config.SetSelectedTrusses(context.selection.trusses);
  config.SetSelectedSupports(context.selection.supports);
  config.SetSelectedSceneObjects(context.selection.sceneObjects);
  if (execution.mutation.sceneChanged || execution.mutation.selectionChanged)
    window_.RefreshAfterToolSceneUpdate();
  return execution.response;
}
