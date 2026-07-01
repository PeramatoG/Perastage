#pragma once
#include "xchange/mvr_xchange_service.h"
#include "xchange/mvr_xchange_network_interfaces.h"
#include <memory>
#include <optional>
#include <atomic>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

class MvrXchangeDialog : public wxDialog {
public:
  explicit MvrXchangeDialog(wxWindow *parent);
  ~MvrXchangeDialog() override;

private:
  struct AvailableMvrFile {
    std::string stationUuid;
    std::string stationName;
    std::string fileUuid;
    std::string fileName;
    std::string comment;
    std::size_t fileSize = 0;
  };

  void BuildLayout();
  void RefreshState();
  void RefreshAvailableFiles(const std::vector<MvrXchangeRemoteStation> &stations);
  void ApplyConsoleLogStyle();
  void AppendLog(const wxString &message);
  void OnStart(wxCommandEvent &event);
  void OnStop(wxCommandEvent &event);
  void OnPublish(wxCommandEvent &event);
  void OnDiscover(wxCommandEvent &event);
  void OnRequest(wxCommandEvent &event);
  void OnAvailableFileActivated(wxDataViewEvent &event);
  std::optional<AvailableMvrFile> SelectedAvailableFile() const;
  bool ImportRequestedCommit(const AvailableMvrFile &file, const MvrXchangeCommit &commit);

  MvrXchangeSettings settings_;
  std::unique_ptr<MvrXchangeService> service_;
  std::shared_ptr<bool> lifetimeToken_ = std::make_shared<bool>(true);
  std::atomic<bool> shuttingDown_{false};
  wxStaticText *statusText_ = nullptr;
  wxStaticText *remoteStationsText_ = nullptr;
  wxDataViewListCtrl *availableFilesList_ = nullptr;
  wxTextCtrl *stationNameCtrl_ = nullptr;
  wxTextCtrl *groupNameCtrl_ = nullptr;
  wxTextCtrl *stationUuidCtrl_ = nullptr;
  wxTextCtrl *portCtrl_ = nullptr;
  wxChoice *interfaceChoice_ = nullptr;
  std::vector<MvrXchangeNetworkInterface> interfaces_;
  std::vector<AvailableMvrFile> availableFiles_;
  wxTextCtrl *logCtrl_ = nullptr;
  wxButton *startButton_ = nullptr;
  wxButton *stopButton_ = nullptr;
  wxButton *publishButton_ = nullptr;
  wxButton *requestButton_ = nullptr;
  wxButton *discoverButton_ = nullptr;
};
