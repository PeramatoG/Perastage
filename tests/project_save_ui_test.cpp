/* This file is part of Perastage. */
#include "project_save_ui.h"
#include <cassert>
#include <stdexcept>
#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/frame.h>
#include <wx/window.h>

class SaveTestApp : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(SaveTestApp);

int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  {
    wxFrame owner(nullptr, wxID_ANY, "Owner");
    wxFrame alreadyDisabled(nullptr, wxID_ANY, "Disabled");
    owner.Show();
    alreadyDisabled.Enable(false);
    const auto topLevels = wxTopLevelWindows.GetCount();
    for (int outcome = 0; outcome < 4; ++outcome) {
      const auto result = project_save_ui::Run(owner, [&]() -> bool {
        assert(!owner.IsEnabled());
        assert(!alreadyDisabled.IsEnabled());
        assert(wxTopLevelWindows.GetCount() == topLevels + 1);
        bool found = false;
        for (auto *window : wxTopLevelWindows) {
          if (window == &owner || window == &alreadyDisabled) continue;
          assert(window->GetParent() == &owner);
          assert(window->IsShown());
          assert(!(window->GetWindowStyleFlag() & wxSTAY_ON_TOP));
          found = true;
        }
        assert(found);
        if (outcome == 2) throw std::runtime_error("test serialization failure");
        if (outcome == 3) throw 7;
        return outcome == 0;
      });
      assert(result.saved == (outcome == 0));
      assert(result.threw == (outcome >= 2));
      assert(result.exceptionDetail.empty() == (outcome < 2));
      // These are the conditions required before either Save handler opens a dialog.
      assert(owner.IsEnabled());
      assert(!alreadyDisabled.IsEnabled());
      assert(wxTopLevelWindows.GetCount() == topLevels);
    }
    assert(project_save_ui::FailureMessage(ProjectSaveFailureKind::MvrPreservation) !=
           project_save_ui::FailureMessage(ProjectSaveFailureKind::ArchiveIo));
    assert(project_save_ui::FailureMessage(ProjectSaveFailureKind::Resource) !=
           project_save_ui::FailureMessage(ProjectSaveFailureKind::Serialization));
  }
  wxTheApp->OnExit();
  wxEntryCleanup();
}
