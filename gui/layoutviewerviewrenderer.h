#pragma once

#include <wx/gdicmn.h>
#include <wx/image.h>

#include "canvas2d.h"
#include "symbolcache.h"
#include "viewer2dpanel.h"

struct LayoutViewSvgOverlayDebugInfo {
  int symbolInstanceCommands = 0;
  int svgLookupAttempts = 0;
  int svgResolved = 0;
  int fallbackRenderCount = 0;
};

wxImage RenderLayoutViewCommandBufferToImage(
    const wxSize &size, const CommandBuffer &buffer,
    const Viewer2DViewState &viewState,
    const SymbolDefinitionSnapshot *symbols);

wxImage RenderLayoutViewSvgSymbolsOverlayToImage(
    const wxSize &size, const CommandBuffer &buffer,
    const Viewer2DViewState &viewState,
    const SymbolDefinitionSnapshot *symbols,
    LayoutViewSvgOverlayDebugInfo *debugInfo = nullptr);
