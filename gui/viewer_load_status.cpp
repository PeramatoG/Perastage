#include "viewer_load_status.h"
#include <wx/frame.h>
#include <wx/statusbr.h>
#include <wx/intl.h>

void ViewerLoadStatus::Update(wxWindow &viewer,
    const viewer3d::resources::ResourceLoadProgress &progress, bool paused) {
  auto *frame = dynamic_cast<wxFrame *>(wxGetTopLevelParent(&viewer));
  auto *bar = frame ? frame->GetStatusBar() : nullptr;
  if (!bar) return;
  wxString text;
  if (progress.completed < progress.total) {
    const int percent = static_cast<int>(100.0 * progress.completed / progress.total);
    text = wxString::Format(paused
        ? _("Scene loading paused while navigating: %zu/%zu (%d%%)")
        : _("Loading scene: %zu/%zu (%d%%)"), progress.completed, progress.total, percent);
  } else if (progress.representationsPending > 0) {
    text = wxString::Format(wxPLURAL(
        "Preparing scene for navigation: %zu item remaining",
        "Preparing scene for navigation: %zu items remaining", progress.representationsPending),
        progress.representationsPending);
  } else {
    if (!active) return;
    if (bar->GetStatusText(0) == lastText)
      bar->SetStatusText(progress.failed > 0
          ? _("Scene loaded. Some resources are unavailable; see Console.") : _("Ready"), 0);
    active = false;
    return;
  }
  active = true;
  lastText = text;
  if (bar->GetStatusText(0) != text) bar->SetStatusText(text, 0);
}
