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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <unordered_map>
#include <vector>
#include <wx/weakref.h>
#include <wx/window.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <GL/glew.h>
#include "gl_context_utils.h"
// Include GLEW or other OpenGL loader first if present
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "../viewer_common/gl_canvas_config.h"
#include "gl_state_guard.h"
#include "layout_2d_view_rasterizer.h"
#include "layout_render_profiler.h"
#include "layout_render_status_notifier.h"
#include "layoutviewerpanel.h"
#include "layoutviewerpanel_helpers.h"
#include <wx/debug.h>
#include <wx/log.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif

#include "LayoutManager.h"
#include "configmanager.h"
#include "editable_focus_utils.h"
#include "guiconfigservices.h"
#include "legendsymbolcapture.h"
#include "logger.h"
#include "mainwindow.h"
#include "startup_profile.h"
#include "ui_render_size.h"
#include "viewer2doffscreenrenderer.h"
#include "viewer2dstate.h"

namespace {
using gui::layoutviewport::kMaxRenderBytes;
using gui::layoutviewport::kMaxRenderDimension;
using gui::layoutviewport::kMaxRenderPixels;
using gui::layoutviewport::kMinZoom;
using gui::layoutviewport::kZoomStep;
constexpr int kZoomCacheStepsPerLevel = 2;
constexpr int kHandleSizePx = 10;
constexpr int kHandleHalfPx = kHandleSizePx / 2;
constexpr int kEditMenuId = wxID_HIGHEST + 490;
constexpr int kDeleteMenuId = wxID_HIGHEST + 491;
constexpr int kDeleteLegendMenuId = wxID_HIGHEST + 492;
constexpr int kEditLegendMenuId = wxID_HIGHEST + 505;
constexpr int kEditEventTableMenuId = wxID_HIGHEST + 493;
constexpr int kDeleteEventTableMenuId = wxID_HIGHEST + 494;
constexpr int kEditTextMenuId = wxID_HIGHEST + 495;
constexpr int kDeleteTextMenuId = wxID_HIGHEST + 496;
constexpr int kEditImageMenuId = wxID_HIGHEST + 497;
constexpr int kDeleteImageMenuId = wxID_HIGHEST + 498;
constexpr int kBringToFrontMenuId = wxID_HIGHEST + 499;
constexpr int kSendToBackMenuId = wxID_HIGHEST + 500;
constexpr int kLoadingTimerId = wxID_HIGHEST + 501;
constexpr int kRenderDelayTimerId = wxID_HIGHEST + 502;
constexpr int kToggleTextFrameMenuId = wxID_HIGHEST + 503;
constexpr int kToggleTextTransparentBackgroundMenuId = wxID_HIGHEST + 504;
constexpr int kToggleViewFrameMenuId = wxID_HIGHEST + 506;
constexpr int kLoadingOverlayDelayMs = 150;
constexpr int kZoomRenderDebounceMs = 180;
constexpr int kIncrementalRenderDelayMs = 1;

// Validates that offscreen rendering restored the expected OpenGL state.
void ValidateGlStateAfterRender(const char *stage, int expectedWidth,
                                int expectedHeight) {
  GLint framebuffer = 0;
  GLint viewport[4] = {0, 0, 0, 0};
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  glGetIntegerv(GL_VIEWPORT, viewport);
  const bool validFramebuffer = framebuffer == 0;
  const bool validViewport = viewport[0] == 0 && viewport[1] == 0 &&
                             viewport[2] == expectedWidth &&
                             viewport[3] == expectedHeight;
  if (!validFramebuffer || !validViewport) {
    const wxString stageText = wxString::FromUTF8(stage ? stage : "unknown");
    wxLogTrace("layoutviewer_gl_state",
               "%s left unexpected GL state (fbo=%d viewport=%d,%d,%d,%d "
               "expected=0,0,%d,%d).",
               stageText.c_str(), framebuffer, viewport[0], viewport[1],
               viewport[2], viewport[3], expectedWidth, expectedHeight);
  }
  if (!validFramebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    wxASSERT_MSG(false,
                 "Unexpected non-default framebuffer after layout render.");
  }
  if (!validViewport) {
    // The layout draw path can temporarily adjust the viewport for
    // sub-elements. Restore a known onscreen viewport before presenting.
    glViewport(0, 0, expectedWidth, expectedHeight);
  }
}

// Computes the layout-wide maximum safe zoom across all renderable frames.
double GetLayoutSafeMaxZoom(const layouts::LayoutDefinition &layout) {
  std::vector<gui::layoutviewport::Size> frameSizes;
  auto appendFrames = [&frameSizes](const auto &collection) {
    for (const auto &entry : collection)
      frameSizes.push_back({entry.frame.width, entry.frame.height});
  };
  appendFrames(layout.view2dViews);
  appendFrames(layout.legendViews);
  appendFrames(layout.eventTables);
  appendFrames(layout.textViews);
  appendFrames(layout.imageViews);
  return gui::layoutviewport::GetLayoutSafeMaxZoom(frameSizes);
}

bool AreEqual(const layouts::Layout2DViewFrame &lhs,
              const layouts::Layout2DViewFrame &rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width &&
         lhs.height == rhs.height;
}

bool AreEqual(const layouts::Layout2DViewCameraState &lhs,
              const layouts::Layout2DViewCameraState &rhs) {
  return lhs.offsetPixelsX == rhs.offsetPixelsX &&
         lhs.offsetPixelsY == rhs.offsetPixelsY && lhs.zoom == rhs.zoom &&
         lhs.viewportWidth == rhs.viewportWidth &&
         lhs.viewportHeight == rhs.viewportHeight && lhs.view == rhs.view;
}

bool AreEqual(const layouts::Layout2DViewRenderOptions &lhs,
              const layouts::Layout2DViewRenderOptions &rhs) {
  return lhs.renderMode == rhs.renderMode && lhs.darkMode == rhs.darkMode &&
         lhs.forceBottomViewForTopFixtures ==
             rhs.forceBottomViewForTopFixtures &&
         lhs.showGrid == rhs.showGrid && lhs.gridStyle == rhs.gridStyle &&
         lhs.gridColorR == rhs.gridColorR && lhs.gridColorG == rhs.gridColorG &&
         lhs.gridColorB == rhs.gridColorB &&
         lhs.gridDrawAbove == rhs.gridDrawAbove &&
         lhs.showRuler == rhs.showRuler && lhs.rulerColorR == rhs.rulerColorR &&
         lhs.rulerColorG == rhs.rulerColorG &&
         lhs.rulerColorB == rhs.rulerColorB &&
         lhs.showLabelName == rhs.showLabelName &&
         lhs.showLabelId == rhs.showLabelId &&
         lhs.showLabelDmx == rhs.showLabelDmx &&
         lhs.labelFontSizeName == rhs.labelFontSizeName &&
         lhs.labelFontSizeId == rhs.labelFontSizeId &&
         lhs.labelFontSizeDmx == rhs.labelFontSizeDmx &&
         lhs.labelOffsetDistance == rhs.labelOffsetDistance &&
         lhs.labelOffsetAngle == rhs.labelOffsetAngle;
}

bool AreEqual(const layouts::Layout2DViewLayers &lhs,
              const layouts::Layout2DViewLayers &rhs) {
  return lhs.hiddenLayers == rhs.hiddenLayers &&
         lhs.hiddenFixtureTypes == rhs.hiddenFixtureTypes;
}

bool AreEqual(const layouts::Layout2DViewDefinition &lhs,
              const layouts::Layout2DViewDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame) && AreEqual(lhs.camera, rhs.camera) &&
         AreEqual(lhs.renderOptions, rhs.renderOptions) &&
         lhs.drawFrame == rhs.drawFrame && AreEqual(lhs.layers, rhs.layers);
}

bool AreEqual(const layouts::LayoutLegendDefinition &lhs,
              const layouts::LayoutLegendDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame);
}

bool AreEqual(const layouts::LayoutEventTableDefinition &lhs,
              const layouts::LayoutEventTableDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame) && lhs.fields == rhs.fields;
}

bool AreEqual(const layouts::LayoutTextDefinition &lhs,
              const layouts::LayoutTextDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame) && lhs.text == rhs.text &&
         lhs.richText == rhs.richText &&
         lhs.solidBackground == rhs.solidBackground &&
         lhs.drawFrame == rhs.drawFrame;
}

bool AreEqual(const layouts::LayoutImageDefinition &lhs,
              const layouts::LayoutImageDefinition &rhs) {
  return lhs.id == rhs.id && lhs.zIndex == rhs.zIndex &&
         AreEqual(lhs.frame, rhs.frame) && lhs.imagePath == rhs.imagePath &&
         lhs.aspectRatio == rhs.aspectRatio;
}

template <typename T>
bool AreEqual(const std::vector<T> &lhs, const std::vector<T> &rhs) {
  if (lhs.size() != rhs.size())
    return false;

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (!AreEqual(lhs[i], rhs[i]))
      return false;
  }
  return true;
}

bool IsSameRenderableLayout(const layouts::LayoutDefinition &lhs,
                            const layouts::LayoutDefinition &rhs) {
  return lhs.pageSetup.pageSize == rhs.pageSetup.pageSize &&
         lhs.pageSetup.landscape == rhs.pageSetup.landscape &&
         AreEqual(lhs.view2dViews, rhs.view2dViews) &&
         AreEqual(lhs.legendViews, rhs.legendViews) &&
         AreEqual(lhs.eventTables, rhs.eventTables) &&
         AreEqual(lhs.textViews, rhs.textViews) &&
         AreEqual(lhs.imageViews, rhs.imageViews);
}

unsigned int *gActivePixelUnpackPbo = nullptr;
size_t *gActivePixelUnpackPboBytes = nullptr;

class ScopedActivePixelUnpackPbo {
public:
  ScopedActivePixelUnpackPbo(unsigned int &pbo, size_t &capacity)
      : previousPbo_(gActivePixelUnpackPbo),
        previousCapacity_(gActivePixelUnpackPboBytes) {
    gActivePixelUnpackPbo = &pbo;
    gActivePixelUnpackPboBytes = &capacity;
  }

  ~ScopedActivePixelUnpackPbo() {
    gActivePixelUnpackPbo = previousPbo_;
    gActivePixelUnpackPboBytes = previousCapacity_;
  }

private:
  unsigned int *previousPbo_;
  size_t *previousCapacity_;
};

bool TryAllocatePixelBuffer(std::vector<unsigned char> &pixels, int width,
                            int height, const char *context) {
  if (width <= 0 || height <= 0)
    return false;
  const size_t totalPixels =
      static_cast<size_t>(width) * static_cast<size_t>(height);
  const size_t totalBytes = totalPixels * 4;
  if (totalBytes > kMaxRenderBytes) {
    Logger::Instance().Log(
        std::string("LayoutViewerPanel: ") + context +
        " render buffer exceeds kMaxRenderBytes (" +
        std::to_string(totalBytes) + " > " + std::to_string(kMaxRenderBytes) +
        ") for " + std::to_string(width) + "x" + std::to_string(height) + ".");
    return false;
  }
  if (totalPixels > kMaxRenderPixels) {
    Logger::Instance().Log(std::string("LayoutViewerPanel: ") + context +
                           " render buffer too large (" +
                           std::to_string(width) + "x" +
                           std::to_string(height) + ").");
    return false;
  }
  try {
    pixels.resize(totalBytes);
  } catch (const std::bad_alloc &) {
    Logger::Instance().Log(std::string("LayoutViewerPanel: ") + context +
                           " render buffer allocation failed.");
    return false;
  }
  return true;
}

bool IsPixelUnpackPboSupported() {
  static int cachedSupport = -1;
  if (cachedSupport >= 0)
    return cachedSupport == 1;

  const GLubyte *versionData = glGetString(GL_VERSION);
  if (!versionData) {
    cachedSupport = 0;
    return false;
  }

  int major = 0;
  int minor = 0;
  if (std::sscanf(reinterpret_cast<const char *>(versionData), "%d.%d", &major,
                  &minor) == 2) {
    if (major > 2 || (major == 2 && minor >= 1)) {
      cachedSupport = 1;
      return true;
    }
  }

  const GLubyte *extensionsData = glGetString(GL_EXTENSIONS);
  if (!extensionsData) {
    cachedSupport = 0;
    return false;
  }

  const std::string extensions(reinterpret_cast<const char *>(extensionsData));
  const bool hasPboExtension =
      extensions.find("GL_ARB_pixel_buffer_object") != std::string::npos;
  cachedSupport = hasPboExtension ? 1 : 0;
  return hasPboExtension;
}

bool EnsurePboCapacity(unsigned int &pbo, size_t &capacity,
                       size_t bytesNeeded) {
  if (bytesNeeded == 0)
    return false;
  if (pbo == 0)
    glGenBuffers(1, &pbo);
  if (pbo == 0)
    return false;

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
  if (capacity < bytesNeeded) {
    glBufferData(GL_PIXEL_UNPACK_BUFFER, static_cast<GLsizeiptr>(bytesNeeded),
                 nullptr, GL_STREAM_DRAW);
    capacity = bytesNeeded;
  }
  return true;
}

bool UploadRgbaToTexture(unsigned int texture, int width, int height,
                         const unsigned char *data,
                         const wxSize &currentTextureSize, bool allowPbo) {
  if (texture == 0 || width <= 0 || height <= 0 || data == nullptr)
    return false;

  const bool needsAllocation = currentTextureSize.GetWidth() != width ||
                               currentTextureSize.GetHeight() != height;

  if (needsAllocation) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
  }

  const size_t bytesNeeded =
      static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
  bool uploaded = false;

  if (allowPbo && IsPixelUnpackPboSupported() && gActivePixelUnpackPbo &&
      gActivePixelUnpackPboBytes &&
      EnsurePboCapacity(*gActivePixelUnpackPbo, *gActivePixelUnpackPboBytes,
                        bytesNeeded)) {
    void *mappedBuffer = nullptr;
#if defined(GL_MAP_INVALIDATE_BUFFER_BIT) && defined(GL_MAP_WRITE_BIT)
    mappedBuffer = glMapBufferRange(
        GL_PIXEL_UNPACK_BUFFER, 0, static_cast<GLsizeiptr>(bytesNeeded),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
#endif
    if (!mappedBuffer) {
      mappedBuffer = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    }

    if (mappedBuffer) {
      std::memcpy(mappedBuffer, data, bytesNeeded);
      if (glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER) == GL_TRUE) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
                        GL_UNSIGNED_BYTE, nullptr);
        uploaded = true;
      }
    }
  }

  if (!uploaded) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA,
                    GL_UNSIGNED_BYTE, data);
    uploaded = true;
  }

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  return uploaded;
}

} // namespace

