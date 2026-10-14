#pragma once

class wxWindow;

namespace gui {
// Initialize native erase backgrounds from the active system theme. Preserve
// explicit colours and exclude GL-containing ancestors, just as publication does.
void InitializeNativeGuiBackgrounds(wxWindow &root);

// Publish native child controls once at the final GUI composition boundary.
// GL canvases and their ancestors are never explicitly refreshed or updated.
void PublishNativeGuiChildren(wxWindow &root);
}
