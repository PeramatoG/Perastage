/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include "../viewer_common/context_menu_model.h"
#include <functional>
#include <wx/gdicmn.h>
class wxWindow;
namespace gui::viewport {
// UUID must come from the viewport's normal, scope-aware picking route.
void ShowContextMenu(wxWindow &window, const wxPoint &position,
                     const std::string &uuid, viewport_context::Kind activeKind,
                     bool threeDimensional, std::function<void()> exportImage = {});
} // namespace gui::viewport
