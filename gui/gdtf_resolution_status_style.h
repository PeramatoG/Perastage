#pragma once

#include "../core/rider_fixture_resolution.h"

#include <wx/colour.h>

// Returns the established GDTF workflow colour for a semantic resolution status.
inline wxColour GdtfResolutionStatusColour(
    rider_fixture_resolution::StatusSemantic semantic) {
  switch (semantic) {
  case rider_fixture_resolution::StatusSemantic::Success:
    return wxColour(30, 120, 60);
  case rider_fixture_resolution::StatusSemantic::Warning:
    return wxColour(150, 100, 0);
  case rider_fixture_resolution::StatusSemantic::Muted:
    return wxColour(130, 130, 130);
  case rider_fixture_resolution::StatusSemantic::Modified:
  case rider_fixture_resolution::StatusSemantic::Information:
    return wxColour(20, 80, 160);
  case rider_fixture_resolution::StatusSemantic::Neutral:
    return wxColour(80, 80, 80);
  }
  return wxColour(80, 80, 80);
}