wxDEFINE_EVENT(EVT_LAYOUT_VIEW_EDIT, wxCommandEvent);

wxBEGIN_EVENT_TABLE(LayoutViewerPanel, wxGLCanvas) EVT_PAINT(
    LayoutViewerPanel::
        OnPaint) EVT_SIZE(LayoutViewerPanel::
                              OnSize) EVT_LEFT_DOWN(LayoutViewerPanel::
                                                        OnLeftDown)
    EVT_LEFT_UP(LayoutViewerPanel::OnLeftUp) EVT_LEFT_DCLICK(
        LayoutViewerPanel::
            OnLeftDClick) EVT_MOTION(LayoutViewerPanel::OnMouseMove)
        EVT_MOUSEWHEEL(LayoutViewerPanel::OnMouseWheel) EVT_MOUSE_CAPTURE_LOST(
            LayoutViewerPanel::
                OnCaptureLost) EVT_RIGHT_UP(LayoutViewerPanel::OnRightUp)
            EVT_KEY_DOWN(LayoutViewerPanel::OnKeyDown) EVT_SHOW(
                LayoutViewerPanel::
                    OnShow) EVT_MENU(kEditMenuId,
                                     LayoutViewerPanel::
                                         OnEditView) EVT_MENU(kDeleteMenuId,
                                                              LayoutViewerPanel::
                                                                  OnDeleteView)
                EVT_MENU(kToggleViewFrameMenuId,
                         LayoutViewerPanel::
                             OnToggleViewFrame) EVT_MENU(kEditLegendMenuId,
                                                         LayoutViewerPanel::
                                                             OnEditLegend)
                    EVT_MENU(kDeleteLegendMenuId,
                             LayoutViewerPanel::
                                 OnDeleteLegend) EVT_MENU(kEditEventTableMenuId,
                                                          LayoutViewerPanel::
                                                              OnEditEventTable)
                        EVT_MENU(
                            kDeleteEventTableMenuId,
                            LayoutViewerPanel::
                                OnDeleteEventTable) EVT_MENU(kEditTextMenuId,
                                                             LayoutViewerPanel::
                                                                 OnEditText)
                            EVT_MENU(
                                kDeleteTextMenuId,
                                LayoutViewerPanel::
                                    OnDeleteText) EVT_MENU(kToggleTextFrameMenuId,
                                                           LayoutViewerPanel::
                                                               OnToggleTextFrame)
                                EVT_MENU(kToggleTextTransparentBackgroundMenuId,
                                         LayoutViewerPanel::
                                             OnToggleTextTransparentBackground)
                                    EVT_MENU(kEditImageMenuId,
                                             LayoutViewerPanel::OnEditImage)
                                        EVT_MENU(
                                            kDeleteImageMenuId,
                                            LayoutViewerPanel::OnDeleteImage)
                                            EVT_MENU(kBringToFrontMenuId,
                                                     LayoutViewerPanel::
                                                         OnBringToFront)
                                                EVT_MENU(kSendToBackMenuId,
                                                         LayoutViewerPanel::
                                                             OnSendToBack)
                                                    wxEND_EVENT_TABLE()

                                                        wxDEFINE_EVENT(
                                                            EVT_LAYOUT_RENDER_READY,
                                                            wxCommandEvent);
wxDEFINE_EVENT(EVT_LAYOUT_VIEW_SELECTED, wxCommandEvent);

LayoutViewerPanel::LayoutViewerPanel(wxWindow *parent)
    : wxGLCanvas(parent, wxID_ANY, gl_lifecycle::GetStandardCanvasAttributes(),
                 wxDefaultPosition, wxDefaultSize,
                 wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS) {
  SetBackgroundStyle(wxBG_STYLE_CUSTOM);
  glContext_ = new wxGLContext(this);
  currentLayout.pageSetup.pageSize = print::PageSize::A4;
  currentLayout.pageSetup.landscape = true;
  loadingTimer_.SetOwner(this, kLoadingTimerId);
  renderDelayTimer_.SetOwner(this, kRenderDelayTimerId);
  Bind(wxEVT_TIMER, &LayoutViewerPanel::OnLoadingTimer, this, kLoadingTimerId);
  Bind(wxEVT_TIMER, &LayoutViewerPanel::OnRenderDelayTimer, this,
       kRenderDelayTimerId);
}

LayoutViewerPanel::~LayoutViewerPanel() {
  if (mouseCapture_.IsOwned())
    mouseCapture_.Release("destructor");
  Unbind(wxEVT_TIMER, &LayoutViewerPanel::OnLoadingTimer, this,
         kLoadingTimerId);
  Unbind(wxEVT_TIMER, &LayoutViewerPanel::OnRenderDelayTimer, this,
         kRenderDelayTimerId);
  ClearCachedTexture();
  ClearLoadingTextTexture();
  loadingTimer_.Stop();
  renderDelayTimer_.Stop();
  delete glContext_;
}

// Requests a deferred automatic fit once the pane has a stable visible size.
void LayoutViewerPanel::RequestFitToViewport() {
  viewportState_.RequestAutomaticFit();
  SchedulePendingFitToViewport();
}

// Clears project-scoped Layout preview render caches before applying a new
// project.
void LayoutViewerPanel::ResetPreviewCachesForProjectLoad() {
  ClearCachedTexture();
  pendingPersistentViewCacheJson_.clear();
  pendingPersistentViewCacheRasters_.clear();

  captureInProgress = false;
  renderDirty = true;
  renderPending = false;
  isLoading = false;
  loadingRequested = false;
  loadingTimer_.Stop();
  renderDelayTimer_.Stop();
  legendDataDirty_ = true;
  legendItems_.clear();
  legendDataHash = 0;
  hasSceneContentHash = false;
  lastSceneContentHash = 0;
  pendingFrameCommit_ = false;
  interactionSession_.ResetForLayoutReplacement();
  layoutVersion++;
  viewRenderVersion++;
  InvalidateSelectionIndexCache();
}

// Applies a layout snapshot, preserving selection and scheduling texture
// rebuilds when renderable data changes.
void LayoutViewerPanel::SetLayoutDefinition(
    const layouts::LayoutDefinition &layout) {
  if (IsSameRenderableLayout(currentLayout, layout)) {
    currentLayout = layout;
    InvalidateSelectionIndexCache();
    legendDataDirty_ = true;
    RefreshLegendData();
    HydratePendingPersistentViewCache();
    InvalidateRenderIfFrameChanged(false);
    if (NeedsRenderRebuild())
      RequestRenderRebuild();
    Refresh();
    NotifyRenderReady();
    return;
  }

  const layouts::LayoutDefinition previousLayout = currentLayout;
  const bool sameLayoutName =
      !previousLayout.name.empty() && previousLayout.name == layout.name;
  currentLayout = layout;
  InvalidateSelectionIndexCache();
  const auto selected = gui::layoutselection::RetainOrChooseDefault(
      selectionState_.Current(), BuildElementIdsByKind());
  if (!selectionState_.Matches(selected)) {
    if (selected.kind == LayoutElementKind::None) {
      selectionState_.Clear();
    } else {
      selectionState_.Select(selected.kind, selected.id);
      if (selected.kind == LayoutElementKind::View2D)
        EmitViewSelectionChanged(selected.id);
    }
  }
  layoutVersion++;
  if (!AreEqual(previousLayout.view2dViews, currentLayout.view2dViews)) {
    captureInProgress = false;
  }
  pendingFrameCommit_ = false;

  if (sameLayoutName) {
    captureInProgress = false;
    renderDirty = true;
    loadingRequested = false;
    legendDataDirty_ = true;
    RefreshLegendData();
    HydratePendingPersistentViewCache();
    InvalidateRenderIfFrameChanged(true);
    if (NeedsRenderRebuild())
      RequestRenderRebuild();
    Refresh();
    return;
  }

  captureInProgress = false;
  ClearCachedTexture();
  legendDataDirty_ = true;
  HydratePendingPersistentViewCache();
  const bool emptyLayout = IsLayoutEmpty();
  if (emptyLayout) {
    selectionState_.Clear();
    renderDirty = false;
    loadingRequested = false;
    isLoading = false;
    legendItems_.clear();
    legendDataHash = 0;
    legendDataDirty_ = false;
    Refresh();
    NotifyRenderReady();
    return;
  }
  renderDirty = true;
  loadingRequested = false;
  RefreshLegendData();
  InvalidateRenderIfFrameChanged(true);
  RequestRenderRebuild();
  Refresh();
}

// Posts a deferred render-ready event after layout rendering state has settled.
void LayoutViewerPanel::NotifyRenderReady() {
  wxWeakRef<LayoutViewerPanel> weakThis(this);
  CallAfter([weakThis]() {
    if (!weakThis)
      return;
    LayoutViewerPanel *panel = weakThis.get();
    if (!panel)
      return;
    wxCommandEvent event(EVT_LAYOUT_RENDER_READY);
    event.SetEventObject(panel);
    wxPostEvent(panel, event);
  });
}

// Marks the cached selection/render indices as dirty so they rebuild on demand.
void LayoutViewerPanel::InvalidateSelectionIndexCache() {
  selectionIndexCache_.dirty = true;
}

// Rebuilds cached ID lookups and z-ordered render elements when invalidated.
void LayoutViewerPanel::EnsureSelectionIndexCache() {
  // Detect stale cached pointers when layout vectors changed without explicit
  // invalidation.
  if (!selectionIndexCache_.dirty) {
    const auto pointerInRange = [](const auto &container, const auto *ptr) {
      if (ptr == nullptr)
        return false;
      if (container.empty())
        return false;
      const auto *begin = container.data();
      const auto *end = begin + container.size();
      return ptr >= begin && ptr < end;
    };
    const auto mapPointersValid = [&pointerInRange](const auto &map,
                                                    const auto &container) {
      for (const auto &entry : map) {
        if (!pointerInRange(container, entry.second))
          return false;
      }
      return true;
    };
    const bool cacheShapeMatches = selectionIndexCache_.viewById.size() ==
                                       currentLayout.view2dViews.size() &&
                                   selectionIndexCache_.legendById.size() ==
                                       currentLayout.legendViews.size() &&
                                   selectionIndexCache_.eventTableById.size() ==
                                       currentLayout.eventTables.size() &&
                                   selectionIndexCache_.textById.size() ==
                                       currentLayout.textViews.size() &&
                                   selectionIndexCache_.imageById.size() ==
                                       currentLayout.imageViews.size();
    const bool cachePointersValid =
        mapPointersValid(selectionIndexCache_.viewById,
                         currentLayout.view2dViews) &&
        mapPointersValid(selectionIndexCache_.legendById,
                         currentLayout.legendViews) &&
        mapPointersValid(selectionIndexCache_.eventTableById,
                         currentLayout.eventTables) &&
        mapPointersValid(selectionIndexCache_.textById,
                         currentLayout.textViews) &&
        mapPointersValid(selectionIndexCache_.imageById,
                         currentLayout.imageViews);
    if (cacheShapeMatches && cachePointersValid) {
      return;
    }
  }

  selectionIndexCache_.zOrderedElements = BuildZOrderedElements();

  auto &viewById = selectionIndexCache_.viewById;
  auto &legendById = selectionIndexCache_.legendById;
  auto &eventTableById = selectionIndexCache_.eventTableById;
  auto &textById = selectionIndexCache_.textById;
  auto &imageById = selectionIndexCache_.imageById;

  viewById.clear();
  legendById.clear();
  eventTableById.clear();
  textById.clear();
  imageById.clear();

  viewById.reserve(currentLayout.view2dViews.size());
  legendById.reserve(currentLayout.legendViews.size());
  eventTableById.reserve(currentLayout.eventTables.size());
  textById.reserve(currentLayout.textViews.size());
  imageById.reserve(currentLayout.imageViews.size());

  for (const auto &view : currentLayout.view2dViews)
    viewById.emplace(view.id, &view);
  for (const auto &legend : currentLayout.legendViews)
    legendById.emplace(legend.id, &legend);
  for (const auto &table : currentLayout.eventTables)
    eventTableById.emplace(table.id, &table);
  for (const auto &text : currentLayout.textViews)
    textById.emplace(text.id, &text);
  for (const auto &image : currentLayout.imageViews)
    imageById.emplace(image.id, &image);

  selectionIndexCache_.dirty = false;
}

// Builds a stable z-ordered element list used for selection and rendering
// passes.
std::vector<LayoutViewerPanel::ZOrderedElement>
LayoutViewerPanel::BuildZOrderedElements() const {
  gui::layoutselection::ElementsByKind elements;
  const auto append = [](auto &target, const auto &source) {
    target.reserve(source.size());
    for (const auto &entry : source)
      target.push_back({entry.id, entry.zIndex});
  };
  append(elements[0], currentLayout.view2dViews);
  append(elements[1], currentLayout.legendViews);
  append(elements[2], currentLayout.eventTables);
  append(elements[3], currentLayout.textViews);
  append(elements[4], currentLayout.imageViews);
  return gui::layoutselection::BuildStableZOrder(elements);
}

// Builds category-prioritized element identifiers for selection validation.
gui::layoutselection::ElementRefsByKind
LayoutViewerPanel::BuildElementIdsByKind() const {
  gui::layoutselection::ElementRefsByKind ids;
  const auto append = [](auto &target, const auto &source) {
    target.reserve(source.size());
    for (const auto &entry : source)
      target.push_back(entry.id);
  };
  append(ids[0], currentLayout.view2dViews);
  append(ids[1], currentLayout.legendViews);
  append(ids[2], currentLayout.eventTables);
  append(ids[3], currentLayout.textViews);
  append(ids[4], currentLayout.imageViews);
  return ids;
}

