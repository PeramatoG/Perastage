#include "user_manual/console_help.h"
#include "user_manual/manual_resolver.h"
#include "markdown.h"

namespace user_manual {
std::optional<std::string> ExtractConsoleHelp(std::string_view markdown) {
  const auto begin = markdown.find(kConsoleHelpBegin);
  const auto end = markdown.find(kConsoleHelpEnd);
  if (begin == markdown.npos || end == markdown.npos || begin >= end ||
      markdown.find(kConsoleHelpBegin, begin + kConsoleHelpBegin.size()) != markdown.npos ||
      markdown.find(kConsoleHelpEnd, end + kConsoleHelpEnd.size()) != markdown.npos)
    return {};
  const auto section = markdown.substr(begin + kConsoleHelpBegin.size(),
                                      end - begin - kConsoleHelpBegin.size());
  if (section.find_first_not_of(" \t\r\n") == section.npos) return {};
  return std::string(section);
}

std::string LoadConsoleHelp(const std::filesystem::path &manualRoot) {
  const auto page = ManualResolver(manualRoot, "en").Load(kConsoleHelpPage);
  if (page) {
    const auto section = ExtractConsoleHelp(page->markdown);
    if (section) {
      const auto text = MarkdownToPlainText(*section);
      if (!text.empty()) return text;
    }
  }
  return kConsoleHelpUnavailable;
}
} // namespace user_manual
