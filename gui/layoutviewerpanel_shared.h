/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <vector>

#include <wx/font.h>
#include <wx/string.h>

namespace layoutviewerpanel {
namespace detail {
// Native application chrome font preferences.
inline const std::vector<const char *> kUiFontFaceNames = {
#ifdef _WIN32
    "Arial",
#elif defined(__APPLE__)
    "Arial",
#else
    "DejaVu Sans",
    "Liberation Sans",
#endif
};

constexpr int kTextDefaultFontSize = 12;

inline wxString ResolveDefaultUiFontFaceName() {
  static wxString faceName;
  static bool initialized = false;
  if (initialized)
    return faceName;
  for (const char *candidate : kUiFontFaceNames) {
    wxFont testFont(10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                    wxFONTWEIGHT_NORMAL, false,
                    wxString::FromUTF8(candidate));
    if (testFont.IsOk() &&
        testFont.GetFaceName().CmpNoCase(candidate) == 0) {
      faceName = testFont.GetFaceName();
      break;
    }
  }
  initialized = true;
  return faceName;
}

} // namespace detail
} // namespace layoutviewerpanel