bool LayoutViewerPanel::IsLayoutEmpty() const {
  return currentLayout.view2dViews.empty() &&
         currentLayout.legendViews.empty() &&
         currentLayout.eventTables.empty() && currentLayout.textViews.empty() &&
         currentLayout.imageViews.empty();
}

// Paints the layout page and all elements using cached selection/render
// indices.
void LayoutViewerPanel::OnPaint(wxPaintEvent &) {
  static unsigned long long s_renderFrameId = 0;
  wxPaintDC dc(this);
  try {
    if (auto *mw = MainWindow::Instance();
        mw && mw->IsMvrImportPipelineActive()) {
      return;
    }
    if (!IsShownOnScreen()) {
      return;
    }
    if (!InitGL()) {
      return;
    }
    InitGL();
    if (!isReadyToRender_) {
      return;
    }
    if (!gl_lifecycle::TrySetCurrent(*this, glContext_, "LayoutViewerPanel",
                                     "OnPaint")) {
      return;
    }
    if (legendDataDirty_)
      RefreshLegendData();
    if (!renderPending && NeedsRenderRebuild()) {
      RequestRenderRebuild();
    }

    const wxSize logicalSize = layoutviewerpanel::GetLogicalClientSize(this);
    if (logicalSize.GetWidth() <= 0 || logicalSize.GetHeight() <= 0) {
      return;
    }
    const RenderSize resolvedSize = ResolveRenderSize(this);
    if (!resolvedSize.IsValid()) {
      return;
    }
    const wxSize framebufferSize(resolvedSize.width, resolvedSize.height);
    glstate::ApplyKnownBaseOnscreenState(framebufferSize.GetWidth(),
                                         framebufferSize.GetHeight());
    const RenderSize viewportSize{
        framebufferSize.GetWidth(), framebufferSize.GetHeight(),
        "glstate::ApplyKnownBaseOnscreenState(framebuffer-px)"};
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, logicalSize.GetWidth(), logicalSize.GetHeight(), 0.0, -1.0,
            1.0);
    const wxPoint projectionFramebufferPoint =
        layoutviewerpanel::ToFramebufferPoint(
            this, wxPoint(logicalSize.GetWidth(), logicalSize.GetHeight()));
    const RenderSize projectionSize{projectionFramebufferPoint.x,
                                    projectionFramebufferPoint.y,
                                    "LayoutViewerPanel::OnPaint::ortho(logical-"
                                    "dip mapped to framebuffer-px)"};
    ++s_renderFrameId;
    ValidateRenderSizeContract("LayoutViewerPanel", s_renderFrameId,
                               resolvedSize, viewportSize, projectionSize);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.35f, 0.35f, 0.35f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (auto *mw = MainWindow::Instance();
        mw && mw->IsStartupProjectLoadPending() && currentLayout.name.empty()) {
      glFlush();
      SwapBuffers();
      return;
    }

    const double pageWidth = currentLayout.pageSetup.PageWidthPt();
    const double pageHeight = currentLayout.pageSetup.PageHeightPt();

    const wxRect pageRect = GetPageRect();
    const double scaledWidth = pageWidth * viewportState_.Zoom();
    const double scaledHeight = pageHeight * viewportState_.Zoom();
    const wxPoint topLeft = pageRect.GetPosition();

    glColor4ub(255, 255, 255, 255);
    glBegin(GL_QUADS);
    glVertex2f(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
    glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
               static_cast<float>(topLeft.y));
    glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
               static_cast<float>(topLeft.y + scaledHeight));
    glVertex2f(static_cast<float>(topLeft.x),
               static_cast<float>(topLeft.y + scaledHeight));
    glEnd();

    glColor4ub(200, 200, 200, 255);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y));
    glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
               static_cast<float>(topLeft.y));
    glVertex2f(static_cast<float>(topLeft.x + scaledWidth),
               static_cast<float>(topLeft.y + scaledHeight));
    glVertex2f(static_cast<float>(topLeft.x),
               static_cast<float>(topLeft.y + scaledHeight));
    glEnd();

    const layouts::Layout2DViewDefinition *activeView =
        static_cast<const LayoutViewerPanel *>(this)->GetEditableView();
    const bool showDeferredResizeOverlay =
        interactionSession_.DeferredResize().has_value() &&
        interactionSession_.DragMode() != FrameDragMode::None &&
        interactionSession_.DragMode() != FrameDragMode::Move;
    const int selectedViewId =
        selectionState_.Current().kind == LayoutElementKind::View2D &&
                activeView
            ? activeView->id
            : -1;
    const int selectedLegendId =
        selectionState_.Current().kind == LayoutElementKind::Legend
            ? selectionState_.Current().id
            : -1;
    const int selectedEventTableId =
        selectionState_.Current().kind == LayoutElementKind::EventTable
            ? selectionState_.Current().id
            : -1;
    const int selectedTextId =
        selectionState_.Current().kind == LayoutElementKind::Text
            ? selectionState_.Current().id
            : -1;
    const int selectedImageId =
        selectionState_.Current().kind == LayoutElementKind::Image
            ? selectionState_.Current().id
            : -1;

    const int activeViewId = showDeferredResizeOverlay ? -1 : selectedViewId;
    const int activeLegendId =
        showDeferredResizeOverlay ? -1 : selectedLegendId;
    const int activeEventTableId =
        showDeferredResizeOverlay ? -1 : selectedEventTableId;
    const int activeTextId = showDeferredResizeOverlay ? -1 : selectedTextId;
    const int activeImageId = showDeferredResizeOverlay ? -1 : selectedImageId;

    Viewer2DPanel *capturePanel = nullptr;
    Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;

    EnsureSelectionIndexCache();
    const auto &viewById = selectionIndexCache_.viewById;
    const auto &legendById = selectionIndexCache_.legendById;
    const auto &eventTableById = selectionIndexCache_.eventTableById;
    const auto &textById = selectionIndexCache_.textById;
    const auto &imageById = selectionIndexCache_.imageById;
    const auto &elements = selectionIndexCache_.zOrderedElements;
    for (const auto &element : elements) {
      if (element.element.kind == LayoutElementKind::View2D) {
        auto it = viewById.find(element.element.id);
        if (it != viewById.end())
          DrawViewElement(*it->second, capturePanel, offscreenRenderer,
                          activeViewId);
      } else if (element.element.kind == LayoutElementKind::Legend) {
        auto it = legendById.find(element.element.id);
        if (it != legendById.end())
          DrawLegendElement(*it->second, activeLegendId);
      } else if (element.element.kind == LayoutElementKind::EventTable) {
        auto it = eventTableById.find(element.element.id);
        if (it != eventTableById.end())
          DrawEventTableElement(*it->second);
      } else if (element.element.kind == LayoutElementKind::Text) {
        auto it = textById.find(element.element.id);
        if (it != textById.end())
          DrawTextElement(*it->second, activeTextId);
      } else if (element.element.kind == LayoutElementKind::Image) {
        auto it = imageById.find(element.element.id);
        if (it != imageById.end())
          DrawImageElement(*it->second, activeImageId);
      }
    }

    if (showDeferredResizeOverlay)
      DrawDeferredResizeOverlay();

    const bool texturesReady = AreTexturesReady();
    auto hasTexture = [](const auto &cacheMap, int id) {
      auto it = cacheMap.find(id);
      return it != cacheMap.end() && it->second.texture != 0;
    };
    bool activeElementHasTexture = false;
    if (selectionState_.Current().kind == LayoutElementKind::View2D) {
      if (!activeView) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture = hasTexture(viewCaches_, selectedViewId);
      }
    } else if (selectionState_.Current().kind == LayoutElementKind::Legend) {
      auto it = legendById.find(selectedLegendId);
      if (it == legendById.end()) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture = hasTexture(legendCaches_, selectedLegendId);
      }
    } else if (selectionState_.Current().kind ==
               LayoutElementKind::EventTable) {
      auto it = eventTableById.find(selectedEventTableId);
      if (it == eventTableById.end()) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture =
            hasTexture(eventTableCaches_, selectedEventTableId);
      }
    } else if (selectionState_.Current().kind == LayoutElementKind::Text) {
      auto it = textById.find(selectedTextId);
      if (it == textById.end()) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture = hasTexture(textCaches_, selectedTextId);
      }
    } else if (selectionState_.Current().kind == LayoutElementKind::Image) {
      auto it = imageById.find(selectedImageId);
      if (it == imageById.end()) {
        activeElementHasTexture = false;
      } else {
        activeElementHasTexture = hasTexture(imageCaches_, selectedImageId);
      }
    }
    const auto hasAnyTexture = [this]() {
      auto hasAny = [](const auto &map) {
        for (const auto &entry : map) {
          if (entry.second.texture != 0)
            return true;
        }
        return false;
      };
      return hasAny(viewCaches_) || hasAny(legendCaches_) ||
             hasAny(eventTableCaches_) || hasAny(textCaches_) ||
             hasAny(imageCaches_);
    };
    const bool showLoadingOverlay =
        !IsLayoutEmpty() && isLoading && !hasAnyTexture() &&
        (!texturesReady || !activeElementHasTexture);
    if (showLoadingOverlay && isReadyToRender_) {
      DrawLoadingOverlay(logicalSize);
    }

    glFlush();
    ValidateGlStateAfterRender("LayoutViewerPanel::OnPaint",
                               framebufferSize.GetWidth(),
                               framebufferSize.GetHeight());
    SwapBuffers();
  } catch (const std::exception &ex) {
    Logger::Instance().Log(
        std::string("LayoutViewerPanel::OnPaint exception: ") + ex.what());
  } catch (...) {
    Logger::Instance().Log("LayoutViewerPanel::OnPaint unknown exception.");
  }
}

void LayoutViewerPanel::DrawLoadingOverlay(const wxSize &size) {
  if (!glContext_ || !isReadyToRender_)
    return;
  if (size.GetWidth() <= 0 || size.GetHeight() <= 0)
    return;
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glColor4ub(0, 0, 0, 150);
  glBegin(GL_QUADS);
  glVertex2f(0.0f, 0.0f);
  glVertex2f(static_cast<float>(size.GetWidth()), 0.0f);
  glVertex2f(static_cast<float>(size.GetWidth()),
             static_cast<float>(size.GetHeight()));
  glVertex2f(0.0f, static_cast<float>(size.GetHeight()));
  glEnd();

  EnsureLoadingTextTexture();
  if (loadingTextTexture_ == 0)
    return;

  const int textWidth = loadingTextTextureSize_.GetWidth();
  const int textHeight = loadingTextTextureSize_.GetHeight();
  if (textWidth <= 0 || textHeight <= 0)
    return;

  const float x = (size.GetWidth() - textWidth) * 0.5f;
  const float y = (size.GetHeight() - textHeight) * 0.5f;

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, loadingTextTexture_);
  glColor4ub(255, 255, 255, 255);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(x, y);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(x + textWidth, y);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(x + textWidth, y + textHeight);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(x, y + textHeight);
  glEnd();
  glDisable(GL_TEXTURE_2D);
}

// Draws the session-owned deferred resize frame over the current layout.
void LayoutViewerPanel::DrawDeferredResizeOverlay() {
  if (!interactionSession_.DeferredResize().has_value())
    return;
  if (interactionSession_.DragMode() == FrameDragMode::None ||
      interactionSession_.DragMode() == FrameDragMode::Move)
    return;

  wxRect frameRect;
  if (!GetFrameRect(*interactionSession_.DeferredResize(), frameRect))
    return;

  glColor4ub(160, 160, 160, 110);
  glBegin(GL_QUADS);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetBottom()));
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();

  glColor4ub(0, 128, 255, 255);
  glLineWidth(2.0f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetTop()));
  glVertex2f(static_cast<float>(frameRect.GetRight()),
             static_cast<float>(frameRect.GetBottom()));
  glVertex2f(static_cast<float>(frameRect.GetLeft()),
             static_cast<float>(frameRect.GetBottom()));
  glEnd();

  DrawSelectionHandles(frameRect);
}

void LayoutViewerPanel::EnsureLoadingTextTexture() {
  if (loadingTextTexture_ != 0) {
    if (!InitGL())
      return;
    if (!glIsTexture(loadingTextTexture_)) {
      loadingTextTexture_ = 0;
      loadingTextTextureSize_ = wxSize(0, 0);
    } else {
      return;
    }
  }

  const wxString label = layoutviewerpanel::BuildLoadingOverlayLabel();
  wxFont font = wxFontInfo(14).Bold();
  int textWidth = 0;
  int textHeight = 0;
  {
    wxMemoryDC measureDc;
    measureDc.SetFont(font);
    measureDc.GetTextExtent(label, &textWidth, &textHeight);
  }
  if (textWidth <= 0 || textHeight <= 0)
    return;

  const int padding = 12;
  const int bmpWidth = textWidth + padding * 2;
  const int bmpHeight = textHeight + padding * 2;
  wxBitmap bitmap(bmpWidth, bmpHeight, 32);
  {
    wxMemoryDC dc(bitmap);
    dc.SetBackground(wxBrush(wxColour(0, 0, 0)));
    dc.Clear();
    dc.SetFont(font);
    dc.SetTextForeground(wxColour(255, 255, 255));
    dc.DrawText(label, padding, padding);
    dc.SelectObject(wxNullBitmap);
  }

  wxImage image = bitmap.ConvertToImage();
  if (!image.IsOk())
    return;

  const unsigned char *rgb = image.GetData();
  if (!rgb)
    return;

  std::vector<unsigned char> pixels;
  pixels.resize(static_cast<size_t>(bmpWidth) * bmpHeight * 4);
  for (int i = 0; i < bmpWidth * bmpHeight; ++i) {
    const unsigned char intensity = rgb[i * 3];
    pixels[static_cast<size_t>(i) * 4] = 255;
    pixels[static_cast<size_t>(i) * 4 + 1] = 255;
    pixels[static_cast<size_t>(i) * 4 + 2] = 255;
    pixels[static_cast<size_t>(i) * 4 + 3] = intensity;
  }

  if (!InitGL())
    return;
  glGenTextures(1, &loadingTextTexture_);
  glBindTexture(GL_TEXTURE_2D, loadingTextTexture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmpWidth, bmpHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels.data());
  loadingTextTextureSize_ = wxSize(bmpWidth, bmpHeight);
}

