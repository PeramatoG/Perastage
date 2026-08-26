#pragma once

#include "../core/rider_fixture_resolution.h"

#include <wx/colour.h>
#include <wx/app.h>
#include <wx/settings.h>

// Reports whether the current system window palette is predominantly dark.
inline bool IsDarkGdtfResolutionAppearance() {
  if (!wxTheApp)
    return false;
  const wxColour background =
      wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
  const int luminance = 299 * background.Red() + 587 * background.Green() +
                        114 * background.Blue();
  return luminance < 128000;
}

// Returns the established GDTF workflow colour for an explicit appearance.
inline wxColour GdtfResolutionStatusColour(
    rider_fixture_resolution::StatusSemantic semantic, bool dark) {
  switch (semantic) {
  case rider_fixture_resolution::StatusSemantic::Success:
    return dark ? wxColour(100, 210, 125) : wxColour(30, 120, 60);
  case rider_fixture_resolution::StatusSemantic::Warning:
    return dark ? wxColour(240, 180, 70) : wxColour(150, 100, 0);
  case rider_fixture_resolution::StatusSemantic::Muted:
    return dark ? wxColour(155, 155, 155) : wxColour(105, 105, 105);
  case rider_fixture_resolution::StatusSemantic::Modified:
    return dark ? wxColour(195, 145, 255) : wxColour(105, 45, 160);
  case rider_fixture_resolution::StatusSemantic::Information:
    return dark ? wxColour(95, 175, 255) : wxColour(20, 80, 160);
  case rider_fixture_resolution::StatusSemantic::Neutral:
    return dark ? wxColour(235, 235, 235) : wxColour(35, 35, 35);
  }
  return dark ? wxColour(235, 235, 235) : wxColour(35, 35, 35);
}

// Returns the established GDTF workflow colour for the current appearance.
inline wxColour GdtfResolutionStatusColour(
    rider_fixture_resolution::StatusSemantic semantic) {
  return GdtfResolutionStatusColour(semantic,
                                    IsDarkGdtfResolutionAppearance());
}
