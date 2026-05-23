#pragma once

#include <string>

// Stores per-language help markdown fragments and section metadata.
struct HelpMarkdown {
  std::string english;
  std::string spanish;
  bool hasSections = false;
};

// Splits bilingual help markdown into per-language content blocks.
HelpMarkdown SplitHelpMarkdown(const std::string &markdown);
