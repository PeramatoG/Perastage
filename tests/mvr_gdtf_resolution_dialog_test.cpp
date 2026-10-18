#include "mvr_gdtf_resolution_dialog.h"
#include "ui_feature_flags.h"
#include "runtime_storage.h"
#include "filesystem_path_utils.h"
#include "support/archive_entry_test_utils.h"
#include <wx/wx.h>
#include "gdtf_download_queue_table.h"
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

static bool ToggleSource(GdtfDownloadQueueTable &table, unsigned source, unsigned row = 0);

enum class ApplyOutcome { Success, DownloadFails, CancelDuringDownload, DefaultButton, ExistingFallback };

static void Run(bool automaticClose, bool fail = false, bool canceled = false, bool multipleModes = false, bool authFailed = false, bool cancelFinished = false, ApplyOutcome applyOutcome = ApplyOutcome::Success) {
  runtime_storage::TemporaryWorkspace workspace("mvr-gdtf-gui-test");
  const auto path = workspace.Path() / "fixture.gdtf";
  std::string error;
  assert(tests::archive::WriteStoredZipWithRawNames(path, {{"description.xml", R"(
    <GDTF DataVersion="1.2"><FixtureType Name="Beam 200" Manufacturer="Acme">
    <DMXModes><DMXMode Name="Standard"/><DMXMode Name="Extended"/></DMXModes></FixtureType></GDTF>)"}}, error));
  const auto downloadedPath = workspace.Path() / "downloaded.gdtf";
  assert(tests::archive::WriteStoredZipWithRawNames(downloadedPath, {{"description.xml", R"(
    <GDTF DataVersion="1.2"><FixtureType Name="Beam 200" Manufacturer="Acme">
    <!-- Downloaded revision -->
    <DMXModes><DMXMode Name="Standard"/><DMXMode Name="Extended"/></DMXModes></FixtureType></GDTF>)"}}, error));
  const auto profile = ReadProfile(downloadedPath);
  assert(profile.usable);
  mvr::SceneReadGdtfConflict request;
  request.type = "Original display type";
  request.fixtureName = "Beam 200";
  request.manufacturer = "Acme";
  request.mvrPath = PathUtils::PathToUtf8(path);
  request.modeName = "Standard";
  auto alias = request;
  alias.type = "Independent alias";
  if (applyOutcome == ApplyOutcome::ExistingFallback)
    alias.manufacturer = "Unlisted manufacturer";
  std::atomic<int> downloads{0};
  std::atomic<bool> replacementStarted{false};
  int logins = 0;
  MvrGdtfResolutionCallbacks callbacks;
  callbacks.loadCatalog = [multipleModes](auto) {
    MvrGdtfCatalog result;
    result.entries = {{"a", "Acme", "Beam 200", {{"Standard",0}}, 1, 1},
                      {"b", "Acme", "Beam 200", {{"Standard",0}}, 2, 2}};
    if (multipleModes)
      for (auto &entry : result.entries)
        entry.manufacturer = "Other";
    return result;
  };
  callbacks.authenticate = [&] { ++logins; return !authFailed; };
  callbacks.confirmSelection = [](const Choice &, const Profile *) { return true; };
  callbacks.download = [&](const Choice &choice, auto, auto cancel) {
    ++downloads;
    if (applyOutcome == ApplyOutcome::CancelDuringDownload && choice.revisionId == "a") {
      replacementStarted = true;
      const auto deadline = std::chrono::steady_clock::now() + 2s;
      while (!cancel() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
      assert(cancel());
      return MvrGdtfDownload{};
    }
    if (fail || (applyOutcome == ApplyOutcome::DownloadFails && choice.revisionId == "a") || (canceled && downloads == 2))
      return MvrGdtfDownload{{}, {}, "Simulated offline"};
    if (canceled && downloads > 1)
      assert(cancel());
    return MvrGdtfDownload{PathUtils::PathToUtf8(downloadedPath), profile, {}};
  };
  std::unordered_map<std::string, std::vector<std::string>> importedModes;
  if (multipleModes) {
    importedModes[request.type] = {"Standard", "Extended"};
    importedModes[alias.type] = {"Standard", "Extended"};
  }
  MvrGdtfResolutionDialog dialog(nullptr, {request, alias}, workspace.Path(), callbacks,
                                  automaticClose, importedModes);
  assert(!Button(dialog, "Apply selections"));
  assert(dialog.GetDefaultItem() == Button(dialog, "OK"));
  const auto checkAlias = [&](const Resolution &result) {
    if (applyOutcome == ApplyOutcome::ExistingFallback)
      assert(result.applied.source == Source::OriginalMvr);
    else
      assert(result.applied.revisionId == "b");
  };
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
    if (fail || authFailed) {
      if (results.at(request.type).status == Status::DownloadFailed &&
          Button(dialog, "OK")->IsEnabled()) {
        auto *table = Find<GdtfDownloadQueueTable>(dialog);
        assert(ToggleSource(*table, 0));
        // Both fixture aliases must use a valid selected source before acceptance.
        assert(ToggleSource(*table, 0, 1));
        Click(*Button(dialog, "OK"));
      }
      return;
    }
    if (automaticClose)
      return;
    auto *table = Find<GdtfDownloadQueueTable>(dialog);
    if (step == 2 && applyOutcome == ApplyOutcome::CancelDuringDownload && replacementStarted) {
      assert(!Button(dialog, "OK")->IsEnabled());
      ++step;
      Click(*Button(dialog, "Cancel"));
      return;
    }
    if (!Button(dialog, "OK")->IsEnabled())
      return;
    if (step == 0 && results.at(request.type).status == Status::Downloaded &&
        results.at(alias.type).status == (applyOutcome == ApplyOutcome::ExistingFallback ? Status::OriginalMvr : Status::Reused)) {
      if (cancelFinished) {
        Click(*Button(dialog, "Cancel"));
        return;
      }
      table->SelectRow(0);
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
      checkAlias(results.at(alias.type));
      assert(ToggleSource(*table, 0));
      assert(ToggleSource(*table, 2));
      assert(dialog.Results().at(request.type).selected.revisionId == "a");
      assert(dialog.Results().at(request.type).status == Status::Pending);
      ++step;
      if (canceled)
        Click(*Button(dialog, "Cancel"));
      else if (applyOutcome == ApplyOutcome::DefaultButton) {
        wxCommandEvent accept(wxEVT_BUTTON, wxID_OK);
        dialog.ProcessWindowEvent(accept); // Same route as native default-button activation.
      } else
        Click(*Button(dialog, "OK"));
    } else if (step == 2 && applyOutcome == ApplyOutcome::DownloadFails &&
               results.at(request.type).status == Status::DownloadFailed) {
      assert(dialog.IsModal() && results.at(alias.type).applied.revisionId == "b");
      ++step;
      assert(ToggleSource(*table, 0));
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
  assert(!Find<wxCheckBox>(dialog)); // The setting belongs exclusively in Preferences.
  if (multipleModes) {
    assert(downloads == 0 && logins == 0);
    assert(results.at(request.type).applied.mode.empty());
    assert(results.at(alias.type).applied.mode.empty());
    assert(results.at(request.type).status == Status::OriginalMvr);
  } else if (fail || authFailed) {
    if (authFailed)
      assert(downloads == 0 && logins == 1);
    assert(results.at(request.type).status == Status::OriginalMvr);
    assert(results.at(request.type).applied.source == Source::OriginalMvr);
  } else if (canceled || cancelFinished || applyOutcome == ApplyOutcome::CancelDuringDownload) {
    assert(downloads == (applyOutcome == ApplyOutcome::CancelDuringDownload ? 2 : 1));
    assert(results.at(request.type).status == Status::Canceled);
    assert(results.at(request.type).applied.source == Source::OriginalMvr);
    assert(results.at(request.type).applied.mode.empty());
    assert(results.at(alias.type).status == Status::Canceled);
    assert(results.at(alias.type).applied.source == Source::OriginalMvr);
  } else if (applyOutcome == ApplyOutcome::DownloadFails) {
    assert(downloads == 2 && step == 3);
    assert(results.at(request.type).applied.source == Source::OriginalMvr);
    checkAlias(results.at(alias.type));
  } else {
    assert(downloads == (automaticClose ? 1 : 2));
    assert(results.at(request.type).applied.revisionId == (automaticClose ? "b" : "a"));
    checkAlias(results.at(alias.type));
    assert(results.at(request.type).origin == (automaticClose ? SelectionOrigin::DownloadRequested
                                                             : SelectionOrigin::User));
  }
  assert(multipleModes || logins >= 1);
}

static void CancelLifecycle(int action) {
  runtime_storage::TemporaryWorkspace workspace("mvr-gdtf-cancel-test");
  mvr::SceneReadGdtfConflict request;
  request.type = "Martin Mac Aura XB";
  request.fixtureName = "MAC Aura XB";
  request.manufacturer = "Martin Professional";
  request.modeName = action == 4 ? "Standard" : "DMX Mode";
  request.mvrPath = "/original.gdtf";
  std::atomic<bool> started{false};
  std::atomic<bool> observedCancel{false};
  std::atomic<bool> downloading{false};
  std::atomic<bool> workerFinished{false};
  MvrGdtfResolutionCallbacks callbacks;
  callbacks.loadCatalog = [&](auto canceled) {
    started = true;
    if (action == 0) {
      const auto deadline = std::chrono::steady_clock::now() + 2s;
      while (!canceled() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
      observedCancel = canceled();
      // Model cleanup that completes after the next GUI timer tick.
      if (observedCancel)
        std::this_thread::sleep_for(100ms);
      workerFinished = true;
    }
    MvrGdtfCatalog catalog;
    catalog.entries = {{"aura", "Martin Professional", "MAC Aura XB",
                         {{"Standard",14}, {"Extended",25}}, 1, 1}};
    if (action == 5)
      catalog.entries.front().modes.clear();
    if (action != 4)
      catalog.entries.front().manufacturer = "Other";
    return catalog;
  };
  callbacks.authenticate = [action] { assert(action == 4); return true; };
  callbacks.download = [&](const Choice &, auto, auto canceled) {
    downloading = true;
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!canceled() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(1ms);
    observedCancel = canceled();
    // Native worker teardown can span several GUI timer ticks.
    if (observedCancel)
      std::this_thread::sleep_for(100ms);
    workerFinished = true;
    return MvrGdtfDownload{};
  };
  MvrGdtfResolutionDialog dialog(nullptr, {request}, workspace.Path(), callbacks, false);
  bool cancellationSent = false;
  wxTimer timer;
  timer.Bind(wxEVT_TIMER, [&](wxTimerEvent &) {
    // Already queued native timer notifications must not repeat the commands.
    if (cancellationSent)
      return;
    if (!started)
      return;
    if (action == 4 && !downloading)
      return;
    if (action != 0 && action != 4 && !dialog.IsFinished())
      return;
    if (action != 0 && action != 4) {
      wxChoice *candidates = Find<wxChoice>(dialog);
      assert(candidates && candidates->GetCount() == 1);
      const auto label = candidates->GetString(0);
      if (action == 5) {
        assert(label.Contains("Mode and channel metadata unavailable"));
      } else {
        assert(label.Contains("Standard / 14 channels"));
        assert(label.Contains("Extended / 25 channels"));
      }
      assert(!label.Contains("0 channels"));
      assert(Button(dialog, "Cancel")->IsEnabled());
    }
    cancellationSent = true;
    timer.Stop();
    if (action == 0 || action == 4) {
      Click(*Button(dialog, "OK"));
      assert(!dialog.IsCancellationRequested());
    }
    if (action == 2) {
      wxKeyEvent escape(wxEVT_CHAR_HOOK);
      escape.m_keyCode = WXK_ESCAPE;
      dialog.ProcessWindowEvent(escape);
    } else if (action == 3) {
      dialog.Close();
    } else {
      Click(*Button(dialog, "Cancel"));
    }
  });
  timer.Start(10);
  wxTimer deadline;
  deadline.Bind(wxEVT_TIMER, [&](wxTimerEvent &) { assert(false && "Cancellation timed out"); });
  deadline.StartOnce(3000);
  assert(dialog.RunResolution() == wxID_CANCEL);
  timer.Stop();
  const auto result = dialog.Results().at(request.type);
  assert(result.applied.source == Source::OriginalMvr && result.applied.path == request.mvrPath);
  assert(result.applied.mode.empty() && result.applied.revisionId.empty());
  if (action == 0 || action == 4) {
    assert(observedCancel);
    assert(workerFinished); // The modal boundary must wait for worker cleanup.
  }
}

static bool ToggleSource(GdtfDownloadQueueTable &table, unsigned source, unsigned row) {
  auto *model = dynamic_cast<wxDataViewIndexListModel *>(table.GetModel());
  assert(model);
  const auto item = model->GetItem(row);
  if (!model->SetValue(wxVariant(true), item, 5 + source))
    return false;
  wxDataViewEvent event(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, &table, table.GetColumn(1 + source), item);
  table.ProcessWindowEvent(event);
  return true;
}

static void DownloadRequested(bool identical, int scenario, bool libraryAvailable) {
  runtime_storage::TemporaryWorkspace workspace("mvr-elp-par-download-test");
  const auto original = workspace.Path() / "original.gdtf";
  const auto downloaded = workspace.Path() / "downloaded.gdtf";
  const auto library = workspace.Path() / "library.gdtf";
  std::string originalXml = R"(<GDTF DataVersion="1.2">
    <FixtureType Name="Martin ELP PAR" Manufacturer="Custom">
    <DMXModes><DMXMode Name="DMX Mode"><DMXChannels><DMXChannel Offset="1,14" Geometry="Base"/>
    </DMXChannels></DMXMode></DMXModes></FixtureType></GDTF>)";
  std::string downloadedXml = R"(<GDTF DataVersion="1.2">
    <FixtureType Name="ELP PAR" Manufacturer="Martin Professional">
    <DMXModes><DMXMode Name="Basic"><DMXChannels><DMXChannel Offset="1,14" Geometry="Base"/>
    </DMXChannels></DMXMode><DMXMode Name="Compact"><DMXChannels><DMXChannel Offset="1,9" Geometry="Base"/>
    </DMXChannels></DMXMode></DMXModes></FixtureType></GDTF>)";
  std::string error;
  if (scenario == 1)
    originalXml.replace(originalXml.find("Offset=\"1,14\""), 13, ""); // Unknown original footprint.
  if (scenario == 2)
    downloadedXml.replace(downloadedXml.find("1,9"), 3, "1,14"); // Two equally plausible modes.
  if (scenario == 3)
    originalXml.replace(originalXml.find("DMX Mode"), 8, " bAsIc ");
  if (scenario == 4)
    originalXml.replace(originalXml.find("1,14"), 4, "1,10"); // Known count overrides patch hints.
  assert(tests::archive::WriteStoredZipWithRawNames(original, {{"description.xml", originalXml}}, error));
  assert(tests::archive::WriteStoredZipWithRawNames(downloaded, {{"description.xml", downloadedXml}}, error));
  std::filesystem::copy_file(downloaded, library);
  mvr::SceneReadGdtfConflict request;
  request.type = request.fixtureName = "Martin ELP PAR";
  request.manufacturer = "Custom";
  request.modeName = scenario == 3 ? " bAsIc " : "DMX Mode";
  request.footprint = 0; // Recover authoritative counts from the original package.
  request.patchSpacingHint = scenario == 1 || scenario == 4 ? 14 : 0;
  request.mvrPath = PathUtils::PathToUtf8(original);
  if (libraryAvailable)
    request.appPath = PathUtils::PathToUtf8(library);
  int downloads = 0, confirmations = 0;
  MvrGdtfResolutionCallbacks callbacks;
  callbacks.loadCatalog = [](auto) {
    MvrGdtfCatalog catalog;
    catalog.entries = {{"77879", "Martin Professional", "ELP PAR", {{"Basic",14}, {"Compact",9}}, 1, 1},
                       {"77880", "Martin Professional", "ELP PAR IP", {{"Basic",14}}, 2, 5}};
    return catalog;
  };
  callbacks.authenticate = [] { return true; };
  callbacks.confirmSelection = [&](const Choice &choice, const Profile *) {
    ++confirmations;
    assert(choice.mode == "Basic");
    return true;
  };
  callbacks.download = [&](const Choice &choice, auto progress, auto canceled) {
    assert(!canceled() && choice.revisionId == "77879");
    ++downloads;
    const auto path = identical ? original : downloaded;
    progress(std::filesystem::file_size(path), std::filesystem::file_size(path));
    return MvrGdtfDownload{PathUtils::PathToUtf8(path), ReadProfile(path), {}};
  };
  MvrGdtfResolutionDialog dialog(nullptr, {request}, workspace.Path(), callbacks, false);
  bool finishing = false;
  wxTimer modalTimer;
  modalTimer.Bind(wxEVT_TIMER, [&](wxTimerEvent &) {
    for (auto *window : wxTopLevelWindows)
      if (auto *picker = dynamic_cast<wxSingleChoiceDialog *>(window))
        assert(!picker->IsModal() && "Automatic mode resolution must not open a picker");
  });
  modalTimer.Start(10);
  wxTimer timer;
  timer.Bind(wxEVT_TIMER, [&](wxTimerEvent &) {
    if (finishing)
      return;
    const auto result = dialog.Results().at(request.type);
    if ((scenario == 2 || scenario == 4) && downloads == 1 &&
        result.selected.source == Source::OriginalMvr && Button(dialog, "OK")->IsEnabled()) {
      finishing = true;
      assert(result.applied.path == request.mvrPath && result.applied.mode.empty());
      auto *table = Find<GdtfDownloadQueueTable>(dialog);
      assert(table->SourceChecked(0, 0) && table->SourceEnabled(0, 2));
      assert(table->GetItemText(0, 2).Contains("DMX mode unresolved"));
      assert(Find<wxTextCtrl>(dialog)->GetValue().Contains("could not be assigned unambiguously"));
      assert(Button(dialog, "Choose DMX mode...")->IsEnabled());
      Click(*Button(dialog, "OK"));
      return;
    }
    if (result.status != Status::Downloaded && result.status != Status::Reused && result.status != Status::Identical)
      return;
    if (!Button(dialog, "OK")->IsEnabled())
      return;
    finishing = true;
    auto *table = Find<GdtfDownloadQueueTable>(dialog);
    assert(table && table->GetColumnCount() == 8);
    assert(table->SourceChecked(0, 2));
    assert(table->SourceEnabled(0, 0));
    assert(table->SourceEnabled(0, 1) == libraryAvailable);
    assert(downloads == 1 && result.applied.revisionId == "77879");
    assert(result.applied.mode == (identical ? "DMX Mode" : "Basic"));
    if (identical) {
      assert(result.status == Status::Identical);
    } else {
      const auto details = Find<wxTextCtrl>(dialog)->GetValue();
      assert(details.Contains("Automatic DMX mode assignment"));
      assert(details.Contains(scenario == 1 ? "repeated patch spacing" : scenario == 3 ? "MVR mode name" : "original channel count"));
      assert(!details.Contains("DMX mode compatibility requires review."));
    }
    assert(ToggleSource(*table, 0));
    assert(table->SourceChecked(0, 0) && !table->SourceChecked(0, 2));
    auto current = dialog.Results().at(request.type);
    assert(current.applied.path == request.mvrPath && current.applied.mode.empty());
    assert(ToggleSource(*table, 2));
    assert(dialog.Results().at(request.type).applied.source == Source::Catalog && downloads == 1);
    if (libraryAvailable) {
      assert(ToggleSource(*table, 1));
      assert(table->SourceChecked(0, 1) && !table->SourceChecked(0, 2));
      assert(dialog.Results().at(request.type).applied.path == request.appPath);
      assert(ToggleSource(*table, 2));
      assert(downloads == 1 && table->SourceChecked(0, 2));
    } else {
      assert(!ToggleSource(*table, 1));
      assert(table->SourceChecked(0, 2));
    }
    Click(*Button(dialog, "OK"));
  });
  timer.Start(10);
  wxTimer deadline;
  deadline.Bind(wxEVT_TIMER, [&](wxTimerEvent &) { assert(false && "Explicit download/source switching timed out"); });
  deadline.StartOnce(10000);
  assert(dialog.RunResolution() == wxID_OK);
  timer.Stop();
  modalTimer.Stop();
  assert(downloads == 1 && finishing);
  assert(confirmations == 0);
}

int main(int argc, char **argv) {
  if (!wxEntryStart(argc, argv))
    return 77;
  if (!wxTheApp || !wxTheApp->CallOnInit())
    return 77;
  for (int action = 0; action < 6; ++action)
    CancelLifecycle(action);
  DownloadRequested(false, 0, true);
  DownloadRequested(false, 1, false);
  DownloadRequested(false, 2, false);
  DownloadRequested(false, 3, false);
  DownloadRequested(false, 4, false);
  DownloadRequested(true, 0, false);
  Run(true);
  Run(false);
  Run(false, false, false, false, false, false, ApplyOutcome::DownloadFails);
  Run(false, false, false, false, false, false, ApplyOutcome::CancelDuringDownload);
  Run(false, false, false, false, false, false, ApplyOutcome::DefaultButton);
  Run(false, false, false, false, false, false, ApplyOutcome::ExistingFallback);
  Run(false, false, true);
  Run(false, false, false, false, false, true);
  Run(true, true);
  Run(true, false, false, true);
  Run(true, false, false, false, true);
  wxTheApp->OnExit();
  wxEntryCleanup();
}
