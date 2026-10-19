#pragma once

#include "mvr_export_options.h"
#include <wx/filedlg.h>
#include <wx/filedlgcustomize.h>

// The saved preference suggests an alternate representation, never opts into it.
class MvrExportDialogOptions final : public wxFileDialogCustomizeHook {
public:
  explicit MvrExportDialogOptions(const MvrExportOptions &preferred)
      : preferred_(preferred) {}
  void AddCustomControls(wxFileDialogCustomize &customizer) override;
  void UpdateCustomControls() override;
  void TransferDataFromCustomControls() override;
  MvrExportOptions GetOptions() const { return selected_; }

private:
  MvrExportOptions preferred_;
  MvrExportOptions selected_ = CanonicalMvrExportOptions();
  wxFileDialogCheckBox *compatibility_ = nullptr;
  wxFileDialogChoice *representation_ = nullptr;
};
