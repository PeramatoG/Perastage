#include "mainwindow_menu_text_utils.h"

#include <wx/string.h>

// Converts a wxString value to a UTF-8 std::string.
std::string WxToUtf8(const wxString &value) {
  const wxScopedCharBuffer utf8 = value.ToUTF8();
  if (utf8)
    return std::string(utf8.data(), utf8.length());
  return value.ToStdString();
}

// Removes leading blank-line whitespace from a text block.
std::string TrimLeadingWhitespace(const std::string &text) {
  const auto start = text.find_first_not_of("\r\n");
  if (start == std::string::npos)
    return std::string();
  return text.substr(start);
}

// Wraps HTML body content in a UTF-8 HTML document shell.
std::string WrapHelpHtml(const std::string &body) {
  return "<html><head><meta charset=\"UTF-8\"></head><body>" + body +
         "</body></html>";
}
