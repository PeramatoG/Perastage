#pragma once

#include <array>
#include <string_view>

namespace gui {

inline constexpr std::array<std::string_view, 6> kAlwaysVisibleToolbarPanes = {
    "FileToolbar", "EditToolbar", "LayoutViewsToolbar", "ToolsToolbar",
    "LayoutToolbar", "AlignmentToolbar"};

} // namespace gui
