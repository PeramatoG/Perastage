#include "mvr_gdtf_resolution_dialog.h"
#include "ui_feature_flags.h"
#include "runtime_storage.h"
#include "filesystem_path_utils.h"
#include "support/archive_entry_test_utils.h"
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <wx/choice.h>
#include <wx/choicdlg.h>
#include <wx/msgdlg.h>
#include <wx/timer.h>
#include <cassert>
#include <atomic>
#include <chrono>

namespace ui { bool IsFeatureEnabled(FeatureFlag) { return false; } }
class ResolutionTestApp final : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(ResolutionTestApp);
using namespace gdtf_resolution;
using namespace std::chrono_literals;

template<class T> T *Find(wxWindow &window) {
  for (auto *child : window.GetChildren()) {
    if (auto *match = dynamic_cast<T *>(child))
      return match;
    if (auto *nested = Find<T>(*child))
      return nested;
  }
  return nullptr;
}
static wxButton *Button(wxWindow &window, const wxString &label) {
  for (auto *child : window.GetChildren()) {
    if (auto *button = dynamic_cast<wxButton *>(child))
      if (button->GetLabel() == label)
        return button;
    if (auto *nested = Button(*child, label))
      return nested;
  }
  return nullptr;
}
static void Click(wxWindow &window) {
  wxCommandEvent event(wxEVT_BUTTON, window.GetId());
  window.GetEventHandler()->ProcessEvent(event);
}
static void ApproveModal() {
  for (auto *window : wxTopLevelWindows)
    if (auto *dialog = dynamic_cast<wxDialog *>(window))
      if (dialog->IsModal() &&
          (dynamic_cast<wxSingleChoiceDialog *>(dialog) || dynamic_cast<wxMessageDialog *>(dialog)))
        dialog->EndModal(wxID_OK);
}

