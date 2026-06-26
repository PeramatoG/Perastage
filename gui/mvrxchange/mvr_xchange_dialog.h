#pragma once
#include "xchange/mvr_xchange_service.h"
#include "xchange/mvr_xchange_network_interfaces.h"
#include <memory>
#include <atomic>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

class MvrXchangeDialog : public wxDialog {
public:
  explicit MvrXchangeDialog(wxWindow *parent);
  ~MvrXchangeDialog() override;

private:
  void BuildLayout();
  void RefreshState();
  void AppendLog(const wxString &message);
  void OnStart(wxCommandEvent &event);
  void OnStop(wxCommandEvent &event);
  void OnPublish(wxCommandEvent &event);

  MvrXchangeSettings settings_;
  std::unique_ptr<MvrXchangeService> service_;
  std::shared_ptr<bool> lifetimeToken_ = std::make_shared<bool>(true);
  std::atomic<bool> shuttingDown_{false};
  wxStaticText *statusText_ = nullptr;
  wxTextCtrl *stationNameCtrl_ = nullptr;
  wxTextCtrl *groupNameCtrl_ = nullptr;
  wxTextCtrl *stationUuidCtrl_ = nullptr;
  wxTextCtrl *portCtrl_ = nullptr;
  wxChoice *interfaceChoice_ = nullptr;
  std::vector<MvrXchangeNetworkInterface> interfaces_;
  wxTextCtrl *logCtrl_ = nullptr;
  wxButton *startButton_ = nullptr;
  wxButton *stopButton_ = nullptr;
  wxButton *publishButton_ = nullptr;
};
