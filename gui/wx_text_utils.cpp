#include "wx_text_utils.h"

#include "utf8_utils.h"

namespace wxtext {

// Converts wxWidgets text to the application UTF-8 string contract.
std::string ToUtf8(const wxString &text) {
  const wxScopedCharBuffer buffer = text.ToUTF8();
  return buffer ? std::string(buffer.data(), buffer.length()) : std::string();
}

// Converts validated UTF-8 application text into wxWidgets text.
wxString FromUtf8(std::string_view text) {
  if (!IsValidUtf8(text))
    return wxString();
  return wxString::FromUTF8(text.data(), text.size());
}

} // namespace wxtext