static void Run(bool automaticClose, bool fail = false, bool canceled = false, bool multipleModes = false) {
  runtime_storage::TemporaryWorkspace workspace("mvr-gdtf-gui-test");
  const auto path = workspace.Path() / "fixture.gdtf";
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path, {{"description.xml", R"(
    <GDTF DataVersion="1.2"><FixtureType Name="Beam 200" Manufacturer="Acme">
    <DMXModes><DMXMode Name="Standard"/><DMXMode Name="Extended"/></DMXModes></FixtureType></GDTF>)"}}, error));
  const auto profile = ReadProfile(path);
  assert(profile.usable);
  mvr::SceneReadGdtfConflict request;
  request.type = "Original display type";
  request.fixtureName = "Beam 200";
  request.manufacturer = "Acme";
  request.mvrPath = PathUtils::PathToUtf8(path);
  request.modeName = "Standard";
  auto alias = request;
  alias.type = "Independent alias";
  int downloads = 0;
  int logins = 0;
  bool persistCalled = false;
  MvrGdtfResolutionCallbacks callbacks;
  callbacks.loadCatalog = [] {
    MvrGdtfCatalog result;
    result.entries = {{"a", "Acme", "Beam 200", {{"Standard",0}}, 1, 1},
                      {"b", "Acme", "Beam 200", {{"Standard",0}}, 2, 2}};
    return result;
  };
  callbacks.authenticate = [&] { ++logins; return true; };
  callbacks.confirmSelection = [](const Choice &, const Profile *) { return true; };
  callbacks.persistAutoClose = [&](bool) { persistCalled = true; };
  callbacks.download = [&](const Choice &, auto, auto cancel) {
    ++downloads;
    if (fail || (canceled && downloads == 2))
      return MvrGdtfDownload{{}, {}, "Simulated offline"};
    if (canceled && downloads > 1)
      assert(cancel());
    return MvrGdtfDownload{PathUtils::PathToUtf8(path), profile, {}};
  };
  std::unordered_map<std::string, std::vector<std::string>> importedModes;
  if (multipleModes) {
    importedModes[request.type] = {"Standard", "Extended"};
    importedModes[alias.type] = {"Standard", "Extended"};
  }
  MvrGdtfResolutionDialog dialog(nullptr, {request, alias}, workspace.Path(), callbacks,
                                  automaticClose, importedModes);
  int step = 0;
  wxTimer timer;
  timer.Bind(wxEVT_TIMER, [&](wxTimerEvent &) {
    ApproveModal();
    const auto results = dialog.Results();
    if (multipleModes) {
      if (results.at(request.type).status == Status::OriginalMvr)
        Click(*Button(dialog, "OK"));
      return;
    }
    if (fail) {
      if (results.at(request.type).status == Status::DownloadFailed)
        Click(*Button(dialog, "OK"));
      return;
    }
    if (automaticClose)
      return;
    auto *table = Find<wxListCtrl>(dialog);
    auto *apply = Button(dialog, "Apply selections");
    if (!apply->IsEnabled())
      return;
    if (step == 0 && results.at(request.type).status == Status::Downloaded &&
        results.at(alias.type).status == Status::Reused) {
      table->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
      wxChoice *candidates = nullptr;
      for (auto *child : dialog.GetChildren())
        if (auto *choice = dynamic_cast<wxChoice *>(child)) {
          candidates = choice;
          break;
        }
      assert(candidates && candidates->GetCount() == 2);
      assert(candidates->GetStringSelection().Find("b") != wxNOT_FOUND);
      candidates->SetSelection(1); // Revision a, after b was already applied.
      wxCommandEvent event(wxEVT_CHOICE, candidates->GetId());
      ++step;
      candidates->GetEventHandler()->ProcessEvent(event);
    } else if (step == 1 && results.at(request.type).status == Status::Pending) {
      assert(results.at(alias.type).applied.revisionId == "b");
      ++step;
      if (canceled)
        Click(*Button(dialog, "Cancel"));
      else
        Click(*apply);
    } else if (step == 2 && results.at(request.type).applied.revisionId == "a") {
      assert(results.at(alias.type).applied.revisionId == "b");
      Click(*Button(dialog, "OK"));
    }
  });
  timer.Start(10);
  wxTimer deadline;
  deadline.Bind(wxEVT_TIMER, [&](wxTimerEvent &) { assert(false && "Queue GUI timed out"); });
  deadline.StartOnce(10000);
  dialog.RunResolution();
  timer.Stop();
  deadline.Stop();
  const auto results = dialog.Results();
  assert(!persistCalled); // Merely restoring the preference must not write it.
  if (multipleModes) {
    assert(downloads == 0 && logins == 0);
    assert(results.at(request.type).applied.mode.empty());
    assert(results.at(alias.type).applied.mode.empty());
    assert(results.at(request.type).status == Status::OriginalMvr);
  } else if (fail) {
    assert(results.at(request.type).status == Status::DownloadFailed);
    assert(results.at(request.type).applied.source == Source::OriginalMvr);
  } else if (canceled) {
    assert(downloads == 1);
    assert(results.at(request.type).status == Status::Canceled);
    assert(results.at(request.type).applied.revisionId == "b");
    assert(results.at(alias.type).status == Status::Reused);
  } else {
    assert(downloads == (automaticClose ? 1 : 2));
    assert(results.at(request.type).applied.revisionId == (automaticClose ? "b" : "a"));
    assert(results.at(alias.type).applied.revisionId == "b");
    assert(results.at(request.type).origin == (automaticClose ? SelectionOrigin::Automatic
                                                             : SelectionOrigin::User));
  }
  assert(multipleModes || logins >= 1);
}

int main(int argc, char **argv) {
  if (!wxEntryStart(argc, argv))
    return 77;
  if (!wxTheApp || !wxTheApp->CallOnInit())
    return 77;
  Run(true);
  Run(false);
  Run(false, false, true);
  Run(true, true);
  Run(true, false, false, true);
  wxTheApp->OnExit();
  wxEntryCleanup();
}
