#pragma once

#include <string>

class wxString;

// Converts a wxString value to a UTF-8 std::string.
std::string WxToUtf8(const wxString &value);

// Wraps HTML body content in a UTF-8 HTML document shell.
std::string WrapHelpHtml(const std::string &body);
