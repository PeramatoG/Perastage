#include "viewer_load_status.h"
#include <cassert>
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/statusbr.h>

class TestApp : public wxApp { public: bool OnInit() override { return true; } };
wxIMPLEMENT_APP_NO_MAIN(TestApp);

int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  auto *frame = new wxFrame(nullptr, wxID_ANY, "Loading progress test");
  auto *bar = frame->CreateStatusBar();
  auto *viewer = new wxPanel(frame);
  ViewerLoadStatus status;
  status.Update(*viewer, {0, 10}, false);
  assert(bar->GetStatusText() == "Loading scene: 0/10 (0%)");
  status.Update(*viewer, {4, 10}, true);
  assert(bar->GetStatusText().Contains("paused") && bar->GetStatusText().Contains("4/10 (40%)"));
  status.Update(*viewer, {10, 10, 0, 1}, false);
  assert(bar->GetStatusText() == "Preparing scene for navigation: 1 item remaining");
  status.Update(*viewer, {10, 10}, false);
  assert(bar->GetStatusText() == "Ready");
  status.Update(*viewer, {0, 2}, false);
  bar->SetStatusText("Saving project...");
  status.Update(*viewer, {2, 2}, false);
  assert(bar->GetStatusText() == "Saving project...");
  status.Update(*viewer, {1, 2}, false);
  status.Update(*viewer, {2, 2, 1}, false);
  assert(bar->GetStatusText().Contains("unavailable"));
  // A replaced project begins a new count rather than retaining completion.
  status.Update(*viewer, {0, 100}, false);
  assert(bar->GetStatusText() == "Loading scene: 0/100 (0%)");
  delete frame;
  wxTheApp->OnExit();
  wxEntryCleanup();
}
