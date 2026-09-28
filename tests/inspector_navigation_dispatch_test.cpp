#include "inspection/inspector_navigation_dispatch.h"

#include <cassert>

#include <wx/app.h>
#include <wx/frame.h>

namespace {

class TestApp final : public wxApp {
public:
  // Initializes the GUI runtime required by deferred event dispatch.
  bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(TestApp);

} // namespace

// Verifies nested navigation cannot run inside its originating event dispatch.
int main() {
  int argc = 0;
  char **argv = nullptr;
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp && wxTheApp->CallOnInit());
  auto *frame = new wxFrame(nullptr, wxID_ANY, "Navigation dispatch test");
  bool dispatching = true;
  bool called = false;
  gui::inspection::NestedGdtfNavigationRequest request{
      "fixtures/example.gdtf", true, 42};
  gui::inspection::QueueNestedGdtfNavigation(
      *frame, request,
      [&](gui::inspection::NestedGdtfNavigationRequest queued) {
        assert(!dispatching);
        assert(queued.archivePath == "fixtures/example.gdtf");
        called = true;
      });
  assert(!called);
  dispatching = false;
  wxTheApp->ProcessPendingEvents();
  assert(called);
  frame->Destroy();
  wxTheApp->ProcessPendingEvents();
  wxEntryCleanup();
  return 0;
}