// Releases the loading-text texture only when the panel can safely bind GL.
void LayoutViewerPanel::ClearLoadingTextTexture() {
  if (loadingTextTexture_ == 0 || !glContext_)
    return;
  if (!IsShownOnScreen()) {
    loadingTextTexture_ = 0;
    loadingTextTextureSize_ = wxSize(0, 0);
    return;
  }
  if (!gl_lifecycle::TrySetCurrent(*this, glContext_, "LayoutViewerPanel",
                                   "ClearLoadingTextTexture"))
    return;
  glDeleteTextures(1, &loadingTextTexture_);
  loadingTextTexture_ = 0;
  loadingTextTextureSize_ = wxSize(0, 0);
}

void LayoutViewerPanel::DrawSelectionHandles(const wxRect &frameRect) const {
  wxRect handleRight(frameRect.GetRight() - kHandleHalfPx,
                     frameRect.GetTop() + frameRect.GetHeight() / 2 -
                         kHandleHalfPx,
                     kHandleSizePx, kHandleSizePx);
  wxRect handleBottom(
      frameRect.GetLeft() + frameRect.GetWidth() / 2 - kHandleHalfPx,
      frameRect.GetBottom() - kHandleHalfPx, kHandleSizePx, kHandleSizePx);
  wxRect handleCorner(frameRect.GetRight() - kHandleHalfPx,
                      frameRect.GetBottom() - kHandleHalfPx, kHandleSizePx,
                      kHandleSizePx);

  glColor4ub(60, 160, 240, 255);
  auto drawHandle = [](const wxRect &rect) {
    glBegin(GL_QUADS);
    glVertex2f(static_cast<float>(rect.GetLeft()),
               static_cast<float>(rect.GetTop()));
    glVertex2f(static_cast<float>(rect.GetRight()),
               static_cast<float>(rect.GetTop()));
    glVertex2f(static_cast<float>(rect.GetRight()),
               static_cast<float>(rect.GetBottom()));
    glVertex2f(static_cast<float>(rect.GetLeft()),
               static_cast<float>(rect.GetBottom()));
    glEnd();
  };
  drawHandle(handleRight);
  drawHandle(handleBottom);
  drawHandle(handleCorner);
}

// Updates cached render state and repaint scheduling after viewport size
// changes.
void LayoutViewerPanel::OnSize(wxSizeEvent &) {
  if (TryCompletePendingFitToViewport())
    return;

  InvalidateRenderIfFrameChanged(false);
  RequestRenderRebuild();
  Refresh();
}

// Begins frame editing or viewport panning from a left-button press.
void LayoutViewerPanel::OnLeftDown(wxMouseEvent &event) {
  SetFocus();
  pendingFrameCommit_ = false;
  const wxPoint pos = layoutviewerpanel::GetLogicalMousePosition(event);
  const gui::layoutinteraction::Point pointer{pos.x, pos.y};
  SelectElementAtPosition(pos);
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(selectedFrame, frameRect)) {
    const FrameDragMode mode = gui::layoutinteraction::HitTestFrame(
        pointer, {frameRect.GetX(), frameRect.GetY(), frameRect.GetWidth(),
                  frameRect.GetHeight()});
    if (mode != FrameDragMode::None) {
      interactionSession_.BeginFrameDrag(mode, pointer, selectedFrame);
      if (!mouseCapture_.TryAcquire("frame-drag")) {
        interactionSession_.CancelAfterCaptureLoss();
        return;
      }
      if (!currentLayout.name.empty()) {
        auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
        cfg.PushUndoState("edit layout element frame");
        layouts::LayoutManager::Get().BeginBatchUpdate();
      }
      return;
    }
  }
  interactionSession_.BeginPan(pointer);
  if (!mouseCapture_.TryAcquire("viewport-pan"))
    interactionSession_.CancelAfterCaptureLoss();
}
// Finalizes the active frame edit or viewport pan gesture.
void LayoutViewerPanel::OnLeftUp(wxMouseEvent &) {
  const FrameDragMode dragMode = interactionSession_.DragMode();
  if (dragMode != FrameDragMode::None) {
    if (dragMode != FrameDragMode::Move &&
        interactionSession_.DeferredResize().has_value()) {
      const layouts::Layout2DViewFrame finalFrame =
          gui::layoutinteraction::FinalizeResizedFrame(
              *interactionSession_.DeferredResize());
      ApplyFrameUpdateToSelection(finalFrame, false);
    }
    if (dragMode == FrameDragMode::Move) {
      layouts::Layout2DViewFrame finalFrame;
      if (GetSelectedFrame(finalFrame)) {
        finalFrame = gui::layoutinteraction::FinalizeMovedFrame(finalFrame);
        ApplyFrameUpdateToSelection(finalFrame, true);
      }
      CommitPendingFrameUpdate();
    }
    interactionSession_.CompleteFrameDrag();
    layouts::LayoutManager::Get().EndBatchUpdate();
    mouseCapture_.Release("frame-drag");
    return;
  }
  if (interactionSession_.IsPanning()) {
    interactionSession_.EndPan();
    mouseCapture_.Release("viewport-pan");
  }
}

void LayoutViewerPanel::OnLeftDClick(wxMouseEvent &event) {
  const wxPoint pos = layoutviewerpanel::GetLogicalMousePosition(event);
  SelectElementAtPosition(pos);
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(selectedFrame, frameRect) && frameRect.Contains(pos)) {
    if (selectionState_.Current().kind == LayoutElementKind::View2D) {
      EmitEditViewRequest();
      return;
    }
    if (selectionState_.Current().kind == LayoutElementKind::EventTable) {
      wxCommandEvent editEvent;
      OnEditEventTable(editEvent);
      return;
    }
    if (selectionState_.Current().kind == LayoutElementKind::Legend) {
      wxCommandEvent editEvent;
      OnEditLegend(editEvent);
      return;
    }
    if (selectionState_.Current().kind == LayoutElementKind::Text) {
      wxCommandEvent editEvent;
      OnEditText(editEvent);
      return;
    }
    if (selectionState_.Current().kind == LayoutElementKind::Image) {
      wxCommandEvent editEvent;
      OnEditImage(editEvent);
      return;
    }
  }
  event.Skip();
}

bool LayoutViewerPanel::DeleteSelectedElement() {
  wxCommandEvent deleteEvent;
  if (selectionState_.Current().kind == LayoutElementKind::View2D) {
    OnDeleteView(deleteEvent);
    return true;
  }
  if (selectionState_.Current().kind == LayoutElementKind::Legend) {
    OnDeleteLegend(deleteEvent);
    return true;
  }
  if (selectionState_.Current().kind == LayoutElementKind::EventTable) {
    OnDeleteEventTable(deleteEvent);
    return true;
  }
  if (selectionState_.Current().kind == LayoutElementKind::Text) {
    OnDeleteText(deleteEvent);
    return true;
  }
  if (selectionState_.Current().kind == LayoutElementKind::Image) {
    OnDeleteImage(deleteEvent);
    return true;
  }
  return false;
}

void LayoutViewerPanel::OnKeyDown(wxKeyEvent &event) {
  if (gui::IsEditableWidgetFocused(wxWindow::FindFocus())) {
    event.Skip();
    return;
  }

  const int key = event.GetKeyCode();
  if (key == WXK_DELETE || key == WXK_NUMPAD_DELETE) {
    DeleteSelectedElement();
    return;
  }
  if (key == 'Z' || key == 'z') {
    ResetViewToFit();
    RequestRenderRebuild();
    Refresh();
    return;
  }
  event.Skip();
}

// Handles pane visibility changes and resumes pending automatic fits.
void LayoutViewerPanel::OnShow(wxShowEvent &event) {
  if (event.IsShown()) {
    InitGL();
    SchedulePendingFitToViewport();
    InvalidateRenderIfFrameChanged(true);
    if (isReadyToRender_ && NeedsRenderRebuild()) {
      RequestRenderRebuild();
    }
  }
  event.Skip();
}

// Routes pointer motion through frame-edit and viewport-pan policies.
void LayoutViewerPanel::OnMouseMove(wxMouseEvent &event) {
  const wxPoint currentPos = layoutviewerpanel::GetLogicalMousePosition(event);
  const gui::layoutinteraction::Point pointer{currentPos.x, currentPos.y};
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (GetSelectedFrame(selectedFrame) &&
      GetFrameRect(selectedFrame, frameRect)) {
    interactionSession_.UpdateHoverMode(gui::layoutinteraction::HitTestFrame(
        pointer, {frameRect.GetX(), frameRect.GetY(), frameRect.GetWidth(),
                  frameRect.GetHeight()}));
    SetCursor(CursorForMode(interactionSession_.HoverMode()));
  } else {
    interactionSession_.UpdateHoverMode(FrameDragMode::None);
    SetCursor(wxCursor(wxCURSOR_ARROW));
  }

  const FrameDragMode dragMode = interactionSession_.DragMode();
  if (dragMode != FrameDragMode::None && event.Dragging()) {
    SetCursor(CursorForMode(dragMode));
    std::optional<double> imageAspectRatio;
    if (selectionState_.Current().kind == LayoutElementKind::Image) {
      const auto *image = GetSelectedImage();
      imageAspectRatio = image ? image->aspectRatio : 0.0;
    }
    const layouts::Layout2DViewFrame frame =
        gui::layoutinteraction::ComputeDraggedFrame(
            dragMode, interactionSession_.DragStartFrame(),
            interactionSession_.DragStartPointer(), pointer,
            viewportState_.Zoom(), imageAspectRatio);
    const bool updatePosition = dragMode == FrameDragMode::Move;
    if (updatePosition) {
      ApplyFrameUpdateToSelection(frame, true);
    } else {
      interactionSession_.SetDeferredResize(frame);
      Refresh();
    }
    return;
  }

  if (!interactionSession_.IsPanning() || !event.Dragging())
    return;

  const gui::layoutinteraction::Point delta =
      interactionSession_.UpdatePan(pointer);
  viewportState_.ApplyPan({delta.x, delta.y});
  Refresh();
}

// Updates visual zoom around the cursor and defers high-quality cache
// rendering.
void LayoutViewerPanel::OnMouseWheel(wxMouseEvent &event) {
  if (interactionSession_.DragMode() != FrameDragMode::None)
    return;
  const wxSize size = layoutviewerpanel::GetLogicalClientSize(this);
  const wxPoint mousePos = layoutviewerpanel::GetLogicalMousePosition(event);
  if (!viewportState_.ApplyWheelZoom(
          event.GetWheelRotation(), event.GetWheelDelta(),
          {mousePos.x, mousePos.y}, {size.GetWidth(), size.GetHeight()},
          GetLayoutSafeMaxZoom(currentLayout)))
    return;
  InvalidateRenderIfFrameChanged(false);
  RequestRenderRebuild();
  Refresh();
}

// Commits pending movement and cancels transient state after capture loss.
void LayoutViewerPanel::OnCaptureLost(wxMouseCaptureLostEvent &) {
  mouseCapture_.AbandonOnLoss("active-gesture");
  CommitPendingFrameUpdate();
  if (interactionSession_.DragMode() != FrameDragMode::None) {
    layouts::LayoutManager::Get().EndBatchUpdate();
  }
  interactionSession_.CancelAfterCaptureLoss();
}

void LayoutViewerPanel::ApplyFrameUpdateToSelection(
    const layouts::Layout2DViewFrame &frame, bool updatePosition) {
  if (selectionState_.Current().kind == LayoutElementKind::Legend) {
    UpdateLegendFrame(frame, updatePosition);
  } else if (selectionState_.Current().kind == LayoutElementKind::EventTable) {
    UpdateEventTableFrame(frame, updatePosition);
  } else if (selectionState_.Current().kind == LayoutElementKind::Text) {
    UpdateTextFrame(frame, updatePosition);
  } else if (selectionState_.Current().kind == LayoutElementKind::Image) {
    UpdateImageFrame(frame, updatePosition);
  } else {
    UpdateFrame(frame, updatePosition);
  }
}

void LayoutViewerPanel::CommitPendingFrameUpdate() {
  if (!pendingFrameCommit_)
    return;
  pendingFrameCommit_ = false;
  if (currentLayout.name.empty())
    return;

  if (selectionState_.Current().kind == LayoutElementKind::View2D) {
    if (const auto *view = GetEditableView()) {
      layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name,
                                                       *view);
    }
    return;
  }

  if (selectionState_.Current().kind == LayoutElementKind::Legend) {
    if (const auto *legend = GetSelectedLegend()) {
      layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name,
                                                       *legend);
    }
    return;
  }

  if (selectionState_.Current().kind == LayoutElementKind::EventTable) {
    if (const auto *table = GetSelectedEventTable()) {
      layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                           *table);
    }
    return;
  }

  if (selectionState_.Current().kind == LayoutElementKind::Text) {
    if (const auto *text = GetSelectedText()) {
      layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *text);
    }
    return;
  }

  if (selectionState_.Current().kind == LayoutElementKind::Image) {
    if (const auto *image = GetSelectedImage()) {
      layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name,
                                                      *image);
    }
  }
}

