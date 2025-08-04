#pragma once

#include <string>

// Simple Markdown to HTML converter.
// Supports headers (#, ##, ###), bold (**text**), and unordered lists (- item).
// Designed for lightweight help documentation.
std::string MarkdownToHtml(const std::string &markdown);
