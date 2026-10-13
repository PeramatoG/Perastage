#include "preferencesdialog.h"
#include "apppaths.h"
#include "cache_service.h"
#include <chrono>
#include <fstream>
#include <filesystem>
#include <wx/button.h>
#include "guiconfigservices.h"
#include "preferences/preferences_page.h"
#include "preferences/units_preferences_page.h"
#include "preferences/language_preferences_page.h"
#include "preferences/rider_import_preferences_page.h"
#include "model_detail_policy.h"
#include "localization/app_language.h"
#include "localization/localization_manager.h"
#include <wx/translation.h>
#include <cassert>
#include <cmath>
#include <map>
#include <iostream>
#include <wx/sizer.h>
#include <wx/app.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/frame.h>
#include <wx/modalhook.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/textctrl.h>
#include <wx/treebook.h>

extern int preferencesTestCredentialLoads;
extern int preferencesTestCredentialApplies;

const auto cacheTestDir = std::filesystem::temp_directory_path() /
    ("perastage-cache-preferences-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
namespace AppPaths {
std::filesystem::path GetUserDataDir() { return cacheTestDir; }
}

class TestApp : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(TestApp);

class Preferences : public IGuiPreferencesService {
public:
  std::map<std::string, std::string> values;
  std::map<std::string, float> floats;
  mutable int saves = 0;
  void SetValue(const std::string &key, const std::string &value) override {
    values[key] = value;
  }
  std::optional<std::string> GetValue(const std::string &key) const override {
    const auto it = values.find(key);
    return it == values.end() ? std::nullopt : std::optional<std::string>(it->second);
  }
  void RemoveKey(const std::string &key) override { values.erase(key); }
  bool SaveUserConfig() const override { ++saves; return true; }
  float GetFloat(const std::string &key) const override {
    const auto it = floats.find(key);
    return it == floats.end() ? 0.0f : it->second;
  }
  void SetFloat(const std::string &key, float value) override { floats[key] = value; }
};

class Notices : public wxModalDialogHook {
public:
  int restartNotices = 0;
  int cacheNotices = 0;
  int cacheResponse = wxID_NO;
private:
  int Enter(wxDialog *dialog) override {
    if (dynamic_cast<wxMessageDialog *>(dialog)) {
      if (dialog->GetTitle() == "Clear Cache") {
        ++cacheNotices;
        return cacheResponse;
      }
      assert(dialog->GetTitle() == "Restart required");
      ++restartNotices;
      return wxID_OK;
    }
    return wxID_NONE;
  }
};

template <typename Control>
std::vector<Control *> FindControls(wxWindow *parent) {
  std::vector<Control *> found;
  for (wxWindow *child : parent->GetChildren()) {
    if (auto *control = dynamic_cast<Control *>(child))
      found.push_back(control);
    const auto descendants = FindControls<Control>(child);
    found.insert(found.end(), descendants.begin(), descendants.end());
  }
  return found;
}

void DumpSizes(wxWindow *window, int depth = 0) {
  std::cerr << std::string(depth, ' ') << wxString(window->GetClassInfo()->GetClassName()).ToStdString()
            << " size=" << window->GetSize().x
            << " min=" << window->GetMinSize().x
            << " best=" << window->GetBestSize().x
            << " label=" << window->GetLabel().Left(60) << std::endl;
  for (auto *child : window->GetChildren())
    DumpSizes(child, depth + 1);
}

wxWindow *Page(wxTreebook *book, const wxString &name) {
  for (std::size_t i = 0; i < book->GetPageCount(); ++i) {
    if (book->GetPageText(i) == name) {
      book->SetSelection(i);
      wxTheApp->Yield();
      return book->GetPage(i);
    }
  }
  assert(false && "Missing preferences page");
  return nullptr;
}

void SendButton(PreferencesDialog &dialog, int id) {
  wxCommandEvent event(wxEVT_BUTTON, id);
  dialog.ProcessWindowEvent(event);
  wxTheApp->ProcessPendingEvents();
}

void Select(wxChoice *choice, int index, bool notify = false) {
  choice->SetSelection(index);
  if (notify) {
    wxCommandEvent event(wxEVT_CHOICE, choice->GetId());
    event.SetEventObject(choice);
    choice->ProcessWindowEvent(event);
  }
}

int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  auto *frame = new wxFrame(nullptr, wxID_ANY, "Preferences regression");
  int appliedEvents = 0, unitEvents = 0;
  frame->Bind(EVT_UI_PREFERENCES_APPLIED, [&](wxCommandEvent &) { ++appliedEvents; });
  frame->Bind(EVT_UI_UNITS_CHANGED, [&](wxCommandEvent &) { ++unitEvents; });
  Notices notices;
  notices.Register();
  Preferences preferences;
  preferences.values = {
      {"rider_autopatch", "0"}, {"rider_layer_mode", "type"},
      {"ui_distance_unit_system", "metric"}, {"ui_weight_unit_system", "imperial"},
      {"ui_language", "es"}, {"app_update_startup_mode", "manual_only"},
      {"gdtf_mutation_policy", "preserve_imported"},
      {"mvr_truss_geometry_export_mode", "direct_geometry3d_for_truss_symbols"},
      {"selection_group_move_fixture", "1"}, {"selection_group_move_truss", "0"},
      {"selection_group_move_support", "1"}, {"selection_group_move_scene_object", "1"},
      {"viewport_magnet_show_anchor_references", "0"},
      {"viewer3d_invert_orbit_horizontal", "1"}, {"viewer3d_invert_orbit", "1"},
      {"viewer3d_render_style", "textured"},
      {model_detail::kDetailConfigKey, "high"}, {model_detail::kMovingProxyConfigKey, "1"}};
  for (int i = 1; i <= 6; ++i)
    for (const auto *suffix : {"height", "pos", "margin"})
      preferences.floats["rider_lx" + std::to_string(i) + "_" + suffix] = 2.5f * i;
  const bool localizedBuild = localization::SupportedAppLanguages().size() > 1;
  const int expectedRestartNotices = localizedBuild ? 1 : 0;
  const auto originalValues = preferences.values;
  const auto originalFloats = preferences.floats;
  auto commit = [&](const std::function<void()> &apply) {
    apply();
    return preferences.SaveUserConfig();
  };
  assert(CacheService::EnsureDirectories());
  const auto cachedFile = AppPaths::GetGdtfCatalogCachePath();
  std::ofstream(cachedFile) << "cache";
  {
    PreferencesDialog dialog(frame, preferences, commit);
    dialog.Show();
    wxTheApp->Yield();
    auto *book = FindControls<wxTreebook>(&dialog).at(0);
    assert(book->GetPageCount() == 13);
    assert(book->GetPageText(0) == "General");
    assert(book->GetPageText(5) == "Import");
    assert(book->GetPageText(7) == "Viewer");
    assert(book->GetPageText(10) == "Formats");
    assert(preferencesTestCredentialLoads == 1);
    auto cacheFields = FindControls<wxTextCtrl>(Page(book, "Cache"));
    assert(cacheFields.size() == 1 && !cacheFields[0]->IsEditable());
    auto *cachePage = Page(book, "Cache");
    auto cacheButtons = FindControls<wxButton>(cachePage);
    auto *sizeLabel = FindControls<wxStaticText>(cachePage).at(1);
    const auto initialCacheSize = sizeLabel->GetLabel();
    assert(cacheButtons.size() == 2);
    auto clearCache = [&] {
      wxCommandEvent event(wxEVT_BUTTON, cacheButtons[0]->GetId());
      cacheButtons[0]->ProcessWindowEvent(event);
    };
    clearCache();
    assert(notices.cacheNotices == 1 && std::filesystem::exists(cachedFile));
    notices.cacheResponse = wxID_YES;
    clearCache();
    assert(notices.cacheNotices == 2 && !std::filesystem::exists(cachedFile));
    assert(CacheService::GetStatistics().bytes == 0);
    assert(sizeLabel->GetLabel() != initialCacheSize);
    assert(preferences.saves == 0 && preferences.values == originalValues);
    auto riderChecks = FindControls<wxCheckBox>(Page(book, "Rider Import"));
    assert(!riderChecks.at(0)->GetValue());
    auto riderRadios = FindControls<wxRadioButton>(Page(book, "Rider Import"));
    assert(riderRadios.at(1)->GetValue());
    auto riderFields = FindControls<wxTextCtrl>(Page(book, "Rider Import"));
    assert(riderFields.size() == 18);
    for (int i = 0; i < 18; ++i) {
      const auto parsed = Units::ParseDistanceToMillimeters(
          riderFields[i]->GetValue().ToStdString(), Units::DistanceUnitSystem::Metric);
      assert(parsed && std::abs(*parsed - 2500.0 * (i / 3 + 1)) < 0.01);
    }
    auto unitChoices = FindControls<wxChoice>(Page(book, "Units"));
    assert(unitChoices.at(0)->GetSelection() == 0);
    assert(unitChoices.at(1)->GetSelection() == 1);
    auto languageChoice = FindControls<wxChoice>(Page(book, "Language")).at(0);
    assert(languageChoice->GetSelection() == (localizedBuild ? 1 : 0));
    assert(languageChoice->GetCount() == (localizedBuild ? 5 : 1));
    if (localizedBuild) {
      const char *labels[] = {"English", "Español", "Français", "Deutsch", "简体中文"};
      for (size_t i = 0; i < 5; ++i)
        assert(languageChoice->GetString(i) == wxString::FromUTF8(labels[i]));
    }
    auto updatesChoice = FindControls<wxChoice>(Page(book, "Updates")).at(0);
    assert(updatesChoice->GetSelection() == 1);
    assert(FindControls<wxChoice>(Page(book, "GDTF")).empty());
    auto mvrChoice = FindControls<wxChoice>(Page(book, "MVR Import / Export")).at(0);
    assert(mvrChoice->GetSelection() == 1);
    auto selectionChecks = FindControls<wxCheckBox>(Page(book, "Selection & Movement"));
    assert(selectionChecks.size() == 5);
    assert(selectionChecks[0]->GetValue() && !selectionChecks[1]->GetValue());
    assert(selectionChecks[2]->GetValue() && selectionChecks[3]->GetValue());
    assert(!selectionChecks[4]->GetValue());
    auto viewerChecks = FindControls<wxCheckBox>(Page(book, "3D Viewer"));
    assert(viewerChecks.size() == 3);
    for (auto *check : viewerChecks)
      assert(check->GetValue());
    auto detailChoice = FindControls<wxChoice>(Page(book, "3D Viewer")).at(0);
    assert(detailChoice->GetSelection() == 2);
    auto renderRadios = FindControls<wxRadioButton>(Page(book, "3D Viewer"));
    assert(renderRadios.at(3)->GetValue());
    assert(preferences.values == originalValues && preferences.floats == originalFloats);
    assert(preferences.saves == 0 && appliedEvents == 0 && unitEvents == 0);

    // Edits in every page remain staged while switching/expanding categories.
    riderChecks[0]->SetValue(true);
    riderRadios[0]->SetValue(true);
    riderFields[0]->ChangeValue("4.5");
    Select(unitChoices[0], 1, true);
    Select(unitChoices[1], 0);
    Select(languageChoice, localizedBuild ? 4 : 0);
    Select(updatesChoice, 0);
    Select(mvrChoice, 0);
    for (auto *check : selectionChecks)
      check->SetValue(!check->GetValue());
    for (auto *check : viewerChecks)
      check->SetValue(false);
    Select(detailChoice, 0, true);
    assert(viewerChecks[2]->GetValue() && !viewerChecks[2]->IsEnabled());
    renderRadios[4]->SetValue(true);
    for (std::size_t i = 0; i < book->GetPageCount(); ++i)
      book->SetSelection(i);
    assert(detailChoice->GetSelection() == 0 && riderChecks[0]->GetValue());
    assert(preferences.values == originalValues && preferences.floats == originalFloats);

    // Resize all pages, including the long explanatory text and Rider grid.
    for (const auto size : {wxSize(760, 540), wxSize(1200, 850), wxSize(940, 680)}) {
      dialog.SetSize(dialog.FromDIP(size));
      for (std::size_t i = 0; i < book->GetPageCount(); ++i) {
        book->SetSelection(i);
        wxTheApp->Yield();
        if (auto *page = book->GetPage(i)) {
          assert(page->GetClientSize().x > 300);
          if (page->GetVirtualSize().x > page->GetClientSize().x) {
            DumpSizes(page);
            std::cerr << book->GetPageText(i) << ": client=" << page->GetClientSize().x
                      << " virtual=" << page->GetVirtualSize().x
                      << " minimum=" << page->GetSizer()->GetMinSize().x << std::endl;
          }
          assert(page->GetVirtualSize().x <= page->GetClientSize().x);
        }
      }
    }
    SendButton(dialog, wxID_APPLY);
    assert(dialog.IsShown());
    assert(preferences.saves == 1 && appliedEvents == 1 && unitEvents == 1);
    assert(preferencesTestCredentialApplies == 1);
    assert(notices.restartNotices == expectedRestartNotices);
    assert(preferences.GetValue("rider_autopatch") == "1");
    assert(preferences.GetValue("rider_layer_mode") == "position");
    // Existing Label formatting rounds to 0.01 ft (at most 1.524 mm error).
    assert(std::abs(preferences.GetFloat("rider_lx1_height") - 4.5f) < 0.001525f);
    assert(preferences.GetValue("ui_distance_unit_system") == "imperial");
    assert(preferences.GetValue("ui_weight_unit_system") == "metric");
    assert(preferences.GetValue("ui_language") == (localizedBuild ? "zh_CN" : "en"));
    assert(preferences.GetValue("app_update_startup_mode") == "startup");
    assert(preferences.GetValue("gdtf_mutation_policy") == "preserve_imported");
    assert(preferences.GetValue("mvr_truss_geometry_export_mode") == "standard");
    assert(preferences.GetValue("selection_group_move_fixture") == "0");
    assert(preferences.GetValue("selection_group_move_truss") == "1");
    assert(preferences.GetValue("selection_group_move_support") == "0");
    assert(preferences.GetValue("selection_group_move_scene_object") == "0");
    assert(preferences.GetValue("viewport_magnet_show_anchor_references") == "1");
    assert(preferences.GetValue("viewer3d_invert_orbit_horizontal") == "0");
    assert(preferences.GetValue("viewer3d_invert_orbit") == "0");
    assert(preferences.GetValue("viewer3d_render_style") == "wireframe");
    assert((model_detail::ReadPreferences(preferences) ==
            model_detail::Preferences{model_detail::Level::Low, true}));
    assert(preferences.GetValue(model_detail::kMovingProxyConfigKey) == "1");
    SendButton(dialog, wxID_APPLY);
    assert(appliedEvents == 2 && unitEvents == 1 && notices.restartNotices == expectedRestartNotices);
    const auto appliedValues = preferences.values;
    const auto appliedFloats = preferences.floats;
    Select(detailChoice, 2, true);
    assert(viewerChecks[2]->GetValue() && viewerChecks[2]->IsEnabled());
    riderFields[0]->ChangeValue("100");
    Select(languageChoice, localizedBuild ? 1 : 0);
    dialog.Hide();
    wxTheApp->CallAfter([&] { SendButton(dialog, wxID_CANCEL); });
    assert(dialog.ShowModal() == wxID_CANCEL);
    assert(preferences.values == appliedValues && preferences.floats == appliedFloats);
    assert(preferences.saves == 2 && appliedEvents == 2 && unitEvents == 1);
    assert(notices.restartNotices == expectedRestartNotices);
  }
  // Cancel a fresh dialog with edits on every page before any Apply.
  const auto appliedValues = preferences.values;
  const auto appliedFloats = preferences.floats;
  {
    PreferencesDialog dialog(frame, preferences, commit);
    for (auto *choice : FindControls<wxChoice>(&dialog))
      Select(choice, (choice->GetSelection() + 1) % choice->GetCount());
    for (auto *check : FindControls<wxCheckBox>(&dialog))
      check->SetValue(!check->GetValue());
    for (auto *field : FindControls<wxTextCtrl>(&dialog))
      field->ChangeValue("99");
    wxTheApp->CallAfter([&] { SendButton(dialog, wxID_CANCEL); });
    assert(dialog.ShowModal() == wxID_CANCEL);
    assert(preferences.values == appliedValues && preferences.floats == appliedFloats);
    assert(preferences.saves == 2);
  }
  // Each release language survives page recreation and applies its canonical code.
  if (localizedBuild) {
    Preferences languagePreferences;
    for (const auto &option : localization::SupportedAppLanguages()) {
      languagePreferences.SetValue("ui_language", std::string(option.code));
      LanguagePreferencesPage page(frame);
      page.LoadPreferences(languagePreferences);
      assert(page.SelectedLanguage() == option.language);
      const auto *choice = FindControls<wxChoice>(&page).at(0);
      LanguagePreferencesPage changedPage(frame);
      languagePreferences.SetValue("ui_language", "en");
      changedPage.LoadPreferences(languagePreferences);
      Select(FindControls<wxChoice>(&changedPage).at(0), choice->GetSelection());
      changedPage.ApplyPreferences(languagePreferences);
      assert(languagePreferences.GetValue("ui_language") == option.code);
      LanguagePreferencesPage recreatedPage(frame);
      recreatedPage.LoadPreferences(languagePreferences);
      assert(recreatedPage.SelectedLanguage() == option.language);
    }
  }
  // OK commits pending edits and closes using the same notification path.
  {
    PreferencesDialog dialog(frame, preferences, commit);
    auto *book = FindControls<wxTreebook>(&dialog).at(0);
    Select(FindControls<wxChoice>(Page(book, "3D Viewer")).at(0), 2);
    wxTheApp->CallAfter([&] { SendButton(dialog, wxID_OK); });
    assert(dialog.ShowModal() == wxID_OK);
    assert(preferences.saves == 3 && appliedEvents == 3 && unitEvents == 1);
    assert(model_detail::ReadPreferences(preferences).level == model_detail::Level::High);
  }
  // Exercise actual catalog loading and actual standard-ID Preferences buttons.
  if (localizedBuild) {
    const std::map<std::string, std::vector<wxString>> labels = {
        {"en", {"Apply", "OK", "Cancel"}},
        {"es", {wxString::FromUTF8("Aplicar"), "Aceptar", "Cancelar"}},
        {"fr", {"Appliquer", "OK", "Annuler"}},
        {"de", {"Anwenden", "OK", "Abbrechen"}},
        {"zh_CN", {wxString::FromUTF8("应用"), wxString::FromUTF8("确定"),
                   wxString::FromUTF8("取消")}}};
    auto &manager = localization::LocalizationManager::Get();
    for (const auto &option : localization::SupportedAppLanguages()) {
      const auto loaded = manager.Initialize(option.language);
      assert(loaded.activeLanguage == option.language);
      assert(option.code == "en" || loaded.catalogLoaded);
      assert(!wxTranslations::Get() || !wxTranslations::Get()->IsLoaded("wxstd"));
      PreferencesDialog dialog(frame, preferences, commit);
      const auto &expected = labels.at(std::string(option.code));
      std::size_t index = 0;
      for (const int id : {wxID_APPLY, wxID_OK, wxID_CANCEL}) {
        const auto *button = dynamic_cast<wxButton *>(dialog.FindWindow(id));
        assert(button && button->GetId() == id);
        if (button->GetLabelText() != expected.at(index))
          std::cerr << option.code << " button " << id << ": "
                    << button->GetLabelText() << '\n';
        assert(button->GetLabelText() == expected.at(index++));
      }
    }
    manager.Initialize(localization::AppLanguage::English);
  }
  std::filesystem::remove_all(cacheTestDir);
  delete frame;
  wxTheApp->OnExit();
  wxEntryCleanup();
}
