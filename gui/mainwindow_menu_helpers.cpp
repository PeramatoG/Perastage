#include "mainwindow_menu_helpers.h"

#include <cstring>

#include "mainwindow_menu_text_utils.h"

namespace menu {

// Splits help markdown using language markers while preserving a safe fallback for missing sections.
HelpMarkdown SplitHelpMarkdown(const std::string &markdown) {
  constexpr const char *kEnglishMarker = "<!-- LANG:en -->";
  constexpr const char *kSpanishMarker = "<!-- LANG:es -->";
  HelpMarkdown result;

  const auto enPos = markdown.find(kEnglishMarker);
  const auto esPos = markdown.find(kSpanishMarker);
  if (enPos == std::string::npos && esPos == std::string::npos) {
    result.english = markdown;
    result.spanish = markdown;
    return result;
  }

  result.hasSections = true;
  auto extract = [&](size_t start, size_t end, const char *marker) {
    if (start == std::string::npos)
      return std::string();
    start += std::strlen(marker);
    if (end == std::string::npos || end < start)
      end = markdown.size();
    return TrimLeadingWhitespace(markdown.substr(start, end - start));
  };

  if (enPos != std::string::npos && esPos != std::string::npos) {
    if (enPos < esPos) {
      result.english = extract(enPos, esPos, kEnglishMarker);
      result.spanish = extract(esPos, std::string::npos, kSpanishMarker);
    } else {
      result.spanish = extract(esPos, enPos, kSpanishMarker);
      result.english = extract(enPos, std::string::npos, kEnglishMarker);
    }
  } else {
    result.english =
        extract(enPos, std::string::npos, kEnglishMarker);
    result.spanish =
        extract(esPos, std::string::npos, kSpanishMarker);
  }

  if (result.english.empty())
    result.english = markdown;
  if (result.spanish.empty())
    result.spanish = markdown;

  return result;
}

} // namespace menu
