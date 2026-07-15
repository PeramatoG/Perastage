#pragma once

#include <wx/string.h>
#include <wx/window.h>

#include <map>
#include <set>
#include <vector>

struct HangPositionDialogResult {
  wxString selectedName;
  std::map<wxString, wxString> renamedPositions;
  std::set<wxString> deletedPositions;
};

bool ShowHangPositionDialog(wxWindow *parent, const wxString &currentName,
                            HangPositionDialogResult *result);
void ApplySharedHangPositionChanges(
    const HangPositionDialogResult &result, bool updateFixtures,
    const std::vector<unsigned int> &fixtureRows, bool updateTrusses,
    const std::vector<unsigned int> &trussRows, bool updateHoists,
    const std::vector<unsigned int> &hoistRows);
