#pragma once

#include <wx/menu.h>

namespace ui {
enum class FeatureFlag;
}

// Builds the complete main window menu bar structure with all top-level menus.
wxMenuBar *BuildMainWindowMenuBar();