void LayoutViewerPanel::OnRightUp(wxMouseEvent &event) {
  SetFocus();
  const wxPoint pos = layoutviewerpanel::GetLogicalMousePosition(event);
  if (!SelectElementAtPosition(pos)) {
    event.Skip();
    return;
  }
  layouts::Layout2DViewFrame selectedFrame;
  wxRect frameRect;
  if (!(GetSelectedFrame(selectedFrame) &&
        GetFrameRect(selectedFrame, frameRect) && frameRect.Contains(pos))) {
    event.Skip();
    return;
  }

  wxMenu menu;
  if (selectionState_.Current().kind == LayoutElementKind::View2D) {
    menu.Append(kEditMenuId, layoutviewerpanel::BuildEditViewMenuLabel());
    menu.AppendCheckItem(kToggleViewFrameMenuId,
                         layoutviewerpanel::BuildShowBorderMenuLabel());
    menu.Append(kDeleteMenuId, layoutviewerpanel::BuildDeleteViewMenuLabel());
    if (const auto *view = GetEditableView())
      menu.Check(kToggleViewFrameMenuId, view->drawFrame);
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId,
                layoutviewerpanel::BuildBringToFrontMenuLabel());
    menu.Append(kSendToBackMenuId,
                layoutviewerpanel::BuildSendToBackMenuLabel());
  } else if (selectionState_.Current().kind == LayoutElementKind::Legend) {
    menu.Append(kEditLegendMenuId,
                layoutviewerpanel::BuildEditLegendMenuLabel());
    menu.Append(kDeleteLegendMenuId,
                layoutviewerpanel::BuildDeleteLegendMenuLabel());
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId,
                layoutviewerpanel::BuildBringToFrontMenuLabel());
    menu.Append(kSendToBackMenuId,
                layoutviewerpanel::BuildSendToBackMenuLabel());
  } else if (selectionState_.Current().kind == LayoutElementKind::EventTable) {
    menu.Append(kEditEventTableMenuId,
                layoutviewerpanel::BuildEditEventTableMenuLabel());
    menu.Append(kDeleteEventTableMenuId,
                layoutviewerpanel::BuildDeleteEventTableMenuLabel());
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId,
                layoutviewerpanel::BuildBringToFrontMenuLabel());
    menu.Append(kSendToBackMenuId,
                layoutviewerpanel::BuildSendToBackMenuLabel());
  } else if (selectionState_.Current().kind == LayoutElementKind::Text) {
    menu.Append(kEditTextMenuId, layoutviewerpanel::BuildEditTextMenuLabel());
    menu.AppendCheckItem(kToggleTextFrameMenuId,
                         layoutviewerpanel::BuildShowBorderMenuLabel());
    menu.AppendCheckItem(
        kToggleTextTransparentBackgroundMenuId,
        layoutviewerpanel::BuildTransparentBackgroundMenuLabel());
    menu.Append(kDeleteTextMenuId,
                layoutviewerpanel::BuildDeleteTextMenuLabel());
    if (const auto *text = GetSelectedText()) {
      menu.Check(kToggleTextFrameMenuId, text->drawFrame);
      menu.Check(kToggleTextTransparentBackgroundMenuId,
                 !text->solidBackground);
    }
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId,
                layoutviewerpanel::BuildBringToFrontMenuLabel());
    menu.Append(kSendToBackMenuId,
                layoutviewerpanel::BuildSendToBackMenuLabel());
  } else if (selectionState_.Current().kind == LayoutElementKind::Image) {
    menu.Append(kEditImageMenuId,
                layoutviewerpanel::BuildChangeImageMenuLabel());
    menu.Append(kDeleteImageMenuId,
                layoutviewerpanel::BuildDeleteImageMenuLabel());
    menu.AppendSeparator();
    menu.Append(kBringToFrontMenuId,
                layoutviewerpanel::BuildBringToFrontMenuLabel());
    menu.Append(kSendToBackMenuId,
                layoutviewerpanel::BuildSendToBackMenuLabel());
  }
  PopupMenu(&menu, pos);
}

void LayoutViewerPanel::OnBringToFront(wxCommandEvent &) {
  if (selectionState_.Current().id < 0)
    return;
  const int targetZ =
      gui::layoutselection::BringToFrontTarget(BuildZOrderedElements());
  if (selectionState_.Current().kind == LayoutElementKind::View2D) {
    auto it = std::find_if(currentLayout.view2dViews.begin(),
                           currentLayout.view2dViews.end(),
                           [this](const auto &entry) {
                             return entry.id == selectionState_.Current().id;
                           });
    if (it == currentLayout.view2dViews.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name, *it);
    }
  } else if (selectionState_.Current().kind == LayoutElementKind::Legend) {
    auto it = std::find_if(currentLayout.legendViews.begin(),
                           currentLayout.legendViews.end(),
                           [this](const auto &entry) {
                             return entry.id == selectionState_.Current().id;
                           });
    if (it == currentLayout.legendViews.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name, *it);
    }
  } else if (selectionState_.Current().kind == LayoutElementKind::EventTable) {
    auto it = std::find_if(currentLayout.eventTables.begin(),
                           currentLayout.eventTables.end(),
                           [this](const auto &entry) {
                             return entry.id == selectionState_.Current().id;
                           });
    if (it == currentLayout.eventTables.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                           *it);
    }
  } else if (selectionState_.Current().kind == LayoutElementKind::Text) {
    auto it =
        std::find_if(currentLayout.textViews.begin(),
                     currentLayout.textViews.end(), [this](const auto &entry) {
                       return entry.id == selectionState_.Current().id;
                     });
    if (it == currentLayout.textViews.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *it);
    }
  } else if (selectionState_.Current().kind == LayoutElementKind::Image) {
    auto it =
        std::find_if(currentLayout.imageViews.begin(),
                     currentLayout.imageViews.end(), [this](const auto &entry) {
                       return entry.id == selectionState_.Current().id;
                     });
    if (it == currentLayout.imageViews.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("bring layout element to front");
      layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name, *it);
    }
  } else {
    return;
  }
  layoutVersion++;
  InvalidateSelectionIndexCache();
  renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

void LayoutViewerPanel::OnSendToBack(wxCommandEvent &) {
  if (selectionState_.Current().id < 0)
    return;
  const int targetZ =
      gui::layoutselection::SendToBackTarget(BuildZOrderedElements());
  if (selectionState_.Current().kind == LayoutElementKind::View2D) {
    auto it = std::find_if(currentLayout.view2dViews.begin(),
                           currentLayout.view2dViews.end(),
                           [this](const auto &entry) {
                             return entry.id == selectionState_.Current().id;
                           });
    if (it == currentLayout.view2dViews.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayout2DView(currentLayout.name, *it);
    }
  } else if (selectionState_.Current().kind == LayoutElementKind::Legend) {
    auto it = std::find_if(currentLayout.legendViews.begin(),
                           currentLayout.legendViews.end(),
                           [this](const auto &entry) {
                             return entry.id == selectionState_.Current().id;
                           });
    if (it == currentLayout.legendViews.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayoutLegend(currentLayout.name, *it);
    }
  } else if (selectionState_.Current().kind == LayoutElementKind::EventTable) {
    auto it = std::find_if(currentLayout.eventTables.begin(),
                           currentLayout.eventTables.end(),
                           [this](const auto &entry) {
                             return entry.id == selectionState_.Current().id;
                           });
    if (it == currentLayout.eventTables.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayoutEventTable(currentLayout.name,
                                                           *it);
    }
  } else if (selectionState_.Current().kind == LayoutElementKind::Text) {
    auto it =
        std::find_if(currentLayout.textViews.begin(),
                     currentLayout.textViews.end(), [this](const auto &entry) {
                       return entry.id == selectionState_.Current().id;
                     });
    if (it == currentLayout.textViews.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayoutText(currentLayout.name, *it);
    }
  } else if (selectionState_.Current().kind == LayoutElementKind::Image) {
    auto it =
        std::find_if(currentLayout.imageViews.begin(),
                     currentLayout.imageViews.end(), [this](const auto &entry) {
                       return entry.id == selectionState_.Current().id;
                     });
    if (it == currentLayout.imageViews.end())
      return;
    it->zIndex = targetZ;
    if (!currentLayout.name.empty()) {
      auto &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
      cfg.PushUndoState("send layout element to back");
      layouts::LayoutManager::Get().UpdateLayoutImage(currentLayout.name, *it);
    }
  } else {
    return;
  }
  layoutVersion++;
  InvalidateSelectionIndexCache();
  renderDirty = true;
  RequestRenderRebuild();
  Refresh();
}

// Reports whether the current client area can produce a reliable automatic fit.
bool LayoutViewerPanel::IsViewportReadyForAutomaticFit() const {
  const wxSize size = layoutviewerpanel::GetLogicalClientSize(this);
  return gui::layoutviewport::IsViewportReadyForAutomaticFit(
      {size.GetWidth(), size.GetHeight()}, IsShownOnScreen());
}

// Attempts to consume the pending automatic fit with the current viewport.
bool LayoutViewerPanel::TryCompletePendingFitToViewport() {
  const wxSize size = layoutviewerpanel::GetLogicalClientSize(this);
  if (!viewportState_.ConsumeAutomaticFitIfReady(
          {size.GetWidth(), size.GetHeight()}, IsShownOnScreen()))
    return false;
  ResetViewToFit();
  InvalidateRenderIfFrameChanged(false);
  RequestRenderRebuild();
  Refresh();
  return true;
}

// Schedules at most one deferred automatic-fit attempt for the pending request.
void LayoutViewerPanel::SchedulePendingFitToViewport() {
  if (!viewportState_.HasPendingAutomaticFit() ||
      deferredFitToViewportScheduled_)
    return;
  deferredFitToViewportScheduled_ = true;
  wxWeakRef<LayoutViewerPanel> weakThis(this);
  CallAfter([weakThis]() {
    if (!weakThis)
      return;
    LayoutViewerPanel *panel = weakThis.get();
    if (!panel)
      return;
    panel->deferredFitToViewportScheduled_ = false;
    panel->TryCompletePendingFitToViewport();
  });
}

// Resets the layout camera so the page fits within the current viewport.
void LayoutViewerPanel::ResetViewToFit() {
  const wxSize size = layoutviewerpanel::GetLogicalClientSize(this);
  viewportState_.Fit({size.GetWidth(), size.GetHeight()},
                     currentLayout.pageSetup.PageWidthPt(),
                     currentLayout.pageSetup.PageHeightPt());
  InvalidateRenderIfFrameChanged(false);
}

// Adapts pure page geometry to the wx rectangle used for rendering.
wxRect LayoutViewerPanel::GetPageRect() const {
  const wxSize size = layoutviewerpanel::GetLogicalClientSize(this);
  const auto rect = viewportState_.PageRect(
      {size.GetWidth(), size.GetHeight()},
      currentLayout.pageSetup.PageWidthPt(),
      currentLayout.pageSetup.PageHeightPt());
  return wxRect(rect.x, rect.y, rect.width, rect.height);
}

// Adapts pure frame geometry to the wx rectangle used for hit testing.
bool LayoutViewerPanel::GetFrameRect(const layouts::Layout2DViewFrame &frame,
                                     wxRect &rect) const {
  const wxSize size = layoutviewerpanel::GetLogicalClientSize(this);
  gui::layoutviewport::Rect viewportRect;
  if (!viewportState_.FrameRect(
          {size.GetWidth(), size.GetHeight()},
          currentLayout.pageSetup.PageWidthPt(),
          currentLayout.pageSetup.PageHeightPt(),
          {static_cast<double>(frame.x), static_cast<double>(frame.y),
           static_cast<double>(frame.width), static_cast<double>(frame.height)},
          viewportRect))
    return false;
  rect = wxRect(viewportRect.x, viewportRect.y, viewportRect.width,
                viewportRect.height);
  return true;
}

// Calculates a raster frame size for the requested cache zoom.
wxSize
LayoutViewerPanel::GetFrameSizeForZoom(const layouts::Layout2DViewFrame &frame,
                                       double targetZoom) const {
  if (frame.width <= 0 || frame.height <= 0 || targetZoom <= 0.0)
    return wxSize(0, 0);
  const double scaledWidthValue = frame.width * targetZoom;
  const double scaledHeightValue = frame.height * targetZoom;
  if (scaledWidthValue > kMaxRenderDimension ||
      scaledHeightValue > kMaxRenderDimension)
    return wxSize(0, 0);
  const int scaledWidth = static_cast<int>(std::lround(scaledWidthValue));
  const int scaledHeight = static_cast<int>(std::lround(scaledHeightValue));
  if (scaledWidth <= 0 || scaledHeight <= 0)
    return wxSize(0, 0);
  if (scaledWidth > kMaxRenderDimension || scaledHeight > kMaxRenderDimension)
    return wxSize(0, 0);
  if (static_cast<size_t>(scaledWidth) >
      kMaxRenderPixels / static_cast<size_t>(scaledHeight))
    return wxSize(0, 0);
  const size_t pixelCount =
      static_cast<size_t>(scaledWidth) * static_cast<size_t>(scaledHeight);
  if (pixelCount > kMaxRenderPixels || pixelCount > kMaxRenderBytes / 4)
    return wxSize(0, 0);
  return wxSize(scaledWidth, scaledHeight);
}

// Quantizes the current visual zoom into a stable raster-cache LOD bucket.
double LayoutViewerPanel::GetRenderZoom() const {
  if (viewportState_.Zoom() <= 0.0)
    return kMinZoom;

  const double safeMaxZoom = GetLayoutSafeMaxZoom(currentLayout);
  const double zoomSteps = std::log(viewportState_.Zoom()) / std::log(kZoomStep);
  const double bucketSteps =
      std::round(zoomSteps / kZoomCacheStepsPerLevel) * kZoomCacheStepsPerLevel;
  const double bucketZoom = std::pow(kZoomStep, bucketSteps);
  return std::clamp(bucketZoom, kMinZoom, safeMaxZoom);
}

// Determines whether a cached raster is far enough from the target LOD to
// rebuild.
bool LayoutViewerPanel::ShouldRebuildCacheForRenderZoom(
    double cachedRenderZoom, double targetRenderZoom) const {
  if (cachedRenderZoom <= 0.0)
    return true;
  if (targetRenderZoom <= 0.0)
    return true;

  const double levelRatio = std::pow(kZoomStep, kZoomCacheStepsPerLevel);
  const double halfLevelRatio = std::sqrt(levelRatio);
  return targetRenderZoom >= cachedRenderZoom * halfLevelRatio ||
         targetRenderZoom <= cachedRenderZoom / halfLevelRatio;
}

// Retrieves the frame definition for the currently selected layout element.
bool LayoutViewerPanel::GetSelectedFrame(
    layouts::Layout2DViewFrame &frame) const {
  if (selectionState_.Current().kind == LayoutElementKind::Legend) {
    const auto *legend = GetSelectedLegend();
    if (!legend)
      return false;
    frame = legend->frame;
    return true;
  }
  if (selectionState_.Current().kind == LayoutElementKind::EventTable) {
    const auto *table = GetSelectedEventTable();
    if (!table)
      return false;
    frame = table->frame;
    return true;
  }
  if (selectionState_.Current().kind == LayoutElementKind::Text) {
    const auto *text = GetSelectedText();
    if (!text)
      return false;
    frame = text->frame;
    return true;
  }
  if (selectionState_.Current().kind == LayoutElementKind::Image) {
    const auto *image = GetSelectedImage();
    if (!image)
      return false;
    frame = image->frame;
    return true;
  }
  const auto *view = GetEditableView();
  if (!view)
    return false;
  frame = view->frame;
  return true;
}

