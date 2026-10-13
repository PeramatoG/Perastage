#include "user_manual_dialog.h"
#include "projectutils.h"
#include <fstream>
#include <iterator>
#include "localization/app_language.h"
#include <filesystem>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/html/htmlwin.h>

class ManualTestApp : public wxApp {
public:
  bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(ManualTestApp);
void Check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
// Runs from a real staged/install payload, with a working directory elsewhere.
void CheckRuntime(const std::filesystem::path &assets,
                  const std::filesystem::path &source) {
  const auto root = ProjectUtils::ResolveResourcePath("help");
  Check(!root.empty() && std::filesystem::equivalent(root, assets / "help"),
        "MainWindow help lookup must resolve the staged manual root");
  for (const auto &option : localization::SupportedAppLanguages()) {
    const std::string locale(option.code);
    const auto canonical = locale == "en" ? source / "index.md" :
        source / "locales" / locale / "index.md";
    std::ifstream in(canonical, std::ios::binary);
    Check(in.is_open(), "Canonical index must be readable");
    const std::string expected((std::istreambuf_iterator<char>(in)), {});
    UserManualDialog dialog(nullptr, root, locale);
    dialog.Show(); wxTheApp->Yield();
    Check(dialog.CurrentPage().name == "index.md" &&
          dialog.CurrentPage().locale == locale &&
          !dialog.CurrentPage().englishFallback, "Staged localized entry page");
    Check(dialog.CurrentPage().markdown == expected,
          "Dialog must load the actual canonical localized bytes");
    std::cout << "Staged " << locale << "/index.md loaded from " << root << '\n';
  }
}

int main(int argc, char **argv) {
  if (argc == 4 && std::string(argv[1]) == "--runtime") {
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) return 1;
    int result = 0;
    try { CheckRuntime(argv[2], argv[3]); }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; result = 1; }
    wxTheApp->OnExit(); wxEntryCleanup();
    return result;
  }
  if (argc != 2) return 1;
  const auto root = std::filesystem::temp_directory_path() /
      ("perastage-dialog-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root / "en");
  for (const auto &entry : std::filesystem::directory_iterator(argv[1]))
    if (entry.path().extension() == ".md")
      std::filesystem::copy_file(entry.path(), root / "en" / entry.path().filename());
  for (const std::string locale : {"es", "fr", "de", "zh_CN"}) {
    std::filesystem::create_directories(root / locale);
    for (const auto &entry : std::filesystem::directory_iterator(
             std::filesystem::path(argv[1]) / "locales" / locale))
      if (entry.path().extension() == ".md")
        std::filesystem::copy_file(entry.path(), root / locale / entry.path().filename());
  }
  if (!wxEntryStart(argc, argv)) return 1;
  if (!wxTheApp->CallOnInit()) return 1;
  int result = 0;
  try {
    for (const std::string locale : {"en", "es", "fr", "de", "zh_CN"}) {
      std::string openedUrl;
      UserManualDialog dialog(nullptr, root, locale,
          [&](const std::string &url) { openedUrl = url; });
      dialog.Show(); wxTheApp->Yield();
      Check(dialog.CurrentPage().name == "index.md", "Entry page");
      const auto expectedLocale = localization::AppLanguageCode(localization::ParseAppLanguageCode(locale));
      Check(dialog.CurrentPage().locale == expectedLocale, "Localized entry page");
      Check(!dialog.CurrentPage().englishFallback, "Complete entry must not fall back");
      const std::string exportFragment = expectedLocale == "es" ? "exportar-mvr" :
          expectedLocale == "fr" ? "exporter-mvr" :
          expectedLocale == "de" ? "mvr-exportieren" :
          expectedLocale == "zh_CN" ? "导出-mvr" : "export-mvr";
      const std::string movementFragment = expectedLocale == "es" ? "selección-y-movimiento" :
          expectedLocale == "fr" ? "sélection-et-déplacement" :
          expectedLocale == "de" ? "auswahl-und-bewegung" :
          expectedLocale == "zh_CN" ? "选择与移动" : "selection--movement";
      wxHtmlWindow *html = nullptr;
      for (auto *child : dialog.GetChildren())
        if (auto *window = dynamic_cast<wxHtmlWindow *>(child)) html = window;
      Check(html != nullptr, "HTML window exists");
      // Dispatch the real wxHtmlWindow event to verify default file navigation is intercepted.
      wxHtmlLinkEvent event(html->GetId(), wxHtmlLinkInfo("quick-start.md", ""));
      event.SetEventObject(html);
      Check(html->GetEventHandler()->ProcessEvent(event), "Link event handled");
      Check(dialog.CurrentPage().name == "quick-start.md", "Local event navigation");
      Check(dialog.NavigateLink("opening-mvr-files.md#" + exportFragment), "Page with fragment");
      Check(dialog.NavigateLink("#" + exportFragment), "Fragment navigation");
      Check(dialog.GoHome(), "Home");
      Check(dialog.GoBack(), "Back after Home");
      Check(dialog.CurrentPage().name == "opening-mvr-files.md" &&
            dialog.CurrentPage().fragment == exportFragment, "Back restores page and fragment");
      Check(dialog.GoBack() && dialog.CurrentPage().name == "quick-start.md", "Back stack");
      Check(dialog.NavigateLink("preferences.md#" + movementFragment), "Punctuation heading");
      Check(dialog.NavigateLink("#" + movementFragment), "Canonical punctuation anchor");
      Check(!dialog.CurrentPage().englishFallback && dialog.CurrentPage().locale == expectedLocale,
            "Cross-page localized navigation must not fall back");
      // Deliberately remove a translated page to retain page-level resilience coverage.
      if (expectedLocale != "en") {
        std::filesystem::remove(root / expectedLocale / "quick-start.md");
        Check(dialog.NavigateLink("quick-start.md"), "Missing translation navigation");
        Check(dialog.CurrentPage().englishFallback && dialog.CurrentPage().locale == "en",
              "Deliberately removed page uses English fallback");
        std::ofstream(root / expectedLocale / "quick-start.md", std::ios::binary).put('\xFF');
        Check(dialog.NavigateLink("quick-start.md") && dialog.CurrentPage().englishFallback,
              "Unreadable UTF-8 translation uses English fallback");
        std::filesystem::copy_file(std::filesystem::path(argv[1]) / "locales" /
            expectedLocale / "quick-start.md", root / expectedLocale / "quick-start.md",
            std::filesystem::copy_options::overwrite_existing);
        Check(dialog.NavigateLink("quick-start.md") && !dialog.CurrentPage().englishFallback &&
              dialog.CurrentPage().locale == expectedLocale, "Restored page stops English fallback");
      }
      Check(dialog.NavigateLink("https://example.com"), "Browser external link");
      Check(openedUrl == "https://example.com", "External routed to browser");
      Check(dialog.NavigateLink("../developer/build.md"), "Developer link");
      Check(openedUrl == "https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md", "Developer browser URL");
      openedUrl.clear();
      Check(!dialog.NavigateLink("file:///tmp/secret.md") && openedUrl.empty(), "file scheme blocked");
      Check(!dialog.NavigateLink("../../secret.md"), "Traversal blocked");
      Check(!dialog.NavigateLink("missing.md"), "Missing page error");
      Check(dialog.GoHome(), "Home recovers from missing page");
      dialog.Hide();
    }
    std::string missingOnlineUrl;
    UserManualDialog missing(nullptr, root / "absent", "es",
        [&](const std::string &url) { missingOnlineUrl = url; });
    Check(missing.CurrentPage().name.empty(), "Missing offline entry handled");
    bool onlineAvailable = false;
    for (auto *child : missing.GetChildren())
      if (auto *button = dynamic_cast<wxButton *>(child);
          button && button->GetLabel() == "Online Documentation" && button->IsEnabled()) {
        onlineAvailable = true;
        wxCommandEvent event(wxEVT_BUTTON, button->GetId());
        event.SetEventObject(button);
        button->GetEventHandler()->ProcessEvent(event);
      }
    Check(onlineAvailable, "Online route available without offline files");
    Check(missingOnlineUrl == user_manual::kOnlineDocumentation,
          "Online Documentation button must use the canonical destination");
    std::cout << "GUI complete en/es/fr/de/zh_CN navigation and deliberate page-removal fallback passed\n";
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; result = 1; }
  wxTheApp->OnExit(); wxEntryCleanup();
  std::filesystem::remove_all(root);
  return result;
}
