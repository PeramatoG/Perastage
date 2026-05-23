#pragma once

#include <string>

namespace menu {

struct HelpMarkdown {
  std::string english;
  std::string spanish;
  bool hasSections = false;
};

// Splits markdown into language-specific sections and falls back to full content when needed.
HelpMarkdown SplitHelpMarkdown(const std::string &markdown);

} // namespace menu
