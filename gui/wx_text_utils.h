#pragma once

#include <string>
#include <string_view>
#include <wx/string.h>

namespace wxtext {

std::string ToUtf8(const wxString &text);
wxString FromUtf8(std::string_view text);

} // namespace wxtext
