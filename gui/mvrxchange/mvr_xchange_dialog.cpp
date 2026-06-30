#include "mvr_xchange_dialog.h"
#include "../mainwindow.h"
#include <wx/app.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/thread.h>
#include <wx/filename.h>
#include <fstream>

// Creates the MVR-xchange publisher dialog and loads persisted settings.
MvrXchangeDialog::MvrXchangeDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "MVR-xchange", wxDefaultPosition, wxSize(560, 420)),
      settings_(LoadMvrXchangeSettings()), service_(std::make_unique<MvrXchangeService>()) {
  std::weak_ptr<bool> weakLifetime = lifetimeToken_;
  service_->SetLogCallback([this, weakLifetime](const std::string &message) {
    wxTheApp->CallAfter([this, weakLifetime, message] {
      if (weakLifetime.expired() || shuttingDown_) return;
      AppendLog(wxString::FromUTF8(message));
      RefreshState();
    });
  });
  BuildLayout();
  RefreshState();
}

// Detaches callbacks and stops the service before dialog controls are destroyed.
MvrXchangeDialog::~MvrXchangeDialog() {
  shuttingDown_ = true;
  lifetimeToken_.reset();
  if (service_) {
    service_->SetLogCallback(nullptr);
    service_->Stop();
  }
  logCtrl_ = nullptr;
}

// Builds the dialog controls for service status, settings, and manual publishing.
void MvrXchangeDialog::BuildLayout() {
  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *grid = new wxFlexGridSizer(2, 8, 8);
  grid->AddGrowableCol(1, 1);
  statusText_ = new wxStaticText(this, wxID_ANY, "Stopped");
  stationNameCtrl_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(settings_.stationName));
  stationNameCtrl_->SetSelection(0, 0);
  groupNameCtrl_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(settings_.groupName));
  stationUuidCtrl_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(settings_.stationUuid), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
  portCtrl_ = new wxTextCtrl(this, wxID_ANY, settings_.port > 0 ? wxString::Format("%d", settings_.port) : wxString("Auto"));
  interfaceChoice_ = new wxChoice(this, wxID_ANY);
  interfaceChoice_->Append("Auto / All suitable interfaces");
  interfaces_ = ListMvrXchangeNetworkInterfaces();
  int selectedInterfaceIndex = 0;
  for (std::size_t i = 0; i < interfaces_.size(); ++i) {
    interfaceChoice_->Append(wxString::FromUTF8(FormatMvrXchangeNetworkInterface(interfaces_[i])));
    if (!settings_.selectedInterfaceId.empty() && (settings_.selectedInterfaceId == interfaces_[i].id || settings_.selectedInterfaceId == interfaces_[i].ipv4Address)) selectedInterfaceIndex = static_cast<int>(i + 1);
  }
  interfaceChoice_->SetSelection(selectedInterfaceIndex);
  grid->Add(new wxStaticText(this, wxID_ANY, "Status:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(statusText_, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Station name:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(stationNameCtrl_, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Group name:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(groupNameCtrl_, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Station UUID:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(stationUuidCtrl_, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Network interface:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(interfaceChoice_, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "TCP port:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(portCtrl_, 1, wxEXPAND);
  remoteStationsText_ = new wxTextCtrl(this, wxID_ANY, "0 discovered, 0 incoming joined, 0 outgoing joined", wxDefaultPosition, wxSize(-1, 70), wxTE_MULTILINE | wxTE_READONLY);
  grid->Add(new wxStaticText(this, wxID_ANY, "Remote stations:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(remoteStationsText_, 1, wxEXPAND);
  root->Add(grid, 0, wxEXPAND | wxALL, 12);
  auto *buttons = new wxBoxSizer(wxHORIZONTAL);
  startButton_ = new wxButton(this, wxID_ANY, "Start");
  stopButton_ = new wxButton(this, wxID_ANY, "Stop");
  publishButton_ = new wxButton(this, wxID_ANY, "Publish Current MVR");
  requestButton_ = new wxButton(this, wxID_ANY, "Request Latest Remote MVR");
  discoverButton_ = new wxButton(this, wxID_ANY, "Discover Now");
  buttons->Add(startButton_, 0, wxRIGHT, 8); buttons->Add(stopButton_, 0, wxRIGHT, 8); buttons->Add(discoverButton_, 0, wxRIGHT, 8); buttons->Add(publishButton_, 0, wxRIGHT, 8); buttons->Add(requestButton_, 0, wxRIGHT, 8); buttons->AddStretchSpacer(); buttons->Add(new wxButton(this, wxID_CLOSE, "Close"));
  root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
  logCtrl_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  root->Add(logCtrl_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
  SetSizer(root);
  startButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnStart, this);
  stopButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnStop, this);
  publishButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnPublish, this);
  requestButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnRequest, this);
  discoverButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnDiscover, this);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Close(); }, wxID_CLOSE);
  startButton_->SetFocus();
}

