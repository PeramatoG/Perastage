#pragma once

#include <wx/image.h>
#include <wx/size.h>

#include "canvas2d.h"
#include "symbolcache.h"
#include "viewer2dpanel.h"

wxImage RenderLayoutViewCommandBufferToImage(
    const wxSize &size, const CommandBuffer &buffer,
    const Viewer2DViewState &viewState,
    const SymbolDefinitionSnapshot *symbols);
