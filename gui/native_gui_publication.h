#pragma once

class wxWindow;

namespace gui {
// Source loading and a natural scene paint gate 3D startup; optional navigation
// representations do not. Other saved views retain the native GUI boundary.
inline bool CanDismissStartupSplash(bool nativeGuiPublished, bool visible3D,
    bool sourcesPresented, bool sourceLoadingPending, bool inputEnabled,
    bool interactionLocked) {
  return nativeGuiPublished && inputEnabled && !interactionLocked &&
         (!visible3D || (sourcesPresented && !sourceLoadingPending));
}

// Initialize native erase backgrounds from the active system theme. Preserve
// explicit colours and exclude GL-containing ancestors, just as publication does.
void InitializeNativeGuiBackgrounds(wxWindow &root);

// Publish native child controls once at the final GUI composition boundary.
// GL canvases and their ancestors are never explicitly refreshed or updated.
void PublishNativeGuiChildren(wxWindow &root);
}
