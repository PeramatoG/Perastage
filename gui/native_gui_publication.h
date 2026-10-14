#pragma once

class wxWindow;

namespace gui {
// Publish native child controls once at the final GUI composition boundary.
// GL canvases and their ancestors are never explicitly refreshed or updated.
void PublishNativeGuiChildren(wxWindow &root);
}
