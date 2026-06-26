#pragma once
#include "mvr/xchange/mvr_xchange_service.h"
#include <memory>
#include <wx/button.h>
#include <wx/dialog.h>
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
  wxStaticText *statusText_ = nullptr;
  wxTextCtrl *stationNameCtrl_ = nullptr;
  wxTextCtrl *groupNameCtrl_ = nullptr;
  wxTextCtrl *stationUuidCtrl_ = nullptr;
  wxTextCtrl *portCtrl_ = nullptr;
  wxTextCtrl *logCtrl_ = nullptr;
  wxButton *startButton_ = nullptr;
  wxButton *stopButton_ = nullptr;
  wxButton *publishButton_ = nullptr;
};
