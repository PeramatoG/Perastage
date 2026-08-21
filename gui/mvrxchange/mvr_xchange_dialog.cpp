#include "mvr_xchange_dialog.h"
#include "runtime_storage.h"
#include "../mainwindow.h"
#include <wx/app.h>
#include <wx/clipbrd.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/settings.h>
#include <wx/thread.h>
#include <fstream>

namespace {

// Formats bytes for the MVR file list.
wxString FormatFileSize(std::uint64_t bytes) {
  if (bytes >= 1024u * 1024u)
    return wxString::Format("%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
  if (bytes >= 1024u)
    return wxString::Format("%.1f KB", static_cast<double>(bytes) / 1024.0);
  return wxString::Format("%llu B", static_cast<unsigned long long>(bytes));
}

// Returns the most useful display name for a remote station.
std::string StationDisplayName(const MvrXchangeRemoteStation &station) {
  if (!station.stationName.empty()) return station.stationName;
  if (!station.serviceInstanceName.empty()) return station.serviceInstanceName;
  if (!station.ipAddress.empty()) return station.ipAddress + ":" + std::to_string(station.port);
  return "Unknown station";
}

// Returns the most useful display name for an advertised MVR file.
std::string CommitDisplayName(const MvrXchangeCommit &commit) {
  if (!commit.fileName.empty()) return commit.fileName;
  if (!commit.comment.empty()) return commit.comment;
  return commit.fileUuid;
}

// Returns a filesystem-safe MVR filename from advertised commit metadata.
wxString RequestedMvrFileName(const std::string &fileName, const std::string &fileUuid) {
  wxString name = wxString::FromUTF8(fileName.empty() ? fileUuid : fileName);
  name.Trim(true).Trim(false);
  if (name.empty())
    name = "requested-mvr";

  for (wxString::iterator it = name.begin(); it != name.end(); ++it) {
    if (*it == '/' || *it == '\\' || *it == ':' || *it == '*' ||
        *it == '?' || *it == '"' || *it == '<' || *it == '>' ||
        *it == '|') {
      *it = '_';
    }
  }

  wxFileName filename(name);
  if (filename.GetExt().IsEmpty())
    filename.SetExt("mvr");
  return filename.GetFullName();
}

// Creates a unique temporary path that preserves the advertised MVR filename.
struct RequestedMvrTempPath {
  runtime_storage::TemporaryWorkspace workspace{"mvr-xchange-receive"};
  wxString filePath;
};

// Creates an operation-scoped temporary path that preserves the advertised MVR filename.
RequestedMvrTempPath CreateRequestedMvrTempPath(const std::string &fileName, const std::string &fileUuid) {
  RequestedMvrTempPath temp;
  if (!temp.workspace.IsValid())
    return temp;
  wxFileName requestedFile(wxString::FromUTF8(temp.workspace.Path().string()),
                           RequestedMvrFileName(fileName, fileUuid));
  temp.filePath = requestedFile.GetFullPath();
  return temp;
}

} // namespace

// Creates the MVR-xchange dialog and loads persisted settings.
MvrXchangeDialog::MvrXchangeDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "MVR-xchange", wxDefaultPosition,
               wxSize(1150, 850), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
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

// Builds the dialog controls for service status, settings, file requests, and logging.
void MvrXchangeDialog::BuildLayout() {
  SetMinSize(wxSize(1050, 775));
  SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

  auto *root = new wxBoxSizer(wxVERTICAL);
  auto *title = new wxStaticText(this, wxID_ANY, "MVR-xchange");
  wxFont titleFont = title->GetFont();
  titleFont.SetPointSize(titleFont.GetPointSize() + 4);
  titleFont.SetWeight(wxFONTWEIGHT_BOLD);
  title->SetFont(titleFont);
  auto *subtitle = new wxStaticText(
      this, wxID_ANY,
      "Publish the current scene or request advertised MVR revisions from compatible TCP Mode stations.");
  subtitle->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 14);
  root->Add(subtitle, 0, wxLEFT | wxRIGHT | wxBOTTOM, 14);

