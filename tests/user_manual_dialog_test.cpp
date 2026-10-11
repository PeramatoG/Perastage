#include "user_manual_dialog.h"
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
int main(int argc, char **argv) {
  if (argc != 2) return 1;
  const auto root = std::filesystem::temp_directory_path() /
      ("perastage-dialog-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root / "en");
  for (const auto &entry : std::filesystem::directory_iterator(argv[1]))
    if (entry.path().extension() == ".md")
      std::filesystem::copy_file(entry.path(), root / "en" / entry.path().filename());
  if (!wxEntryStart(argc, argv)) return 1;
  if (!wxTheApp->CallOnInit()) return 1;
  int result = 0;
  try {
    for (const std::string locale : {"en", "es", "zh_CN"}) {
      std::string openedUrl;
      UserManualDialog dialog(nullptr, root, locale,
          [&](const std::string &url) { openedUrl = url; });
      dialog.Show(); wxTheApp->Yield();
      Check(dialog.CurrentPage().name == "index.md", "Entry page");
      Check(dialog.CurrentPage().locale == "en", "Absent mirrors use English");
      Check(dialog.CurrentPage().englishFallback == (localization::ParseAppLanguageCode(locale) != localization::AppLanguage::English), "Fallback flag");
      wxHtmlWindow *html = nullptr;
      for (auto *child : dialog.GetChildren())
        if (auto *window = dynamic_cast<wxHtmlWindow *>(child)) html = window;
      Check(html != nullptr, "HTML window exists");
      // Dispatch the real wxHtmlWindow event to verify default file navigation is intercepted.
      wxHtmlLinkEvent event(html->GetId(), wxHtmlLinkInfo("quick-start.md", ""));
      event.SetEventObject(html);
      Check(html->GetEventHandler()->ProcessEvent(event), "Link event handled");
      Check(dialog.CurrentPage().name == "quick-start.md", "Local event navigation");
      Check(dialog.NavigateLink("opening-mvr-files.md#export-mvr"), "Page with fragment");
      Check(dialog.NavigateLink("#export-mvr"), "Fragment navigation");
      Check(dialog.GoHome(), "Home");
      Check(dialog.GoBack(), "Back after Home");
      Check(dialog.CurrentPage().name == "opening-mvr-files.md" &&
            dialog.CurrentPage().fragment == "export-mvr", "Back restores page and fragment");
      Check(dialog.GoBack() && dialog.CurrentPage().name == "quick-start.md", "Back stack");
      Check(dialog.NavigateLink("preferences.md#selection--movement"), "Punctuation heading");
      Check(dialog.NavigateLink("#selection--movement"), "Canonical punctuation anchor");
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
    std::cout << "GUI manual navigation and en/es/zh_CN fallback passed\n";
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; result = 1; }
  wxTheApp->OnExit(); wxEntryCleanup();
  std::filesystem::remove_all(root);
  return result;
}
