#include "osc_controller.h"

#include "active_project_command_context.h"
#include "command/command_json_serializer.h"
#include "configmanager.h"
#include "diagnostics/DiagnosticLogger.h"
#include "mainwindow.h"
#include "osc/osc_command_adapter.h"

#include <mutex>
#include <wx/app.h>

struct OscController::DispatchState {
  std::mutex mutex;
  OscController *owner = nullptr;
  std::atomic<bool> stopping{false};
};

// Retains the application refresh owner used by OSC mutations.
OscController::OscController(MainWindow &window)
    : window_(window), dispatchState_(std::make_shared<DispatchState>()) {
  dispatchState_->owner = this;
}

// Stops the endpoint before destroying its application callback.
OscController::~OscController() { Stop(); }

// Starts the OSC endpoint on its dedicated application port.
bool OscController::Start(std::string &error) {
  return server_.Start(
      perastage::osc::kDefaultPort,
      [this](std::span<const std::uint8_t> packet) { ReceivePacket(packet); },
      error);
}

// Stops the endpoint before application services are torn down.
void OscController::Stop() {
  dispatchState_->stopping = true;
  {
    std::lock_guard lock(dispatchState_->mutex);
    dispatchState_->owner = nullptr;
  }
  server_.Stop();
}

// Parses a datagram and schedules supported commands on the GUI thread.
void OscController::ReceivePacket(std::span<const std::uint8_t> packet) {
  perastage::osc::Message message;
  std::string error;
  if (!perastage::osc::ParseMessage(packet, message, error)) {
    diagnostics::DiagnosticLogger::Warning("OSC rejected: " + error);
    return;
  }
  std::weak_ptr<DispatchState> weakState = dispatchState_;
  wxTheApp->CallAfter([weakState, message = std::move(message)]() mutable {
    const auto state = weakState.lock();
    if (!state || state->stopping)
      return;
    std::lock_guard lock(state->mutex);
    if (state->owner && !state->stopping)
      state->owner->HandleOnMainThread(std::move(message));
  });
}

// Executes a parsed OSC message against the active project.
void OscController::HandleOnMainThread(perastage::osc::Message message) {
  ActiveProjectCommandContext activeProject(ConfigManager::Get());
  const perastage::command::Result result = perastage::osc::ExecuteMessage(
      message, activeProject.Execution(), activeProject.TransformPolicy());
  activeProject.Publish(window_, result.mutation);
  diagnostics::DiagnosticLogger::Info(
      "OSC result: " +
      perastage::command::serialization::SerializeResultToJson(result));
}