// Refreshes status labels and button enablement from the service state.
void MvrXchangeDialog::RefreshState() {
  if (shuttingDown_ || IsBeingDeleted() || !statusText_) return;
  const bool running = service_->IsRunning();
  const wxString endpoint = wxString::FromUTF8(service_->AdvertisedIpAddress()) + wxString::Format(":%d", service_->Port());
  statusText_->SetLabel(running ? wxString("Running on ") + endpoint : wxString("Stopped"));
  startButton_->Enable(!running);
  stopButton_->Enable(running);
  publishButton_->Enable(running);
  requestButton_->Enable(running);
  discoverButton_->Enable(running);
  if (remoteStationsText_) {
    std::size_t discovered = 0;
    std::size_t incoming = 0;
    std::size_t outgoing = 0;
    wxString summary;
    const auto stations = service_->GetKnownStations();
    for (const auto &station : stations) {
      if (station.discovered) ++discovered;
      if (station.incomingJoined) ++incoming;
      if (station.outgoingJoined) ++outgoing;
    }
    summary << wxString::Format("%zu discovered, %zu incoming joined, %zu outgoing joined", discovered, incoming, outgoing);
    for (const auto &station : stations) {
      summary << "\n" << wxString::FromUTF8(station.stationName.empty() ? station.serviceInstanceName : station.stationName)
              << " | " << wxString::FromUTF8(station.ipAddress) << ":" << station.port
              << (station.discovered ? " | discovered" : "")
              << (station.incomingJoined ? " | incoming joined" : "")
              << (station.outgoingJoined ? " | outgoing joined" : "");
    }
    remoteStationsText_->SetValue(summary);
  }
}

// Appends a status line to the dialog log area.
void MvrXchangeDialog::AppendLog(const wxString &message) {
  if (shuttingDown_ || IsBeingDeleted() || !logCtrl_) return;
  logCtrl_->AppendText(message + "\n");
}

// Starts the MVR-xchange publisher with the current dialog settings.
void MvrXchangeDialog::OnStart(wxCommandEvent &) {
  settings_.stationName = stationNameCtrl_->GetValue().ToStdString();
  settings_.groupName = groupNameCtrl_->GetValue().ToStdString();
  long port = 0;
  if (portCtrl_->GetValue().ToLong(&port)) settings_.port = static_cast<int>(port); else settings_.port = 0;
  const int selectedInterfaceIndex = interfaceChoice_ ? interfaceChoice_->GetSelection() : 0;
  settings_.selectedInterfaceId = selectedInterfaceIndex > 0 && static_cast<std::size_t>(selectedInterfaceIndex - 1) < interfaces_.size() ? interfaces_[static_cast<std::size_t>(selectedInterfaceIndex - 1)].id : std::string{};
  SaveMvrXchangeSettings(settings_);
  if (!service_->Start(settings_)) AppendLog("Service failed to start.");
  RefreshState();
}

// Stops the MVR-xchange publisher.
void MvrXchangeDialog::OnStop(wxCommandEvent &) { service_->Stop(); RefreshState(); }

// Runs an immediate MVR-xchange station discovery pass.
void MvrXchangeDialog::OnDiscover(wxCommandEvent &) { service_->DiscoverNow(); RefreshState(); }

// Publishes the current scene as a new MVR revision.
void MvrXchangeDialog::OnPublish(wxCommandEvent &) {
  std::string projectName;
  if (auto *mainWindow = dynamic_cast<MainWindow *>(GetParent())) projectName = mainWindow->GetCurrentProjectDisplayName().ToStdString();
  if (!service_->PublishCurrentScene("Manual publish from Perastage", projectName)) AppendLog("Publish failed.");
  RefreshState();
}

// Requests the newest advertised remote MVR payload and imports it into the project.
void MvrXchangeDialog::OnRequest(wxCommandEvent &) {
  service_->DiscoverNow();
  const auto stations = service_->GetKnownStations();
  for (const auto &station : stations) {
    if (station.commits.empty()) continue;
    const auto &metadata = station.commits.back();
    auto commit = service_->RequestRemoteCommit(station.stationUuid, metadata.fileUuid);
    if (!commit || commit->payload.empty()) {
      AppendLog(wxString("MVR-xchange request failed for FileUUID=") + wxString::FromUTF8(metadata.fileUuid) + ".");
      continue;
    }
    wxFileName tempFile(wxFileName::CreateTempFileName("perastage_mvr_xchange_"));
    tempFile.SetExt("mvr");
    std::ofstream out(tempFile.GetFullPath().ToStdString(), std::ios::binary);
    out.write(reinterpret_cast<const char *>(commit->payload.data()), static_cast<std::streamsize>(commit->payload.size()));
    out.close();
    if (!out) {
      AppendLog("MVR-xchange could not write the requested MVR payload to a temporary file.");
      return;
    }
    if (auto *mainWindow = dynamic_cast<MainWindow *>(GetParent())) {
      AppendLog(wxString("Importing requested MVR-xchange file ") + wxString::FromUTF8(metadata.fileUuid) + ".");
      mainWindow->OpenPathFromCommandLine(tempFile.GetFullPath().ToStdString());
    }
    RefreshState();
    return;
  }
  AppendLog("No remote MVR-xchange files are currently advertised. Click Discover Now and try again.");
  RefreshState();
}
