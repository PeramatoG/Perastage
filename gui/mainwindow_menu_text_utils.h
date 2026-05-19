#pragma once

#include <string>

class wxString;

// Converts a wxString value to a UTF-8 std::string.
std::string WxToUtf8(const wxString &value);

// Removes leading blank-line whitespace from a text block.
std::string TrimLeadingWhitespace(const std::string &text);

// Wraps HTML body content in a UTF-8 HTML document shell.
std::string WrapHelpHtml(const std::string &body);