bool LayoutViewerPanel::InitGL() {
  if (!glContext_)
    return false;
  if (!IsShownOnScreen())
    return false;
  if (!gl_lifecycle::TrySetCurrent(*this, glContext_, "LayoutViewerPanel",
                                   "InitGL"))
    return false;

  if (!glInitialized_) {
    const GLEWInitResult initResult =
        gl_lifecycle::InitializeGlew(*this, *glContext_, "LayoutViewerPanel");
    if (!initResult.success) {
      Logger::Instance().Log(initResult.message);
      return false;
    }
    if (initResult.isWarningOnly) {
      wxLogDebug("%s", initResult.message);
    }
    glInitialized_ = true;
  }

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  isReadyToRender_ = true;
  return true;
}

// Rebuilds one stale layout element texture per call so the viewer can update
// progressively.
bool LayoutViewerPanel::RebuildCachedTexture() {
  try {
    gui::layoutperf::LayoutRenderProfiler profiler("layout_render_rebuild");
    profiler.SetLayoutContext(
        layouts::LayoutManager::Get().GetLayouts().Count(), currentLayout);
    profiler.BeginPhase("preflight");
    if (!NeedsRenderRebuild()) {
      profiler.Finish("clean");
      return false;
    }
    if (!isReadyToRender_ || !glContext_ || !IsShownOnScreen()) {
      profiler.Finish("not_ready");
      return false;
    }
    profiler.EndPhase();
    Viewer2DOffscreenRenderer *offscreenRenderer = nullptr;
    Viewer2DPanel *capturePanel = nullptr;
    auto stopLoadingRequest = [this]() {
      loadingRequested = false;
      if (loadingTimer_.IsRunning())
        loadingTimer_.Stop();
    };
    auto clearLoadingState = [this, stopLoadingRequest]() {
      stopLoadingRequest();
      isLoading = false;
    };
    renderDirty = false;
    gui::layoutstatus::PostLayoutRenderStatus(
        this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
        "Analyzing layout render workload...");

    const size_t totalRenderItems =
        currentLayout.view2dViews.size() + currentLayout.legendViews.size() +
        currentLayout.eventTables.size() + currentLayout.textViews.size() +
        currentLayout.imageViews.size();
    size_t processedRenderItems = 0;
    auto postRenderProgressStatus = [this, &processedRenderItems,
                                     totalRenderItems](const wxString &stage,
                                                       size_t stageIndex,
                                                       size_t stageTotal) {
      const size_t safeTotal = std::max<size_t>(1, totalRenderItems);
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          stage + wxString::Format(
                      " (%zu/%zu) · global %zu/%zu", stageIndex, stageTotal,
                      std::min(processedRenderItems, safeTotal), safeTotal));
    };

    bool needsViewSceneCapture = false;
    for (const auto &view : currentLayout.view2dViews) {
      const auto cacheIt = viewCaches_.find(view.id);
      if (cacheIt != viewCaches_.end() && cacheIt->second.renderDirty) {
        needsViewSceneCapture = needsViewSceneCapture ||
                                !cacheIt->second.restoredFromPersistentCache;
      }
    }
    bool needsLegendProcessing = false;
    bool needsLegendSymbolCapture = false;
    size_t legendSymbolsMissingCount = 0;
    const double renderZoom = GetRenderZoom();
    for (const auto &legend : currentLayout.legendViews) {
      const size_t legendContentHash = ComputeLegendContentHash(legend);
      const auto cacheIt = legendCaches_.find(legend.id);
      const bool cacheMissing = cacheIt == legendCaches_.end();
      const bool contentChanged =
          !cacheMissing && cacheIt->second.contentHash != legendContentHash;
      const bool needsTextureRebuild =
          cacheMissing || cacheIt->second.renderDirty || contentChanged;
      if (needsTextureRebuild) {
        needsLegendProcessing = true;
        const bool hasMatchingRaster =
            !cacheMissing && cacheIt->second.persistentRaster.IsValid() &&
            cacheIt->second.persistentRaster.contentHash == legendContentHash &&
            cacheIt->second.persistentRaster.renderZoom == renderZoom &&
            cacheIt->second.persistentRaster.size ==
                GetFrameSizeForZoom(legend.frame, renderZoom);
        const bool needsSymbols =
            !hasMatchingRaster && (cacheMissing || !cacheIt->second.symbols);
        if (needsSymbols) {
          needsLegendSymbolCapture = true;
          ++legendSymbolsMissingCount;
        }
      }
      if (needsLegendProcessing && needsLegendSymbolCapture) {
        break;
      }
    }
    gui::layoutstatus::PostLayoutRenderStatus(
        this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
        wxString::Format("Workload: %zu views, %zu legends (symbol recapture: ",
                         currentLayout.view2dViews.size(),
                         currentLayout.legendViews.size()) +
            (needsLegendSymbolCapture ? "yes)." : "no)."));
    const bool needsCapturePanel =
        needsViewSceneCapture || needsLegendSymbolCapture;
    profiler.BeginPhase("prepare_capture_panel");
    if (needsCapturePanel) {
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          "Preparing offscreen renderer for layout capture...");
      if (auto *mw = MainWindow::Instance()) {
        offscreenRenderer = mw->GetOffscreenRenderer();
        capturePanel =
            offscreenRenderer ? offscreenRenderer->GetPanel() : nullptr;
      }
      if (!capturePanel || !offscreenRenderer) {
        profiler.Finish("capture_panel_unavailable");
        return false;
      }
    }
    profiler.EndPhase();

    ConfigManager &cfg = GetDefaultGuiConfigServices().LegacyConfigManager();
    std::shared_ptr<const SymbolDefinitionSnapshot> legendSymbols;
    profiler.BeginPhase("legend_symbol_capture");
    if (needsLegendSymbolCapture) {
      const auto legendCaptureStartedAt = std::chrono::steady_clock::now();
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format(
              "Capturing legend symbols (%zu legend(s) missing symbols)...",
              legendSymbolsMissingCount));
      legendSymbols = CaptureLegendSymbolSnapshot(capturePanel, cfg, true);
      if (startupMetrics_) {
        ++startupMetrics_->legendSymbolCaptureCount;
        startupMetrics_->legendSymbolCaptureMs +=
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - legendCaptureStartedAt)
                .count();
      }
      if (legendSymbols) {
        for (const auto &legend : currentLayout.legendViews) {
          LegendCache &cache = GetLegendCache(legend.id);
          if (cache.renderDirty || cache.contentHash != legendDataHash ||
              !cache.symbols) {
            cache.symbols = legendSymbols;
          }
        }
      }
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          "Legend symbol capture completed.");
    }
    profiler.EndPhase();
    std::vector<unsigned char> legendPixels;
    std::vector<unsigned char> eventTablePixels;
    std::vector<unsigned char> textPixels;
    std::vector<unsigned char> imagePixels;
    bool glReady = false;
    auto ensureGlReady = [&]() {
      if (glReady)
        return true;
      glReady = InitGL();
      return glReady;
    };
    profiler.BeginPhase("2d_views");
    gui::layoutstatus::PostLayoutRenderStatus(
        this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
        wxString::Format("Rendering 2D views (%zu)...",
                         currentLayout.view2dViews.size()));
    for (const auto &view : currentLayout.view2dViews) {
      ++processedRenderItems;
      postRenderProgressStatus("Rendering 2D views", processedRenderItems,
                               currentLayout.view2dViews.size());
      ViewCache &cache = GetViewCache(view.id);
      if (!cache.renderDirty) {
        profiler.RecordReusedElement();
        continue;
      }
      cache.renderDirty = false;
      wxRect frameRect;
      if (!GetFrameRect(view.frame, frameRect)) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        cache.hasLastRenderFailure = true;
        cache.lastRenderFailureReason = gui::layoutraster::
            Layout2DViewRasterFailureReason::InvalidFrameSize;
        cache.lastRenderFailureMessage = "invalid frame size";
        continue;
      }

      const wxSize renderSize = GetFrameSizeForZoom(view.frame, renderZoom);
      if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        cache.hasLastRenderFailure = true;
        cache.lastRenderFailureReason = gui::layoutraster::
            Layout2DViewRasterFailureReason::InvalidFrameSize;
        cache.lastRenderFailureMessage = "invalid frame size";
        continue;
      }

      gui::layoutraster::Layout2DViewRasterizer rasterizer(
          cfg, offscreenRenderer, capturePanel);
      const size_t viewContentHash = HashViewContent(view);
      gui::layoutraster::Layout2DViewRasterRequest rasterRequest;
      rasterRequest.view = &view;
      rasterRequest.renderSize = renderSize;
      rasterRequest.renderZoom = renderZoom;
      rasterRequest.contentHash = viewContentHash;

      gui::layoutraster::Layout2DViewRasterCacheInput rasterCacheInput;
      rasterCacheInput.hasCapture = cache.hasCapture;
      rasterCacheInput.hasRenderState = cache.hasRenderState;
      rasterCacheInput.restoredFromPersistentCache =
          cache.restoredFromPersistentCache;
      rasterCacheInput.buffer = &cache.buffer;
      rasterCacheInput.viewState = &cache.viewState;
      rasterCacheInput.renderState = &cache.renderState;
      rasterCacheInput.symbols = cache.symbols.get();
      rasterCacheInput.persistentRgba = &cache.persistentRgba;
      rasterCacheInput.persistentRgbaSize = cache.persistentRgbaSize;
      rasterCacheInput.persistentRgbaRenderZoom =
          cache.persistentRgbaRenderZoom;
      rasterCacheInput.persistentRgbaContentHash =
          cache.persistentRgbaContentHash;

      if (!rasterCacheInput.restoredFromPersistentCache &&
          !rasterCacheInput.persistentRgba->empty()) {
        wxLogTrace(
            "layoutviewer_raster",
            "Rasterizing 2D view id=%d from persistent RGBA cache when valid.",
            view.id);
      }
      if (!capturePanel || !offscreenRenderer) {
        wxLogTrace("layoutviewer_raster",
                   "Rasterizing 2D view id=%d without capture panel; only "
                   "cached paths can succeed.",
                   view.id);
      }

      gui::layoutraster::Layout2DViewRasterResult rasterResult =
          rasterizer.Rasterize(rasterRequest, rasterCacheInput);
      if (rasterResult.rejectedRestoredPersistentCache)
        cache.restoredFromPersistentCache = false;
      if (!rasterResult.success) {
        cache.hasLastRenderFailure = true;
        cache.lastRenderFailureReason = rasterResult.failureReason;
        cache.lastRenderFailureMessage = rasterResult.diagnosticMessage;
        if (cache.lastLoggedFailureContentHash != viewContentHash ||
            cache.lastLoggedFailureMessage != rasterResult.diagnosticMessage) {
          wxLogTrace(
              "layoutviewer_raster",
              "Rasterizing 2D view id=%d failed size=%dx%d zoom=%.3f hash=%zu: "
              "%s",
              view.id, renderSize.GetWidth(), renderSize.GetHeight(),
              renderZoom, viewContentHash,
              wxString::FromUTF8(rasterResult.diagnosticMessage).c_str());
          cache.lastLoggedFailureContentHash = viewContentHash;
          cache.lastLoggedFailureMessage = rasterResult.diagnosticMessage;
        }
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      std::vector<unsigned char> pixels = std::move(rasterResult.rgbaPixels);
      const int width = rasterResult.width;
      const int height = rasterResult.height;
      if (!ensureGlReady()) {
        clearLoadingState();
        NotifyRenderReady();
        return false;
      }
      if (cache.texture == 0) {
        glGenTextures(1, &cache.texture);
      }
      glBindTexture(GL_TEXTURE_2D, cache.texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo,
                                           cache.pboBytes);
      if (!UploadRgbaToTexture(cache.texture, width, height, pixels.data(),
                               cache.textureSize, true)) {
        cache.hasLastRenderFailure = true;
        cache.lastRenderFailureReason = gui::layoutraster::
            Layout2DViewRasterFailureReason::TextureUploadFailed;
        cache.lastRenderFailureMessage = "GPU texture upload failed";
        wxLogTrace("layoutviewer_raster",
                   "Uploading 2D view texture id=%d failed size=%dx%d "
                   "zoom=%.3f hash=%zu: %s",
                   view.id, width, height, renderZoom, viewContentHash,
                   cache.lastRenderFailureMessage.c_str());
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format(
              "Rendering 2D view id=%d: GPU texture upload completed.",
              view.id));
      cache.textureSize = wxSize(width, height);
      cache.renderZoom = renderZoom;
      cache.contentHash = viewContentHash;
      cache.hasLastRenderFailure = false;
      cache.lastRenderFailureReason =
          gui::layoutraster::Layout2DViewRasterFailureReason::None;
      cache.lastRenderFailureMessage.clear();
      cache.persistentRgba = pixels;
      cache.persistentRgbaSize = cache.textureSize;
      cache.persistentRgbaRenderZoom = renderZoom;
      cache.persistentRgbaContentHash = cache.contentHash;
      profiler.RecordRenderedElement();
      std::vector<unsigned char>().swap(pixels);
      if (rasterResult.reusedPersistentRaster)
        continue;
      const bool hasMoreWork = NeedsRenderRebuild();
      profiler.Finish(hasMoreWork ? "incremental_pending" : "completed");
      return hasMoreWork;
    }

    profiler.BeginPhase("legends");
    gui::layoutstatus::PostLayoutRenderStatus(
        this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
        wxString::Format("Rendering legends (%zu)...",
                         currentLayout.legendViews.size()));
    for (const auto &legend : currentLayout.legendViews) {
      ++processedRenderItems;
      const size_t legendStageIndex =
          processedRenderItems - currentLayout.view2dViews.size();
      postRenderProgressStatus("Rendering legends", legendStageIndex,
                               currentLayout.legendViews.size());
      LegendCache &cache = GetLegendCache(legend.id);
      const std::vector<LegendItem> legendItems = BuildLegendItems(&legend);
      const size_t legendContentHash = HashLegendItems(legendItems, &legend);
      const bool contentChanged = cache.contentHash != legendContentHash;
      const bool requiresSymbolRefresh = !cache.symbols;
      if (requiresSymbolRefresh && legendSymbols &&
          cache.symbols != legendSymbols) {
        cache.symbols = legendSymbols;
        cache.renderDirty = true;
      }
      if (contentChanged) {
        cache.renderDirty = true;
      }
      if (!cache.renderDirty) {
        profiler.RecordReusedElement();
        continue;
      }
      cache.renderDirty = false;

      const wxSize renderSize = GetFrameSizeForZoom(legend.frame, renderZoom);
      if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }

      const bool reusePersistentRaster =
          cache.persistentRaster.IsValid() &&
          cache.persistentRaster.contentHash == legendContentHash &&
          cache.persistentRaster.renderZoom == renderZoom &&
          cache.persistentRaster.size == renderSize;
      int width = renderSize.GetWidth();
      int height = renderSize.GetHeight();
      if (reusePersistentRaster) {
        legendPixels = cache.persistentRaster.rgba;
      } else {
        gui::layoutstatus::PostLayoutRenderStatus(
            this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
            wxString::Format("Rendering legend id=%d (%zu/%zu): rasterizing "
                             "legend content...",
                             legend.id, processedRenderItems,
                             std::max<size_t>(1, totalRenderItems)));
        wxImage image = BuildLegendImage(
            renderSize, wxSize(legend.frame.width, legend.frame.height),
            renderZoom, legendItems, legend, cache.symbols.get());
        if (!image.IsOk()) {
          ClearCachedTexture(cache);
          cache.textureSize = wxSize(0, 0);
          cache.renderZoom = 0.0;
          continue;
        }
        image = image.Mirror(false);
        if (!image.HasAlpha())
          image.InitAlpha();
        width = image.GetWidth();
        height = image.GetHeight();
        const unsigned char *rgb = image.GetData();
        const unsigned char *alpha = image.GetAlpha();
        if (!rgb || width <= 0 || height <= 0 ||
            !TryAllocatePixelBuffer(legendPixels, width, height, "legend")) {
          ClearCachedTexture(cache);
          cache.textureSize = wxSize(0, 0);
          cache.renderZoom = 0.0;
          continue;
        }
        for (int i = 0; i < width * height; ++i) {
          legendPixels[static_cast<size_t>(i) * 4] = rgb[i * 3];
          legendPixels[static_cast<size_t>(i) * 4 + 1] = rgb[i * 3 + 1];
          legendPixels[static_cast<size_t>(i) * 4 + 2] = rgb[i * 3 + 2];
          legendPixels[static_cast<size_t>(i) * 4 + 3] = alpha ? alpha[i] : 255;
        }
      }

      if (!ensureGlReady()) {
        clearLoadingState();
        NotifyRenderReady();
        return false;
      }
      if (cache.texture == 0) {
        glGenTextures(1, &cache.texture);
      }
      glBindTexture(GL_TEXTURE_2D, cache.texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo,
                                           cache.pboBytes);
      if (!UploadRgbaToTexture(cache.texture, width, height,
                               legendPixels.data(), cache.textureSize, true)) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format(
              "Rendering legend id=%d: GPU texture upload completed.",
              legend.id));
      cache.textureSize = wxSize(width, height);
      cache.renderZoom = renderZoom;
      cache.contentHash = legendContentHash;
      cache.persistentRaster = {legendPixels, wxSize(width, height), renderZoom,
                                legendContentHash};
      profiler.RecordRenderedElement();
      legendPixels.clear();
      const bool hasMoreWork = NeedsRenderRebuild();
      profiler.Finish(hasMoreWork ? "incremental_pending" : "completed");
      return hasMoreWork;
    }

    profiler.BeginPhase("event_tables");
    gui::layoutstatus::PostLayoutRenderStatus(
        this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
        wxString::Format("Rendering event tables (%zu)...",
                         currentLayout.eventTables.size()));
    for (const auto &table : currentLayout.eventTables) {
      ++processedRenderItems;
      const size_t tableStageIndex = processedRenderItems -
                                     currentLayout.view2dViews.size() -
                                     currentLayout.legendViews.size();
      postRenderProgressStatus("Rendering event tables", tableStageIndex,
                               currentLayout.eventTables.size());
      EventTableCache &cache = GetEventTableCache(table.id);
      size_t dataHash = HashEventTableFields(table);
      if (cache.contentHash != dataHash)
        cache.renderDirty = true;
      if (!cache.renderDirty) {
        profiler.RecordReusedElement();
        continue;
      }
      cache.renderDirty = false;

      const wxSize renderSize = GetFrameSizeForZoom(table.frame, renderZoom);
      if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }

      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format("Rendering event table id=%d (%zu/%zu): generating "
                           "table image...",
                           table.id, processedRenderItems,
                           std::max<size_t>(1, totalRenderItems)));
      wxImage image = BuildEventTableImage(
          renderSize, wxSize(table.frame.width, table.frame.height), renderZoom,
          table);
      if (!image.IsOk()) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      image = image.Mirror(false);
      if (!image.HasAlpha())
        image.InitAlpha();
      const int width = image.GetWidth();
      const int height = image.GetHeight();
      const unsigned char *rgb = image.GetData();
      const unsigned char *alpha = image.GetAlpha();
      if (!rgb || width <= 0 || height <= 0) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }

      if (!TryAllocatePixelBuffer(eventTablePixels, width, height,
                                  "event table")) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      const bool needsUnpremultiply = false;
      for (int i = 0; i < width * height; ++i) {
        const unsigned char a = alpha ? alpha[i] : 255;
        unsigned char r = rgb[i * 3];
        unsigned char g = rgb[i * 3 + 1];
        unsigned char b = rgb[i * 3 + 2];
        if (needsUnpremultiply && a > 0 && a < 255) {
          r = static_cast<unsigned char>(
              std::min(255, static_cast<int>(r) * 255 / a));
          g = static_cast<unsigned char>(
              std::min(255, static_cast<int>(g) * 255 / a));
          b = static_cast<unsigned char>(
              std::min(255, static_cast<int>(b) * 255 / a));
        }
        eventTablePixels[static_cast<size_t>(i) * 4] = r;
        eventTablePixels[static_cast<size_t>(i) * 4 + 1] = g;
        eventTablePixels[static_cast<size_t>(i) * 4 + 2] = b;
        eventTablePixels[static_cast<size_t>(i) * 4 + 3] = a;
      }

      if (!ensureGlReady()) {
        clearLoadingState();
        NotifyRenderReady();
        return false;
      }
      if (cache.texture == 0) {
        glGenTextures(1, &cache.texture);
      }
      glBindTexture(GL_TEXTURE_2D, cache.texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo,
                                           cache.pboBytes);
      if (!UploadRgbaToTexture(cache.texture, width, height,
                               eventTablePixels.data(), cache.textureSize,
                               true)) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format(
              "Rendering event table id=%d: GPU texture upload completed.",
              table.id));
      cache.textureSize = wxSize(width, height);
      cache.renderZoom = renderZoom;
      cache.contentHash = dataHash;
      profiler.RecordRenderedElement();
      eventTablePixels.clear();
      const bool hasMoreWork = NeedsRenderRebuild();
      profiler.Finish(hasMoreWork ? "incremental_pending" : "completed");
      return hasMoreWork;
    }

    profiler.BeginPhase("text_blocks");
    gui::layoutstatus::PostLayoutRenderStatus(
        this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
        wxString::Format("Rendering text blocks (%zu)...",
                         currentLayout.textViews.size()));
    for (const auto &text : currentLayout.textViews) {
      ++processedRenderItems;
      const size_t textStageIndex =
          processedRenderItems - currentLayout.view2dViews.size() -
          currentLayout.legendViews.size() - currentLayout.eventTables.size();
      postRenderProgressStatus("Rendering text blocks", textStageIndex,
                               currentLayout.textViews.size());
      TextCache &cache = GetTextCache(text.id);
      size_t dataHash = HashTextContent(text);
      if (cache.contentHash != dataHash)
        cache.renderDirty = true;
      if (!cache.renderDirty) {
        profiler.RecordReusedElement();
        continue;
      }
      cache.renderDirty = false;

      const wxSize renderSize = GetFrameSizeForZoom(text.frame, renderZoom);
      if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }

      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format("Rendering text block id=%d (%zu/%zu): rasterizing "
                           "text layout...",
                           text.id, processedRenderItems,
                           std::max<size_t>(1, totalRenderItems)));
      wxImage image = BuildTextImage(
          renderSize, wxSize(text.frame.width, text.frame.height), renderZoom,
          text);
      if (!image.IsOk()) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      image = image.Mirror(false);
      if (!image.HasAlpha())
        image.InitAlpha();
      const int width = image.GetWidth();
      const int height = image.GetHeight();
      const unsigned char *rgb = image.GetData();
      const unsigned char *alpha = image.GetAlpha();
      if (!rgb || width <= 0 || height <= 0) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }

      if (!TryAllocatePixelBuffer(textPixels, width, height, "text")) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      const bool needsUnpremultiply = !text.solidBackground;
      for (int i = 0; i < width * height; ++i) {
        const unsigned char a = alpha ? alpha[i] : 255;
        unsigned char r = rgb[i * 3];
        unsigned char g = rgb[i * 3 + 1];
        unsigned char b = rgb[i * 3 + 2];
        if (needsUnpremultiply && a > 0 && a < 255) {
          r = static_cast<unsigned char>(
              std::min(255, static_cast<int>(r) * 255 / a));
          g = static_cast<unsigned char>(
              std::min(255, static_cast<int>(g) * 255 / a));
          b = static_cast<unsigned char>(
              std::min(255, static_cast<int>(b) * 255 / a));
        }
        textPixels[static_cast<size_t>(i) * 4] = r;
        textPixels[static_cast<size_t>(i) * 4 + 1] = g;
        textPixels[static_cast<size_t>(i) * 4 + 2] = b;
        textPixels[static_cast<size_t>(i) * 4 + 3] = a;
      }

      if (!ensureGlReady()) {
        clearLoadingState();
        NotifyRenderReady();
        return false;
      }
      if (cache.texture == 0) {
        glGenTextures(1, &cache.texture);
      }
      glBindTexture(GL_TEXTURE_2D, cache.texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo,
                                           cache.pboBytes);
      if (!UploadRgbaToTexture(cache.texture, width, height, textPixels.data(),
                               cache.textureSize, true)) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format(
              "Rendering text block id=%d: GPU texture upload completed.",
              text.id));
      cache.textureSize = wxSize(width, height);
      cache.renderZoom = renderZoom;
      cache.contentHash = dataHash;
      profiler.RecordRenderedElement();
      textPixels.clear();
      const bool hasMoreWork = NeedsRenderRebuild();
      profiler.Finish(hasMoreWork ? "incremental_pending" : "completed");
      return hasMoreWork;
    }

    profiler.BeginPhase("images");
    gui::layoutstatus::PostLayoutRenderStatus(
        this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
        wxString::Format("Rendering images (%zu)...",
                         currentLayout.imageViews.size()));
    for (const auto &image : currentLayout.imageViews) {
      ++processedRenderItems;
      const size_t imageStageIndex =
          processedRenderItems - currentLayout.view2dViews.size() -
          currentLayout.legendViews.size() - currentLayout.eventTables.size() -
          currentLayout.textViews.size();
      postRenderProgressStatus("Rendering images", imageStageIndex,
                               currentLayout.imageViews.size());
      ImageCache &cache = GetImageCache(image.id);
      size_t dataHash = HashImageContent(image);
      if (cache.contentHash != dataHash)
        cache.renderDirty = true;
      if (!cache.renderDirty) {
        profiler.RecordReusedElement();
        continue;
      }
      cache.renderDirty = false;

      const wxSize renderSize = GetFrameSizeForZoom(image.frame, renderZoom);
      if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      if (image.imagePath.empty()) {
        ClearCachedTexture(cache);
        ClearCachedImageSource(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }

      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format(
              "Rendering image id=%d (%zu/%zu): preparing source image...",
              image.id, processedRenderItems,
              std::max<size_t>(1, totalRenderItems)));
      if (!EnsureCachedImageSource(image, cache)) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      wxImage scaled = cache.sourceImage.Scale(
          renderSize.GetWidth(), renderSize.GetHeight(), wxIMAGE_QUALITY_HIGH);
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format(
              "Rendering image id=%d: scaling and preparing RGBA texture...",
              image.id));
      if (!scaled.IsOk()) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      scaled = scaled.Mirror(false);
      if (!scaled.HasAlpha())
        scaled.InitAlpha();
      const int width = scaled.GetWidth();
      const int height = scaled.GetHeight();
      const unsigned char *rgb = scaled.GetData();
      const unsigned char *alpha = scaled.GetAlpha();
      if (!rgb || width <= 0 || height <= 0) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }

      if (!TryAllocatePixelBuffer(imagePixels, width, height, "image")) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      for (int i = 0; i < width * height; ++i) {
        imagePixels[static_cast<size_t>(i) * 4] = rgb[i * 3];
        imagePixels[static_cast<size_t>(i) * 4 + 1] = rgb[i * 3 + 1];
        imagePixels[static_cast<size_t>(i) * 4 + 2] = rgb[i * 3 + 2];
        imagePixels[static_cast<size_t>(i) * 4 + 3] = alpha ? alpha[i] : 255;
      }

      if (!ensureGlReady()) {
        clearLoadingState();
        NotifyRenderReady();
        return false;
      }
      if (cache.texture == 0) {
        glGenTextures(1, &cache.texture);
      }
      glBindTexture(GL_TEXTURE_2D, cache.texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      ScopedActivePixelUnpackPbo scopedPbo(cache.pixelUnpackPbo,
                                           cache.pboBytes);
      if (!UploadRgbaToTexture(cache.texture, width, height, imagePixels.data(),
                               cache.textureSize, true)) {
        ClearCachedTexture(cache);
        cache.textureSize = wxSize(0, 0);
        cache.renderZoom = 0.0;
        continue;
      }
      gui::layoutstatus::PostLayoutRenderStatus(
          this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
          wxString::Format(
              "Rendering image id=%d: GPU texture upload completed.",
              image.id));
      cache.textureSize = wxSize(width, height);
      cache.renderZoom = renderZoom;
      cache.contentHash = dataHash;
      profiler.RecordRenderedElement();
      imagePixels.clear();
      const bool hasMoreWork = NeedsRenderRebuild();
      profiler.Finish(hasMoreWork ? "incremental_pending" : "completed");
      return hasMoreWork;
    }

    clearLoadingState();
    NotifyRenderReady();
    profiler.Finish("completed");
    return false;
  } catch (const std::exception &ex) {
    loadingRequested = false;
    isLoading = false;
    Logger::Instance().Log(
        std::string("LayoutViewerPanel::RebuildCachedTexture exception: ") +
        ex.what());
    NotifyRenderReady();
    return false;
  } catch (...) {
    loadingRequested = false;
    isLoading = false;
    Logger::Instance().Log(
        "LayoutViewerPanel::RebuildCachedTexture unknown exception.");
    NotifyRenderReady();
    return false;
  }
}

