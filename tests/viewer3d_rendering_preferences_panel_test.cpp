#include "preferences/viewer3d_rendering_preferences_panel.h"
#include "guiconfigservices.h"
#include "model_detail_policy.h"
#include <cassert>
#include <map>
#include <wx/app.h>
#include <wx/frame.h>

class TestApp : public wxApp { public: bool OnInit() override { return true; } };
wxIMPLEMENT_APP_NO_MAIN(TestApp);

class Preferences : public IGuiPreferencesService {
public:
  std::map<std::string, std::string> values;
  void SetValue(const std::string &key, const std::string &value) override { values[key] = value; }
  std::optional<std::string> GetValue(const std::string &key) const override {
    const auto it = values.find(key);
    return it == values.end() ? std::nullopt : std::optional<std::string>(it->second);
  }
  void RemoveKey(const std::string &key) override { values.erase(key); }
  bool SaveUserConfig() const override { return true; }
  float GetFloat(const std::string &) const override { return 0; }
  void SetFloat(const std::string &, float) override {}
};

int main(int argc, char **argv) {
  assert(wxEntryStart(argc, argv));
  assert(wxTheApp->CallOnInit());
  auto *frame = new wxFrame(nullptr, wxID_ANY, "Preferences test");
  auto *panel = new Viewer3DRenderingPreferencesPanel(frame);
  Preferences preferences;
  panel->LoadPreferences(preferences);
  panel->ApplyPreferences(preferences);
  assert(model_detail::ReadPreferences(preferences) == model_detail::Preferences{});
  assert(preferences.GetValue("viewer3d_render_style") == "standard");
  for (const auto level : {model_detail::Level::Low, model_detail::Level::Standard,
                           model_detail::Level::High}) {
    for (const bool proxy : {false, true}) {
      for (const auto style : {"standard", "white", "white_model", "textured",
                              "wireframe", "by_device_type", "by_layer", "by_universe"}) {
        model_detail::SavePreferences(preferences, {level, proxy});
        preferences.SetValue("viewer3d_render_style", style);
        panel->LoadPreferences(preferences);
        Preferences applied;
        panel->ApplyPreferences(applied);
        assert((model_detail::ReadPreferences(applied) == model_detail::Preferences{level, proxy}));
        assert(applied.GetValue("viewer3d_render_style") == style);
      }
    }
  }
  preferences.SetValue(model_detail::kDetailConfigKey, "unsupported");
  preferences.SetValue(model_detail::kMovingProxyConfigKey, "unsupported");
  panel->LoadPreferences(preferences);
  panel->ApplyPreferences(preferences);
  assert(preferences.GetValue(model_detail::kDetailConfigKey) == "standard");
  assert(preferences.GetValue(model_detail::kMovingProxyConfigKey) == "0");
  delete frame;
  wxTheApp->OnExit();
  wxEntryCleanup();
}
