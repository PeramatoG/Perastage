/*
 * This file is part of Perastage.
 * Copyright (C) 2025 Luisma Peramato
 *
 * Perastage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Perastage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Perastage. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <vector>

#include <wx/gdicmn.h>

#include "canvas2d.h"
#include "symbolcache.h"
#include "viewer2dpanel.h"

namespace gui::layoutcache {

inline constexpr int kLayoutViewCacheSchemaVersion = 3;
inline constexpr const char *kLayoutViewCacheArchiveEntry =
    "resources/layout_view_cache/last_selected_layout_view.json";

bool RenderCommandBufferCacheToRgba(const wxSize &renderSize,
                                    const CommandBuffer &buffer,
                                    const Viewer2DViewState &viewState,
                                    const SymbolDefinitionSnapshot *symbols,
                                    double renderZoom,
                                    std::vector<unsigned char> &pixels,
                                    int &width, int &height);

} // namespace gui::layoutcache