  auto *settingsBox = new wxStaticBoxSizer(wxVERTICAL, this, "Station settings");
  statusText_ = new wxStaticText(settingsBox->GetStaticBox(), wxID_ANY, "Stopped");
  stationNameCtrl_ = new wxTextCtrl(settingsBox->GetStaticBox(), wxID_ANY, wxString::FromUTF8(settings_.stationName));
  stationNameCtrl_->SetSelection(0, 0);
  stationNameCtrl_->SetInsertionPoint(0);
  stationNameCtrl_->ShowPosition(0);
  groupNameCtrl_ = new wxTextCtrl(settingsBox->GetStaticBox(), wxID_ANY, wxString::FromUTF8(settings_.groupName));
  stationUuidCtrl_ = new wxTextCtrl(settingsBox->GetStaticBox(), wxID_ANY, wxString::FromUTF8(settings_.stationUuid), wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
  portCtrl_ = new wxTextCtrl(settingsBox->GetStaticBox(), wxID_ANY, settings_.port > 0 ? wxString::Format("%d", settings_.port) : wxString("Auto"));
  interfaceChoice_ = new wxChoice(settingsBox->GetStaticBox(), wxID_ANY);
  interfaceChoice_->Append("Auto / All suitable interfaces");
  interfaces_ = ListMvrXchangeNetworkInterfaces();
  int selectedInterfaceIndex = 0;
  for (std::size_t i = 0; i < interfaces_.size(); ++i) {
    interfaceChoice_->Append(wxString::FromUTF8(FormatMvrXchangeNetworkInterface(interfaces_[i])));
    if (!settings_.selectedInterfaceId.empty() && (settings_.selectedInterfaceId == interfaces_[i].id || settings_.selectedInterfaceId == interfaces_[i].ipv4Address)) selectedInterfaceIndex = static_cast<int>(i + 1);
  }
  interfaceChoice_->SetSelection(selectedInterfaceIndex);
  auto *statusRow = new wxBoxSizer(wxHORIZONTAL);
  statusRow->Add(new wxStaticText(settingsBox->GetStaticBox(), wxID_ANY, "Status:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  statusRow->Add(statusText_, 1, wxALIGN_CENTER_VERTICAL);
  settingsBox->Add(statusRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
  auto *settingsGrid = new wxFlexGridSizer(4, 8, 8);
  settingsGrid->AddGrowableCol(1, 1);
  settingsGrid->AddGrowableCol(3, 1);
  settingsGrid->Add(new wxStaticText(settingsBox->GetStaticBox(), wxID_ANY, "Station name:"), 0, wxALIGN_CENTER_VERTICAL); settingsGrid->Add(stationNameCtrl_, 1, wxEXPAND);
  settingsGrid->Add(new wxStaticText(settingsBox->GetStaticBox(), wxID_ANY, "Group name:"), 0, wxALIGN_CENTER_VERTICAL); settingsGrid->Add(groupNameCtrl_, 1, wxEXPAND);
  settingsGrid->Add(new wxStaticText(settingsBox->GetStaticBox(), wxID_ANY, "Station UUID:"), 0, wxALIGN_CENTER_VERTICAL); settingsGrid->Add(stationUuidCtrl_, 1, wxEXPAND);
  settingsGrid->Add(new wxStaticText(settingsBox->GetStaticBox(), wxID_ANY, "TCP port:"), 0, wxALIGN_CENTER_VERTICAL); settingsGrid->Add(portCtrl_, 1, wxEXPAND);
  settingsBox->Add(settingsGrid, 0, wxEXPAND | wxALL, 10);
  auto *interfaceRow = new wxBoxSizer(wxHORIZONTAL);
  interfaceRow->Add(new wxStaticText(settingsBox->GetStaticBox(), wxID_ANY, "Network interface:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
  interfaceRow->Add(interfaceChoice_, 1, wxEXPAND);
  settingsBox->Add(interfaceRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  root->Add(settingsBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

  auto *remoteBox = new wxStaticBoxSizer(wxVERTICAL, this, "Remote stations and MVR files");
  remoteStationsText_ = new wxStaticText(remoteBox->GetStaticBox(), wxID_ANY, "0 discovered, 0 incoming joined, 0 outgoing joined");
  remoteStationsText_->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  remoteBox->Add(remoteStationsText_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
  const int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE;
  remoteBox->Add(new wxStaticText(remoteBox->GetStaticBox(), wxID_ANY, "Stations"), 0, wxLEFT | wxRIGHT | wxTOP, 10);
  stationsList_ = new wxDataViewListCtrl(remoteBox->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxSize(-1, 90), wxDV_ROW_LINES | wxDV_SINGLE | wxVSCROLL);
  stationsList_->AppendTextColumn("Station", wxDATAVIEW_CELL_INERT, 250, wxALIGN_LEFT, flags);
  stationsList_->AppendTextColumn("Membership", wxDATAVIEW_CELL_INERT, 150, wxALIGN_LEFT, flags);
  stationsList_->AppendTextColumn("Endpoint", wxDATAVIEW_CELL_INERT, 180, wxALIGN_LEFT, flags);
  stationsList_->AppendTextColumn("Station UUID", wxDATAVIEW_CELL_INERT, 290, wxALIGN_LEFT, flags);
  stationsList_->SetMinSize(wxSize(-1, 80));
  remoteBox->Add(stationsList_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
  remoteBox->Add(new wxStaticText(remoteBox->GetStaticBox(), wxID_ANY, "Advertised MVR files"), 0, wxLEFT | wxRIGHT | wxTOP, 10);
  availableFilesList_ = new wxDataViewListCtrl(remoteBox->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxSize(-1, 130), wxDV_ROW_LINES | wxDV_SINGLE | wxVSCROLL);
  availableFilesList_->AppendTextColumn("Station", wxDATAVIEW_CELL_INERT, 170, wxALIGN_LEFT, flags);
  availableFilesList_->AppendTextColumn("MVR file", wxDATAVIEW_CELL_INERT, 230, wxALIGN_LEFT, flags);
  availableFilesList_->AppendTextColumn("Size", wxDATAVIEW_CELL_INERT, 90, wxALIGN_LEFT, flags);
  availableFilesList_->AppendTextColumn("File UUID", wxDATAVIEW_CELL_INERT, 250, wxALIGN_LEFT, flags);
  availableFilesList_->AppendTextColumn("Comment", wxDATAVIEW_CELL_INERT, 220, wxALIGN_LEFT, flags);
  availableFilesList_->SetMinSize(wxSize(-1, 100));
  remoteBox->Add(availableFilesList_, 2, wxEXPAND | wxALL, 10);
  root->Add(remoteBox, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

  auto *logBox = new wxStaticBoxSizer(wxVERTICAL, this, "Log");
  auto *logActions = new wxBoxSizer(wxHORIZONTAL);
  logActions->AddStretchSpacer();
  copyLogButton_ = new wxButton(logBox->GetStaticBox(), wxID_ANY, "Copy Log");
  logActions->Add(copyLogButton_, 0);
  logBox->Add(logActions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
  logCtrl_ = new wxTextCtrl(logBox->GetStaticBox(), wxID_ANY, {}, wxDefaultPosition, wxSize(-1, 140), wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
  ApplyConsoleLogStyle();
  logBox->Add(logCtrl_, 1, wxEXPAND | wxALL, 10);
  root->Add(logBox, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

  auto *buttons = new wxBoxSizer(wxHORIZONTAL);
  startButton_ = new wxButton(this, wxID_ANY, "Start");
  stopButton_ = new wxButton(this, wxID_ANY, "Stop");
  discoverButton_ = new wxButton(this, wxID_ANY, "Discover Now");
  publishButton_ = new wxButton(this, wxID_ANY, "Publish Current MVR");
  requestButton_ = new wxButton(this, wxID_ANY, "Request Selected MVR");
  buttons->Add(startButton_, 0, wxRIGHT, 8);
  buttons->Add(stopButton_, 0, wxRIGHT, 8);
  buttons->Add(discoverButton_, 0, wxRIGHT, 8);
  buttons->AddStretchSpacer();
  buttons->Add(publishButton_, 0, wxRIGHT, 8);
  buttons->Add(requestButton_, 0, wxRIGHT, 8);
  buttons->Add(new wxButton(this, wxID_CLOSE, "Close"), 0);
  root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);

  SetSizer(root);
  startButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnStart, this);
  stopButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnStop, this);
  publishButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnPublish, this);
  requestButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnRequest, this);
  discoverButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnDiscover, this);
  copyLogButton_->Bind(wxEVT_BUTTON, &MvrXchangeDialog::OnCopyLog, this);
  availableFilesList_->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &MvrXchangeDialog::OnAvailableFileActivated, this);
  Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Close(); }, wxID_CLOSE);
  startButton_->SetFocus();
  wxTheApp->CallAfter([this] {
    if (shuttingDown_ || IsBeingDeleted() || !stationNameCtrl_) return;
    stationNameCtrl_->SetInsertionPoint(0);
    stationNameCtrl_->ShowPosition(0);
  });
}

// Refreshes status labels, remote file rows, and button enablement from the service state.
void MvrXchangeDialog::RefreshState() {
  if (shuttingDown_ || IsBeingDeleted() || !statusText_) return;
  const bool running = service_->IsRunning();
  const wxString endpoint = wxString::FromUTF8(service_->AdvertisedIpAddress()) + wxString::Format(":%d", service_->Port());
  statusText_->SetLabel(running ? wxString("Running on ") + endpoint : wxString("Stopped"));
  startButton_->Enable(!running);
  stopButton_->Enable(running);
  publishButton_->Enable(running);
  discoverButton_->Enable(running);
  const auto stations = service_->GetKnownStations();
  RefreshStations(stations);
  RefreshAvailableFiles(stations);
  requestButton_->Enable(running && SelectedAvailableFile().has_value());
  std::size_t discovered = 0;
  std::size_t incoming = 0;
  std::size_t outgoing = 0;
  for (const auto &station : stations) {
    if (station.discovered) ++discovered;
    if (station.incomingJoined) ++incoming;
    if (station.outgoingJoined) ++outgoing;
  }
  if (remoteStationsText_) {
    remoteStationsText_->SetLabel(wxString::Format("%zu discovered, %zu incoming joined, %zu outgoing joined", discovered, incoming, outgoing));
  }
}

// Rebuilds the station list from discovery and protocol membership state.
void MvrXchangeDialog::RefreshStations(const std::vector<MvrXchangeRemoteStation> &stations) {
  if (!stationsList_) return;
  stationsList_->DeleteAllItems();
  for (const auto &station : stations) {
    std::string membership = station.left ? "Left" : station.incomingJoined && station.outgoingJoined ? "Joined both ways" :
                             station.incomingJoined ? "Incoming joined" : station.outgoingJoined ? "Outgoing joined" :
                             station.discovered ? "Discovered" : "Offline";
    const std::string endpoint = station.ipAddress.empty() ? std::string{} : station.ipAddress + (station.port > 0 ? ":" + std::to_string(station.port) : std::string{});
    wxVector<wxVariant> row;
    row.push_back(wxVariant(wxString::FromUTF8(StationDisplayName(station))));
    row.push_back(wxVariant(wxString::FromUTF8(membership)));
    row.push_back(wxVariant(wxString::FromUTF8(endpoint)));
    row.push_back(wxVariant(wxString::FromUTF8(station.stationUuid)));
    stationsList_->AppendItem(row);
  }
}

// Rebuilds the remote MVR file list from advertised station commit metadata.
void MvrXchangeDialog::RefreshAvailableFiles(const std::vector<MvrXchangeRemoteStation> &stations) {
  if (!availableFilesList_) return;
  const int selectedRow = availableFilesList_->GetSelectedRow();
  availableFiles_.clear();
  availableFilesList_->DeleteAllItems();
  for (const auto &station : stations) {
    for (const auto &commit : station.commits) {
      AvailableMvrFile file;
      file.stationUuid = station.stationUuid;
      file.stationName = StationDisplayName(station);
      file.fileUuid = commit.fileUuid;
      file.fileName = CommitDisplayName(commit);
      file.comment = commit.comment;
      file.fileSize = commit.declaredFileSizeSpecified ? commit.declaredFileSize : commit.FileSize();
      availableFiles_.push_back(file);
      wxVector<wxVariant> row;
      row.push_back(wxVariant(wxString::FromUTF8(file.stationName)));
      row.push_back(wxVariant(wxString::FromUTF8(file.fileName)));
      row.push_back(wxVariant(FormatFileSize(file.fileSize)));
      row.push_back(wxVariant(wxString::FromUTF8(file.fileUuid)));
      row.push_back(wxVariant(wxString::FromUTF8(file.comment)));
      availableFilesList_->AppendItem(row);
    }
  }
  if (!availableFiles_.empty()) {
    const unsigned rowToSelect = selectedRow >= 0 && static_cast<std::size_t>(selectedRow) < availableFiles_.size() ? static_cast<unsigned>(selectedRow) : 0;
    availableFilesList_->SelectRow(rowToSelect);
  }
}

// Applies the same font and color style used by the Console panel output.
void MvrXchangeDialog::ApplyConsoleLogStyle() {
  if (!logCtrl_) return;
  const wxFont consoleFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
  const wxColour consoleText(0, 255, 0);
  logCtrl_->SetBackgroundColour(*wxBLACK);
  logCtrl_->SetForegroundColour(consoleText);
  logCtrl_->SetFont(consoleFont);
  logCtrl_->SetDefaultStyle(wxTextAttr(consoleText, *wxBLACK, consoleFont));
}

// Appends a status line to the dialog log area.
void MvrXchangeDialog::AppendLog(const wxString &message) {
  if (shuttingDown_ || IsBeingDeleted() || !logCtrl_) return;
  ApplyConsoleLogStyle();
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

// Requests the selected advertised remote MVR payload and imports it into the project.
void MvrXchangeDialog::OnRequest(wxCommandEvent &) {
  const auto selectedFile = SelectedAvailableFile();
  if (!selectedFile) {
    AppendLog("Select an advertised remote MVR file before requesting it.");
    return;
  }
  auto commit = service_->RequestRemoteCommit(selectedFile->stationUuid, selectedFile->fileUuid);
  if (!commit || commit->payload.empty()) {
    AppendLog(wxString("MVR-xchange request failed for FileUUID=") + wxString::FromUTF8(selectedFile->fileUuid) + ".");
    RefreshState();
    return;
  }
  ImportRequestedCommit(*selectedFile, *commit);
  RefreshState();
}

// Requests the activated remote MVR row.
void MvrXchangeDialog::OnAvailableFileActivated(wxDataViewEvent &) {
  wxCommandEvent event;
  OnRequest(event);
}

// Copies the complete protocol log to the system clipboard.
void MvrXchangeDialog::OnCopyLog(wxCommandEvent &) {
  if (!logCtrl_ || !wxTheClipboard->Open()) return;
  wxTheClipboard->SetData(new wxTextDataObject(logCtrl_->GetValue()));
  wxTheClipboard->Close();
}

// Returns metadata for the currently selected advertised MVR file.
std::optional<MvrXchangeDialog::AvailableMvrFile> MvrXchangeDialog::SelectedAvailableFile() const {
  if (!availableFilesList_) return std::nullopt;
  const int row = availableFilesList_->GetSelectedRow();
  if (row < 0 || static_cast<std::size_t>(row) >= availableFiles_.size()) return std::nullopt;
  return availableFiles_[static_cast<std::size_t>(row)];
}

// Writes a requested MVR payload to a temporary file and runs the normal import choice flow.
bool MvrXchangeDialog::ImportRequestedCommit(const AvailableMvrFile &file, const MvrXchangeCommit &commit) {
  auto tempPath = CreateRequestedMvrTempPath(file.fileName, file.fileUuid);
  if (tempPath.filePath.empty()) {
    AppendLog("MVR-xchange could not create a temporary folder for the requested MVR payload.");
    return false;
  }

  std::ofstream out(tempPath.filePath.ToStdString(), std::ios::binary);
  out.write(reinterpret_cast<const char *>(commit.payload.data()), static_cast<std::streamsize>(commit.payload.size()));
  out.close();
  if (!out) {
    AppendLog("MVR-xchange could not write the requested MVR payload to a temporary file.");
    return false;
  }
  if (auto *mainWindow = dynamic_cast<MainWindow *>(GetParent())) {
    AppendLog(wxString("Importing requested MVR-xchange file ") + wxString::FromUTF8(file.fileUuid) + ".");
    return mainWindow->ImportMvrWithUserChoice(tempPath.filePath.ToStdString());
  }
  return false;
}
