#pragma once

#include "../viewer3d/resources/progressive_load_policy.h"
#include <wx/string.h>

class wxWindow;

// Only restores status text owned by this indicator; other operation messages
// survive completion. All access stays on the GUI event thread.
class ViewerLoadStatus {
public:
  void Update(wxWindow &viewer, const viewer3d::resources::ResourceLoadProgress &progress,
              bool paused);
private:
  wxString lastText;
  bool active = false;
};
