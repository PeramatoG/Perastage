#include "native_gui_publication.h"

#include <cassert>
#include <stdexcept>
#include <wx/app.h>
#include <wx/dataview.h>
#include <wx/settings.h>
#include <wx/frame.h>
#include <wx/glcanvas.h>
#include <wx/panel.h>
#include <wx/utils.h>
#include <wx/wupdlock.h>

class TestApp : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(TestApp);

class NativeProbe : public wxPanel {
public:
  explicit NativeProbe(wxWindow *parent) : wxPanel(parent) {}
  void Refresh(bool, const wxRect *) override { ++refreshes; }
  void Update() override { ++updates; }
  int refreshes = 0;
  int updates = 0;
};

class GlProbe : public wxGLCanvas {
public:
  explicit GlProbe(wxWindow *parent) : wxGLCanvas(parent) {}
  void Refresh(bool, const wxRect *) override { ++refreshes; }
  void Update() override { ++updates; }
  int refreshes = 0;
  int updates = 0;
};

static void FailedComposition(wxWindow &window, bool throws) {
  wxWindowDisabler inputDisabler;
  wxWindowUpdateLocker composition(&window);
  assert(window.IsFrozen() && !window.IsEnabled());
  if (throws)
    throw std::runtime_error("composition failed");
  return;
}

int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  auto *frame = new wxFrame(nullptr, wxID_ANY, "Publication test");
  auto *native = new NativeProbe(frame);
  auto *nested = new NativeProbe(native);
  auto *mixed = new NativeProbe(frame);
  auto *gl = new GlProbe(mixed);
  auto *sibling = new NativeProbe(mixed);
  auto *hidden = new NativeProbe(frame);
  hidden->Hide();
  auto *table = new wxDataViewListCtrl(native, wxID_ANY);
  auto *custom = new NativeProbe(frame);
  const wxColour consoleBackground(8, 9, 10);
  custom->SetBackgroundColour(consoleBackground);
  native->SetBackgroundColour(wxNullColour);
  table->SetBackgroundColour(wxNullColour);
  gui::InitializeNativeGuiBackgrounds(*frame);
  assert(native->GetBackgroundColour() ==
         wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
  assert(table->GetBackgroundColour() ==
         wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
  assert(custom->GetBackgroundColour() == consoleBackground);
  assert(gl->refreshes == 0 && gl->updates == 0);
  assert(mixed->refreshes == 0 && mixed->updates == 0);
  auto *otherFrame = new wxFrame(frame, wxID_ANY, "Unrelated loading UI");
  auto *other = new NativeProbe(otherFrame);
  frame->Show();
  otherFrame->Show();
  // Process initial native mapping for the fixture only, not production code.
  wxTheApp->Yield();
  native->refreshes = native->updates = nested->updates = 0;
  mixed->refreshes = mixed->updates = gl->refreshes = gl->updates = 0;
  sibling->refreshes = sibling->updates = hidden->updates = other->updates = 0;
  // The completion continuation must run without another native idle event.
  int idleEvents = 0;
  frame->Bind(wxEVT_IDLE, [&idleEvents](wxIdleEvent &event) {
    ++idleEvents;
    event.Skip();
  });
  frame->CallAfter([frame]() { gui::PublishNativeGuiChildren(*frame); });
  wxTheApp->ProcessPendingEvents();
  assert(idleEvents == 0);
  assert(native->refreshes == 1 && native->updates == 1);
  assert(nested->updates == 1);
  assert(mixed->refreshes == 0 && mixed->updates == 0);
  assert(gl->refreshes == 0 && gl->updates == 0);
  assert(sibling->refreshes == 1 && sibling->updates == 1);
  assert(hidden->updates == 0 && other->updates == 0);

  FailedComposition(*frame, false);
  assert(!frame->IsFrozen() && frame->IsEnabled());
  try {
    FailedComposition(*frame, true);
    assert(false);
  } catch (const std::runtime_error &) {
    assert(!frame->IsFrozen() && frame->IsEnabled());
  }
  // A panel-local locker must not thaw the surrounding frame transaction.
  {
    wxWindowUpdateLocker composition(frame);
    { wxWindowUpdateLocker panelReload(native); }
    assert(frame->IsFrozen() && native->IsFrozen());
  }
  assert(!frame->IsFrozen() && !native->IsFrozen());
  delete otherFrame;
  delete frame;
  wxTheApp->OnExit();
  wxEntryCleanup();
}
