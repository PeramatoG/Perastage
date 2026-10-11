#include "user_manual/console_help.h"
#include "user_manual/manual_resolver.h"
#include "markdown.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace user_manual;
namespace {
void Check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
}
struct Fixture {
  fs::path root = fs::temp_directory_path() / ("perastage-console-help-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Fixture() { fs::create_directories(root); }
  ~Fixture() { std::error_code ec; fs::remove_all(root, ec); }
  void Write(const std::string &locale, const std::string &text) {
    fs::create_directories(root / locale);
    std::ofstream(root / locale / kConsoleHelpPage, std::ios::binary) << text;
  }
};
std::string Marked(const std::string &section) {
  return std::string(kConsoleHelpBegin) + section + std::string(kConsoleHelpEnd);
}
}

int main(int argc, char **argv) {
  try {
    Check(argc == 2, "Supply canonical docs/user directory");
    Fixture fixture;
    for (const auto &entry : fs::directory_iterator(argv[1]))
      if (entry.path().extension() == ".md") {
        fs::create_directories(fixture.root / "en");
        fs::copy_file(entry.path(), fixture.root / "en" / entry.path().filename());
      }
    const auto page = ManualResolver(fixture.root, "en").Load(kConsoleHelpPage);
    Check(page && page->locale == "en", "Canonical English packaged page loads");
    const auto section = ExtractConsoleHelp(page->markdown);
    Check(section.has_value(), "Canonical section extracts");
    const auto start = page->markdown.find(kConsoleHelpBegin) + kConsoleHelpBegin.size();
    Check(*section == page->markdown.substr(start, page->markdown.find(kConsoleHelpEnd) - start),
          "Extraction returns exactly the marked bytes");
    const auto help = LoadConsoleHelp(fixture.root);
    Check(help == MarkdownToPlainText(*section), "Canonical Markdown is the content owner");
    for (const char *command : {"clear", "f 1 thru 5", "pos x <values>",
          "pos <x>,<y>,<z>", "rot x|y|z <values> --group", "pos x ++ 1 --local",
          "rot z ++ 30 --group --local", "rot y ++45 --g -2.5,0,0"})
      Check(help.find(command) != help.npos, command);
    for (const char *marker : {"<!--", "-->", "```", "`", "**", "## ", "| ---", "\\|", "]("})
      Check(help.find(marker) == help.npos, "Formatting must not leak into Console");
    Check(help.find("Global shortcuts") == help.npos &&
          help.find("Focus and priority") == help.npos, "Unmarked content is excluded");

    // A translated page exists and is deliberately different: Console still uses English.
    for (const std::string locale : {"en", "es", "zh_CN"}) {
      if (locale != "en") fixture.Write(locale, Marked("localized placeholder"));
      ManualResolver ui(fixture.root, locale);
      Check(ui.Load().has_value(), "F1 entry remains available with English fallback");
      Check(LoadConsoleHelp(fixture.root) == help, "UI language cannot change Console grammar");
    }
    Check(ExtractConsoleHelp("outside" + Marked("\nrenamed heading\n") + "after") ==
          std::optional<std::string>("\nrenamed heading\n"), "Visible headings are irrelevant");
    for (const std::string broken : {
          std::string(), std::string(kConsoleHelpBegin), std::string(kConsoleHelpEnd),
          std::string(kConsoleHelpEnd) + "body" + std::string(kConsoleHelpBegin),
          std::string(kConsoleHelpBegin) + Marked("body"),
          Marked("body") + std::string(kConsoleHelpEnd), Marked(""), Marked(" \n\t\r")}) {
      Check(!ExtractConsoleHelp(broken), "Missing/duplicate/reversed/empty markers fail safely");
      fixture.Write("en", broken);
      Check(LoadConsoleHelp(fixture.root) == kConsoleHelpUnavailable, "Invalid page uses short fallback");
    }
    for (const char *broken : {"<!-- comment only -->", "```text\nf 1\n", "<!-- unclosed"}) {
      fixture.Write("en", Marked(broken));
      Check(LoadConsoleHelp(fixture.root) == kConsoleHelpUnavailable, "Empty/malformed presentation fails safely");
    }
    // No fallback to a localized command page when the English source is missing.
    fs::remove(fixture.root / "en" / kConsoleHelpPage);
    Check(LoadConsoleHelp(fixture.root) == kConsoleHelpUnavailable, "Missing English page fails safely");
    Check(LoadConsoleHelp(fixture.root / "absent") == kConsoleHelpUnavailable, "Missing offline bundle");
    fixture.Write("en", Marked("# Changed source\n\n`sentinel ++ 7`"));
    Check(LoadConsoleHelp(fixture.root) == "Changed source\n\nsentinel ++ 7",
          "Help comes from the packaged Markdown, without a built-in catalog");
    std::cout << "Console extraction, canonical commands, en/es/zh_CN and failure paths passed\n";
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
