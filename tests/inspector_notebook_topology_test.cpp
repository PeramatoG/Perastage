#include <array>
#include <cassert>

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>

namespace {

class TestApp final : public wxApp {
public:
  // Initializes the native GUI runtime used by notebook controls.
  bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(TestApp);

} // namespace

// Verifies repeated format changes preserve fixed native notebook page counts.
int main() {
  int argc = 0;
  char **argv = nullptr;
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp && wxTheApp->CallOnInit());
  auto *frame = new wxFrame(nullptr, wxID_ANY, "Notebook topology test");
  auto *navigation = new wxNotebook(frame, wxID_ANY);
  auto *package = new wxPanel(navigation);
  auto *scene = new wxPanel(navigation);
  navigation->AddPage(package, "Package");
  navigation->AddPage(scene, "Scene");

  auto *details = new wxNotebook(frame, wxID_ANY);
  std::array<wxPanel *, 4> detailPages{};
  for (auto *&page : detailPages) {
    page = new wxPanel(details);
    details->AddPage(page, "Details");
  }
  auto *layout = new wxBoxSizer(wxVERTICAL);
  layout->Add(navigation, 1, wxEXPAND);
  layout->Add(details, 1, wxEXPAND);
  frame->SetSizer(layout);
  frame->SetSize(wxSize(800, 600));
  frame->Show();

  for (int iteration = 0; iteration < 40; ++iteration) {
    const bool gdtf = iteration % 2 != 0;
    scene->Enable(!gdtf);
    detailPages[1]->Enable(gdtf);
    navigation->ChangeSelection(gdtf ? 0 : 1);
    details->ChangeSelection(gdtf ? 1 : 0);
    wxTheApp->ProcessPendingEvents();
    assert(navigation->GetPageCount() == 2);
    assert(details->GetPageCount() == 4);
    assert(navigation->GetPage(0) == package);
    assert(navigation->GetPage(1) == scene);
    assert(details->GetPage(1) == detailPages[1]);
  }

  frame->Destroy();
  wxTheApp->ProcessPendingEvents();
  wxEntryCleanup();
  return 0;
}
