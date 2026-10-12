#include "user_manual/manual_resolver.h"
#include "filesystem_path_utils.h"
#include "localization/app_language.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace user_manual;
void Check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
struct Fixture {
  fs::path root = fs::temp_directory_path() / ("perastage-manual-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Fixture() { fs::create_directories(root); }
  ~Fixture() { std::error_code ec; fs::remove_all(root, ec); }
  void Write(const std::string &name, const std::string &text) {
    const auto path = root / PathUtils::PathFromUtf8(name);
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
  }
};
int main() {
  try {
    Check(std::string_view(kOnlineDocumentation).ends_with("/documentation.html"),
          "Online Documentation must target the documentation hub");
    Fixture fixture;
    fixture.Write("en/index.md", "# English");
    fixture.Write("en/next.md", "# Next");
    fixture.Write("en/café.md", "中文 & español");
    fixture.Write("es/index.md", "# Español");
    fixture.Write("fr/index.md", "# Français");
    fixture.Write("de/index.md", "# Deutsch");
    fixture.Write("zh_CN/index.md", "# 中文");
    ManualResolver english(fixture.root, "en"), spanish(fixture.root, "es"),
        chinese(fixture.root, "zh-Hans"), unknown(fixture.root, "../evil");
    Check(english.Load()->markdown == "# English", "English entry");
    Check(!english.Load()->englishFallback, "English is not fallback");
    Check(spanish.Load()->markdown == (PERASTAGE_ENABLE_LOCALIZATION ? "# Español" : "# English"), "Spanish preferred");
    Check(chinese.Load()->markdown == (PERASTAGE_ENABLE_LOCALIZATION ? "# 中文" : "# English"), "Chinese alias and preference");
    for (const auto code : {"fr", "de"}) {
      ManualResolver resolver(fixture.root, code);
      Check(resolver.RequestedLocale() == (PERASTAGE_ENABLE_LOCALIZATION ? code : "en"),
            "French/German canonical manual locale");
      Check(!resolver.Load()->englishFallback, "Complete French/German entry");
    }
    Check(unknown.RequestedLocale() == "en", "Unknown locale canonicalizes to English");
    for (const auto *resolver : {&spanish, &chinese}) {
      const auto requested = resolver->RequestedLocale();
      const auto page = resolver->Load("next.md", "part");
      Check(page && page->englishFallback == (requested != "en") && page->locale == "en", "Page fallback");
      Check(page->fragment == "part", "Load preserves fragment");
      Check(resolver->RequestedLocale() == requested, "Fallback does not change requested locale");
    }
    for (const std::string locale : {"es", "fr", "de", "zh_CN"}) {
      fixture.Write(locale + "/next.md", "# Localized next");
      ManualResolver resolver(fixture.root, locale);
      Check(!resolver.Load("next.md")->englishFallback, "Existing localized page resolves normally");
      fs::remove(fixture.root / locale / "next.md");
      Check(resolver.Load("next.md")->englishFallback == (resolver.RequestedLocale() != "en"),
            "Deliberately removed localized page falls back");
    }
    Check(!english.Load("missing.md"), "Missing page");
    Check(english.Load("café.md")->markdown == "中文 & español", "UTF-8 path and content");
    const auto relative = english.ResolveLink("section/current.md", "../next.md#part");
    Check(relative.kind == LinkKind::Manual && relative.target == "next.md" &&
          relative.fragment == "part", "Relative link with fragment");
    Check(english.ResolveLink("index.md", "#part").kind == LinkKind::Fragment, "Fragment-only link");
    Check(english.ResolveLink("index.md", "caf%C3%A9.md").target == "café.md", "URL encoded UTF-8");
    const auto developer = english.ResolveLink("index.md", "../developer/build.md#linux");
    Check(developer.kind == LinkKind::Online && developer.target ==
          "https://github.com/PeramatoG/Perastage/blob/main/docs/developer/build.md#linux", "Online developer link");
    Check(english.ResolveLink("index.md", "https://example.com").kind == LinkKind::Online, "HTTPS");
    Check(english.ResolveLink("index.md", "HTTP://example.com").kind == LinkKind::Online, "HTTP case");
    for (const char *target : {"../secret.md", "../../secret.md", "/tmp/file.md",
          "C:/file.md", "C:\\file.md", "file:///tmp/file.md", "//host/file.md",
          "javascript:alert(1)", "invalid%FF.md", "%2e%2e/secret.md", "%2Ftmp/file.md", "bad%00.md",
          "../developer/../../secret.md", "../developer/build.md?raw=1"}) {
      Check(english.ResolveLink("index.md", target).kind == LinkKind::Rejected, target);
    }
    for (const char *page : {"../secret.md", "/tmp/file.md", "C:/file.md", "x\\y.md"})
      Check(!english.Load(page), page);
    Check(!english.Load("invalid\xFF.md"), "Malformed UTF-8 path rejected");
    fixture.Write("es/next.md", "\xFF");
    Check(spanish.Load("next.md")->locale == "en", "Invalid UTF-8 page falls back");
    fixture.Write("outside.md", "secret");
    std::error_code ec;
    fs::create_symlink(fixture.root / "outside.md", fixture.root / "en/escape.md", ec);
    if (!ec) Check(!english.Load("escape.md"), "Symlink page escape");
    fs::remove_all(fixture.root / "es", ec);
    fs::create_directory_symlink(fixture.root.parent_path(), fixture.root / "es", ec);
    if (!ec) Check(spanish.Load()->englishFallback == (spanish.RequestedLocale() != "en"), "Symlink locale escape falls back");
    // Unreadable/non-file localized pages also fall back.
    fs::create_directories(fixture.root / "zh_CN/next.md");
    Check(chinese.Load("next.md")->englishFallback == (chinese.RequestedLocale() != "en"), "Localized directory is not a page");
    std::cout << "Manual resolver passed\n";
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
