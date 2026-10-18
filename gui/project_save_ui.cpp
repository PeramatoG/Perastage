/* This file is part of Perastage. */
#include "project_save_ui.h"
#include <exception>
#include <memory>
#include <wx/dialog.h>
#include <wx/intl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>

namespace project_save_ui {
namespace {
class ScopedSaveOverlay {
public:
  explicit ScopedSaveOverlay(wxWindow &owner) {
    // Owned by this application window, with no system-wide topmost style.
    overlay_ = std::make_unique<wxDialog>(&owner, wxID_ANY, wxString(),
        wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(overlay_.get(), wxID_ANY, _("Saving project...")),
               0, wxALL, 20);
    overlay_->SetSizerAndFit(sizer);
    overlay_->CentreOnParent();
    disabler_ = std::make_unique<wxWindowDisabler>(overlay_.get());
    overlay_->Show();
    // Paint synchronously without dispatching arbitrary pending GUI commands.
    overlay_->Update();
  }
  ~ScopedSaveOverlay() {
    overlay_->Hide();
    overlay_.reset();
    disabler_.reset();
  }
private:
  std::unique_ptr<wxDialog> overlay_;
  std::unique_ptr<wxWindowDisabler> disabler_;
};
} // namespace

Result Run(wxWindow &owner, const std::function<bool()> &save) {
  try {
    ScopedSaveOverlay busy(owner);
    return {save(), false, {}};
  } catch (const std::exception &error) {
    return {false, true, error.what()};
  } catch (...) {
    return {false, true, "Unknown exception during project save."};
  }
}

wxString FailureMessage(ProjectSaveFailureKind kind) {
  switch (kind) {
  case ProjectSaveFailureKind::MvrPreservation:
    return _("The project contains MVR information that cannot yet be saved without loss. The previous file was kept. See the diagnostic log for the affected content.");
  case ProjectSaveFailureKind::Resource:
    return _("Required project resources could not be saved. See the diagnostic log for details.");
  case ProjectSaveFailureKind::ArchiveIo:
    return _("The project archive could not be written. Check the destination and available disk space. See the diagnostic log for details.");
  case ProjectSaveFailureKind::Serialization:
    return _("Project data could not be serialized. See the diagnostic log for details.");
  default:
    return _("An unexpected error interrupted project saving. See the diagnostic log for details.");
  }
}
} // namespace project_save_ui
