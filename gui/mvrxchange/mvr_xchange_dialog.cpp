#include "mvr_xchange_dialog.h"
#include <wx/app.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/thread.h>

// Creates the MVR-xchange publisher dialog and loads persisted settings.
MvrXchangeDialog::MvrXchangeDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "MVR-xchange", wxDefaultPosition, wxSize(560, 420)),
      settings_(LoadMvrXchangeSettings()), service_(std::make_unique<MvrXchangeService>()) {
  service_->SetLogCallback([this](const std::string &message) {
    wxTheApp->CallAfter([this, message] { AppendLog(wxString::FromUTF8(message)); RefreshState(); });
  });
  BuildLayout();
  RefreshState();
}

// Stops the service when the dialog is destroyed.
MvrXchangeDialog::~MvrXchangeDialog() { service_->Stop(); }

// Builds the dialog controls for service status, settings, and manual publishing.
void MvrXchangeDialog::BuildLayout() {
  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *grid = new wxFlexGridSizer(2, 8, 8);
  grid->AddGrowableCol(1, 1);
  statusText_ = new wxStaticText(this, wxID_ANY, "Stopped");
  stationNameCtrl_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(settings_.stationName));
  groupNameCtrl_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(settings_.groupName));
  stationUuidCtrl_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(settings_.stationUuid), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
  portCtrl_ = new wxTextCtrl(this, wxID_ANY, settings_.port > 0 ? wxString::Format("%d", settings_.port) : wxString("Auto"));
  grid->Add(new wxStaticText(this, wxID_ANY, "Status:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(statusText_, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Station name:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(stationNameCtrl_, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Group name:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(groupNameCtrl_, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Station UUID:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(stationUuidCtrl_, 1, wxEXPAND);
  grid->Add(new wxStaticText(this, wxID_ANY, "Port:"), 0, wxALIGN_CENTER_VERTICAL); grid->Add(portCtrl_, 1, wxEXPAND);
  root->Add(grid, 0, wxEXPAND | wxALL, 12);
  auto *buttons = new wxBoxSizer(wxHORIZONTAL);
  startButton_ = new wxButton(this, wxID_ANY, "Start");
  stopButton_ = new wxButton(this, wxID_ANY, "Stop");
  publishButton_ = new wxButton(this, wxID_ANY, "Publish Current MVR");
  buttons->Add(startButton_, 0, wxRIGHT, 8); buttons->Add(stopButton_, 0, wxRIGHT, 8); buttons->Add(publishButton_, 0, wxRIGHT, 8); buttons->AddStretchSpacer(); buttons->Add(new wxButton(this, wxID_CLOSE, "Close"));
  root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
  logCtrl_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  root->Add(logCtrl_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
  SetSizer(root);
  startButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnStart, this);
  stopButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnStop, this);
  publishButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnPublish, this);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Close(); }, wxID_CLOSE);
}

// Refreshes status labels and button enablement from the service state.
void MvrXchangeDialog::RefreshState() {
  const bool running = service_->IsRunning();
  statusText_->SetLabel(running ? wxString::Format("Running on port %d", service_->Port()) : wxString("Stopped"));
  startButton_->Enable(!running);
  stopButton_->Enable(running);
  publishButton_->Enable(running);
}

// Appends a status line to the dialog log area.
void MvrXchangeDialog::AppendLog(const wxString &message) { logCtrl_->AppendText(message + "\n"); }

// Starts the MVR-xchange publisher with the current dialog settings.
void MvrXchangeDialog::OnStart(wxCommandEvent &) {
  settings_.stationName = stationNameCtrl_->GetValue().ToStdString();
  settings_.groupName = groupNameCtrl_->GetValue().ToStdString();
  long port = 0;
  if (portCtrl_->GetValue().ToLong(&port)) settings_.port = static_cast<int>(port); else settings_.port = 0;
  SaveMvrXchangeSettings(settings_);
  if (!service_->Start(settings_)) AppendLog("Service failed to start.");
  RefreshState();
}

// Stops the MVR-xchange publisher.
void MvrXchangeDialog::OnStop(wxCommandEvent &) { service_->Stop(); RefreshState(); }

// Publishes the current scene as a new MVR revision.
void MvrXchangeDialog::OnPublish(wxCommandEvent &) {
  if (!service_->PublishCurrentScene("Manual publish from Perastage")) AppendLog("Publish failed.");
  RefreshState();
}