// Releases all cached layout element textures and clears cache maps.
void LayoutViewerPanel::ClearCachedTexture() {
  for (auto &entry : viewCaches_) {
    ClearCachedTexture(entry.second);
  }
  viewCaches_.clear();
  for (auto &entry : legendCaches_) {
    ClearCachedTexture(entry.second);
  }
  legendCaches_.clear();
  for (auto &entry : eventTableCaches_) {
    ClearCachedTexture(entry.second);
  }
  eventTableCaches_.clear();
  for (auto &entry : textCaches_) {
    ClearCachedTexture(entry.second);
  }
  textCaches_.clear();
  for (auto &entry : imageCaches_) {
    ClearCachedTexture(entry.second);
  }
  imageCaches_.clear();
}

// Releases cached 2D view GL resources when the layout panel is screen-mapped.
void LayoutViewerPanel::ClearCachedTexture(ViewCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShownOnScreen()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!gl_lifecycle::TrySetCurrent(*this, glContext_, "LayoutViewerPanel",
                                   "ClearCachedTexture<ViewCache>"))
    return;
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

// Releases cached legend GL resources when the layout panel is screen-mapped.
void LayoutViewerPanel::ClearCachedTexture(LegendCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShownOnScreen()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!gl_lifecycle::TrySetCurrent(*this, glContext_, "LayoutViewerPanel",
                                   "ClearCachedTexture<LegendCache>"))
    return;
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

