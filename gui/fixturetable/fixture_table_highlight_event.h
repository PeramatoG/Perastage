/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include <wx/event.h>
#include <wx/colour.h>
#include <string>
#include <vector>
namespace gui {
// Presentation colors shared by the fixture table and patch outlines.
inline wxColour FixtureSelectionColor() { return wxColour(0, 255, 255); }
inline wxColour FixtureHoverColor() { return wxColour(170, 220, 0); }
inline wxColour FixtureRelatedHoverColor() { return wxColour(110, 210, 150); }
struct FixtureHoverHighlight {
  std::string primary;
  std::vector<std::string> related;
};
// Observes the existing table/viewer hover path; carries no independent state.
inline const wxEventTypeTag<wxThreadEvent> FixtureHoverChanged(wxNewEventType());
}
