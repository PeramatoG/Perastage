#pragma once

#include <string>
#include <string_view>
#include <wx/string.h>

std::string WxToUtf8(const wxString &text);
wxString Utf8ToWx(std::string_view text);