// Releases cached event-table GL resources when the layout panel is
// screen-mapped.
void LayoutViewerPanel::ClearCachedTexture(EventTableCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShownOnScreen()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!gl_lifecycle::TrySetCurrent(*this, glContext_, "LayoutViewerPanel",
                                   "ClearCachedTexture<EventTableCache>"))
    return;
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

// Releases cached text GL resources when the layout panel is screen-mapped.
void LayoutViewerPanel::ClearCachedTexture(TextCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShownOnScreen()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!gl_lifecycle::TrySetCurrent(*this, glContext_, "LayoutViewerPanel",
                                   "ClearCachedTexture<TextCache>"))
    return;
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

// Releases cached image GL resources when the layout panel is screen-mapped.
void LayoutViewerPanel::ClearCachedTexture(ImageCache &cache) {
  if (cache.texture == 0 && cache.pixelUnpackPbo == 0)
    return;
  if (!glContext_) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!IsShownOnScreen()) {
    cache.texture = 0;
    cache.pixelUnpackPbo = 0;
    cache.pboBytes = 0;
    return;
  }
  if (!gl_lifecycle::TrySetCurrent(*this, glContext_, "LayoutViewerPanel",
                                   "ClearCachedTexture<ImageCache>"))
    return;
  if (cache.texture != 0) {
    glDeleteTextures(1, &cache.texture);
    cache.texture = 0;
  }
  if (cache.pixelUnpackPbo != 0) {
    glDeleteBuffers(1, &cache.pixelUnpackPbo);
    cache.pixelUnpackPbo = 0;
  }
  cache.pboBytes = 0;
}

// Reports whether any element-level render cache is marked dirty.
bool LayoutViewerPanel::HasDirtyRenderCaches() const {
  auto hasDirty = [](const auto &map) {
    for (const auto &entry : map) {
      if (entry.second.renderDirty)
        return true;
    }
    return false;
  };
  return hasDirty(viewCaches_) || hasDirty(legendCaches_) ||
         hasDirty(eventTableCaches_) || hasDirty(textCaches_) ||
         hasDirty(imageCaches_);
}

// Reports whether global or element-level layout rendering work is pending.
bool LayoutViewerPanel::NeedsRenderRebuild() const {
  return renderDirty || HasDirtyRenderCaches();
}

// Reports whether a dirty element should stay hidden until its final texture is
// ready.
bool LayoutViewerPanel::ShouldDeferMissingElementTexture(
    bool cacheRenderDirty, unsigned int texture, const wxSize &textureSize,
    const wxSize &renderSize) const {
  if (!cacheRenderDirty || !(renderPending || loadingRequested || isLoading))
    return false;
  if (renderSize.GetWidth() <= 0 || renderSize.GetHeight() <= 0)
    return false;
  return texture == 0 || textureSize != renderSize;
}

// Debounces render rebuild requests and coalesces repeated zoom/layout changes.
void LayoutViewerPanel::RequestRenderRebuild() {
  if (auto *mw = MainWindow::Instance();
      mw && mw->IsMvrImportPipelineActive()) {
    renderPending = false;
    loadingRequested = false;
    isLoading = false;
    loadingTimer_.Stop();
    renderDelayTimer_.Stop();
    return;
  }
  if (IsLayoutEmpty()) {
    renderPending = false;
    loadingRequested = false;
    isLoading = false;
    loadingTimer_.Stop();
    renderDelayTimer_.Stop();
    return;
  }
  if (!isReadyToRender_ || !glContext_ || !IsShownOnScreen())
    return;
  if (!NeedsRenderRebuild())
    return;

  const bool alreadyPending = renderPending || loadingRequested;
  renderPending = true;
  loadingRequested = true;
  if (alreadyPending)
    return;

  renderQueuedAt_ = std::chrono::steady_clock::now();
  const int configuredDelayMs =
      initialRenderScheduled_ ? kZoomRenderDebounceMs : 1;
  initialRenderScheduled_ = true;
  if (startupMetrics_)
    startupMetrics_->layoutRenderConfiguredDebounceMs = configuredDelayMs;

  gui::layoutstatus::PostLayoutRenderStatus(
      this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
      "Layout render queued...");
  if (renderDelayTimer_.IsRunning())
    renderDelayTimer_.Stop();
  renderDelayTimer_.StartOnce(configuredDelayMs);
}

// Activates the loading overlay when a delayed rebuild is still pending.
void LayoutViewerPanel::OnLoadingTimer(wxTimerEvent &) {
  if (auto *mw = MainWindow::Instance();
      mw && mw->IsMvrImportPipelineActive()) {
    return;
  }
  if (!loadingRequested)
    return;
  if (!renderPending && !NeedsRenderRebuild())
    return;
  gui::layoutstatus::PostLayoutRenderStatus(
      this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
      "Rendering layout content...");
  isLoading = true;
  Refresh();
}

// Processes one incremental render rebuild step and schedules the next step if
// needed.
void LayoutViewerPanel::ProcessDeferredRenderRebuild() {
  if (auto *mw = MainWindow::Instance();
      mw && mw->IsMvrImportPipelineActive()) {
    renderPending = false;
    loadingRequested = false;
    isLoading = false;
    loadingTimer_.Stop();
    renderDelayTimer_.Stop();
    return;
  }
  if (!renderPending || !loadingRequested)
    return;
  if (!NeedsRenderRebuild()) {
    renderPending = false;
    loadingRequested = false;
    isLoading = false;
    loadingTimer_.Stop();
    gui::layoutstatus::PostLayoutRenderStatus(
        this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
        "Layout render completed.");
    Refresh();
    return;
  }

  gui::layoutstatus::PostLayoutRenderStatus(
      this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
      "Building layout textures...");
  const auto rebuildStartedAt = std::chrono::steady_clock::now();
  if (renderQueuedAt_ && startupMetrics_) {
    const long long queueWaitMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(rebuildStartedAt -
                                                              *renderQueuedAt_)
            .count();
    startupMetrics_->layoutRenderQueueWaitMs = queueWaitMs;
    startupMetrics_->layoutRenderEventLoopOverrunMs = std::max(
        0LL, queueWaitMs - startupMetrics_->layoutRenderConfiguredDebounceMs);
  }
  renderQueuedAt_.reset();
  if (!isLoading && !loadingTimer_.IsRunning())
    loadingTimer_.StartOnce(kLoadingOverlayDelayMs);
  const bool hasMoreWork = RebuildCachedTexture();
  if (startupMetrics_)
    startupMetrics_->layoutRenderRebuildMs +=
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - rebuildStartedAt)
            .count();
  Refresh();
  if (hasMoreWork && NeedsRenderRebuild()) {
    if (renderDelayTimer_.IsRunning())
      renderDelayTimer_.Stop();
    renderDelayTimer_.StartOnce(kIncrementalRenderDelayMs);
    return;
  }

  renderPending = false;
  loadingRequested = false;
  isLoading = false;
  loadingTimer_.Stop();
  gui::layoutstatus::PostLayoutRenderStatus(
      this, wxTheApp ? wxTheApp->GetTopWindow() : nullptr,
      "Layout render completed.");
  Refresh();
}

// Handles debounce and incremental render timer ticks for stale layout
// textures.
void LayoutViewerPanel::OnRenderDelayTimer(wxTimerEvent &) {
  InvalidateRenderIfFrameChanged(false);
  if (renderPending && loadingRequested) {
    ProcessDeferredRenderRebuild();
    return;
  }
  RequestRenderRebuild();
}

// Checks whether all layout elements currently have uploaded textures.
bool LayoutViewerPanel::AreTexturesReady() const {
  auto hasTexture = [](const auto &map, int id) {
    auto it = map.find(id);
    return it != map.end() && it->second.texture != 0;
  };

  for (const auto &view : currentLayout.view2dViews) {
    if (!hasTexture(viewCaches_, view.id))
      return false;
  }
  for (const auto &legend : currentLayout.legendViews) {
    if (!hasTexture(legendCaches_, legend.id))
      return false;
  }
  for (const auto &table : currentLayout.eventTables) {
    if (!hasTexture(eventTableCaches_, table.id))
      return false;
  }
  for (const auto &text : currentLayout.textViews) {
    if (!hasTexture(textCaches_, text.id))
      return false;
  }
  for (const auto &image : currentLayout.imageViews) {
    if (!hasTexture(imageCaches_, image.id))
      return false;
  }
  return true;
}

// Resolves a plain element identity to its current layout frame.
bool LayoutViewerPanel::GetElementFrame(
    gui::layoutselection::LayoutElementRef element,
    layouts::Layout2DViewFrame &frame) const {
  switch (element.kind) {
  case LayoutElementKind::View2D:
    return GetViewFrameById(element.id, frame);
  case LayoutElementKind::Legend:
    return GetLegendFrameById(element.id, frame);
  case LayoutElementKind::EventTable:
    return GetEventTableFrameById(element.id, frame);
  case LayoutElementKind::Text:
    return GetTextFrameById(element.id, frame);
  case LayoutElementKind::Image:
    return GetImageFrameById(element.id, frame);
  case LayoutElementKind::None:
    return false;
  }
  return false;
}

// Selects the topmost layout element whose displayed frame contains a point.
bool LayoutViewerPanel::SelectElementAtPosition(const wxPoint &pos) {
  EnsureSelectionIndexCache();
  const auto &elements = selectionIndexCache_.zOrderedElements;
  for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
    layouts::Layout2DViewFrame frame;
    if (!GetElementFrame(it->element, frame))
      continue;
    wxRect frameRect;
    if (!GetFrameRect(frame, frameRect) || !frameRect.Contains(pos))
      continue;
    if (selectionState_.Matches(it->element))
      return true;

    selectionState_.Select(it->element.kind, it->element.id);
    if (it->element.kind == LayoutElementKind::View2D)
      EmitViewSelectionChanged(it->element.id);
    RefreshAfterSelectionOnlyUpdate();
    return true;
  }
  return false;
}

// Maps a frame interaction mode to its wx cursor presentation.
wxCursor LayoutViewerPanel::CursorForMode(FrameDragMode mode) const {
  switch (mode) {
  case FrameDragMode::ResizeRight:
    return wxCursor(wxCURSOR_SIZEWE);
  case FrameDragMode::ResizeBottom:
    return wxCursor(wxCURSOR_SIZENS);
  case FrameDragMode::ResizeCorner:
    return wxCursor(wxCURSOR_SIZENWSE);
  case FrameDragMode::Move:
    return wxCursor(wxCURSOR_SIZING);
  case FrameDragMode::None:
  default:
    return wxCursor(wxCURSOR_ARROW);
  }
}

void LayoutViewerPanel::EmitEditViewRequest() {
  wxCommandEvent event(EVT_LAYOUT_VIEW_EDIT);
  event.SetEventObject(this);
  ProcessWindowEvent(event);
}

void LayoutViewerPanel::EmitViewSelectionChanged(int viewId) {
  if (viewId <= 0)
    return;
  wxCommandEvent event(EVT_LAYOUT_VIEW_SELECTED);
  event.SetEventObject(this);
  event.SetInt(viewId);
  ProcessWindowEvent(event);
}

LayoutViewerPanel::ViewCache &LayoutViewerPanel::GetViewCache(int viewId) {
  auto [it, inserted] = viewCaches_.try_emplace(viewId, ViewCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::LegendCache &
LayoutViewerPanel::GetLegendCache(int legendId) {
  auto [it, inserted] = legendCaches_.try_emplace(legendId, LegendCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::EventTableCache &
LayoutViewerPanel::GetEventTableCache(int tableId) {
  auto [it, inserted] =
      eventTableCaches_.try_emplace(tableId, EventTableCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::TextCache &LayoutViewerPanel::GetTextCache(int textId) {
  auto [it, inserted] = textCaches_.try_emplace(textId, TextCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}

LayoutViewerPanel::ImageCache &LayoutViewerPanel::GetImageCache(int imageId) {
  auto [it, inserted] = imageCaches_.try_emplace(imageId, ImageCache{});
  if (inserted) {
    renderDirty = true;
  }
  return it->second;
}
