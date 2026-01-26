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

#include <GL/glew.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <wx/glcanvas.h>
#include <wx/timer.h>
#include <wx/wx.h>
#include "layouts/LayoutCollection.h"
#include "canvas2d.h"
#include "symbolcache.h"
#include "viewer2dpanel.h"
#include "viewer2dstate.h"

wxDECLARE_EVENT(EVT_LAYOUT_VIEW_EDIT, wxCommandEvent);

class Viewer2DOffscreenRenderer;

class LayoutViewerPanel : public wxGLCanvas {
public:
  explicit LayoutViewerPanel(wxWindow *parent);
  ~LayoutViewerPanel();

  void SetLayoutDefinition(const layouts::LayoutDefinition &layout);
  layouts::Layout2DViewDefinition *GetEditableView();
  const layouts::Layout2DViewDefinition *GetEditableView() const;
  void RefreshLegendData();

private:
  struct LegendItem {
    std::string typeName;
    int count = 0;
    std::optional<int> channelCount;
    std::string symbolKey;
  };

  struct ViewCache {
    int captureVersion = -1;
    bool captureInProgress = false;
    bool hasCapture = false;
    CommandBuffer buffer;
    Viewer2DViewState viewState;
    viewer2d::Viewer2DState renderState;
    bool hasRenderState = false;
    std::shared_ptr<const SymbolDefinitionSnapshot> symbols;
    unsigned int texture = 0;
    wxSize textureSize{0, 0};
    double renderZoom = 0.0;
    bool renderDirty = true;
  };

  struct LegendCache {
    unsigned int texture = 0;
    wxSize textureSize{0, 0};
    double renderZoom = 0.0;
    bool renderDirty = true;
    size_t contentHash = 0;
    std::shared_ptr<const SymbolDefinitionSnapshot> symbols;
  };

  struct EventTableCache {
    unsigned int texture = 0;
    wxSize textureSize{0, 0};
    double renderZoom = 0.0;
    bool renderDirty = true;
    size_t contentHash = 0;
  };

  struct TextCache {
    unsigned int texture = 0;
    wxSize textureSize{0, 0};
    double renderZoom = 0.0;
    bool renderDirty = true;
    size_t contentHash = 0;
  };

  struct ImageCache {
    unsigned int texture = 0;
    wxSize textureSize{0, 0};
    double renderZoom = 0.0;
    bool renderDirty = true;
    size_t contentHash = 0;
  };

  void OnPaint(wxPaintEvent &event);
  void OnSize(wxSizeEvent &event);
  void OnLeftDown(wxMouseEvent &event);
  void OnLeftUp(wxMouseEvent &event);
  void OnLeftDClick(wxMouseEvent &event);
  void OnMouseMove(wxMouseEvent &event);
  void OnMouseWheel(wxMouseEvent &event);
  void OnCaptureLost(wxMouseCaptureLostEvent &event);
  void OnRightUp(wxMouseEvent &event);
  void OnKeyDown(wxKeyEvent &event);
  void OnEditView(wxCommandEvent &event);
  void OnDeleteView(wxCommandEvent &event);
  void OnDeleteLegend(wxCommandEvent &event);
  void OnEditEventTable(wxCommandEvent &event);
  void OnDeleteEventTable(wxCommandEvent &event);
  void OnEditText(wxCommandEvent &event);
  void OnDeleteText(wxCommandEvent &event);
  void OnEditImage(wxCommandEvent &event);
  void OnDeleteImage(wxCommandEvent &event);
  void OnBringToFront(wxCommandEvent &event);
  void OnSendToBack(wxCommandEvent &event);
  void OnRenderDebounceTimer(wxTimerEvent &event);

  void DrawSelectionHandles(const wxRect &frameRect) const;
  void DrawViewElement(const layouts::Layout2DViewDefinition &view,
                       Viewer2DPanel *capturePanel,
                       Viewer2DOffscreenRenderer *offscreenRenderer,
                       int activeViewId);
  void DrawLegendElement(const layouts::LayoutLegendDefinition &legend,
                         int activeLegendId);
  void DrawEventTableElement(const layouts::LayoutEventTableDefinition &table);
  void DrawTextElement(const layouts::LayoutTextDefinition &text,
                       int activeTextId);
  void DrawImageElement(const layouts::LayoutImageDefinition &image,
                        int activeImageId);

  void ResetViewToFit();
  wxRect GetPageRect() const;
  bool GetFrameRect(const layouts::Layout2DViewFrame &frame,
                    wxRect &rect) const;
  wxSize GetFrameSizeForZoom(const layouts::Layout2DViewFrame &frame,
                             double targetZoom) const;
  double GetRenderZoom() const;
  void UpdateFrame(const layouts::Layout2DViewFrame &frame,
                   bool updatePosition);
  void UpdateLegendFrame(const layouts::Layout2DViewFrame &frame,
                         bool updatePosition);
  void UpdateEventTableFrame(const layouts::Layout2DViewFrame &frame,
                             bool updatePosition);
  void UpdateTextFrame(const layouts::Layout2DViewFrame &frame,
                       bool updatePosition);
  void UpdateImageFrame(const layouts::Layout2DViewFrame &frame,
                        bool updatePosition);
  void InitGL();
  void RebuildCachedTexture();
  bool BlitTextureToCache(unsigned int sourceTexture, const wxSize &sourceSize,
                          ViewCache &cache);
  void ClearCachedTexture();
  void ClearCachedTexture(ViewCache &cache);
  void ClearCachedTexture(LegendCache &cache);
  void ClearCachedTexture(EventTableCache &cache);
  void ClearCachedTexture(TextCache &cache);
  void ClearCachedTexture(ImageCache &cache);
  void RequestRenderRebuild();
  bool HasDirtyCaches() const;
  void InvalidateRenderIfFrameChanged();
  void EmitEditViewRequest();
  bool SelectElementAtPosition(const wxPoint &pos);
  bool GetLegendFrameById(int legendId,
                          layouts::Layout2DViewFrame &frame) const;
  bool GetEventTableFrameById(int tableId,
                              layouts::Layout2DViewFrame &frame) const;
  bool GetTextFrameById(int textId, layouts::Layout2DViewFrame &frame) const;
  bool GetImageFrameById(int imageId, layouts::Layout2DViewFrame &frame) const;
  const layouts::LayoutLegendDefinition *GetSelectedLegend() const;
  layouts::LayoutLegendDefinition *GetSelectedLegend();
  const layouts::LayoutEventTableDefinition *GetSelectedEventTable() const;
  layouts::LayoutEventTableDefinition *GetSelectedEventTable();
  const layouts::LayoutTextDefinition *GetSelectedText() const;
  layouts::LayoutTextDefinition *GetSelectedText();
  const layouts::LayoutImageDefinition *GetSelectedImage() const;
  layouts::LayoutImageDefinition *GetSelectedImage();
  bool GetSelectedFrame(layouts::Layout2DViewFrame &frame) const;
  ViewCache &GetViewCache(int viewId);
  LegendCache &GetLegendCache(int legendId);
  EventTableCache &GetEventTableCache(int tableId);
  TextCache &GetTextCache(int textId);
  ImageCache &GetImageCache(int imageId);
  std::vector<LegendItem> BuildLegendItems() const;
  size_t HashLegendItems(const std::vector<LegendItem> &items) const;
  wxImage BuildLegendImage(const wxSize &size, const wxSize &logicalSize,
                           double renderZoom,
                           const std::vector<LegendItem> &items,
                           const SymbolDefinitionSnapshot *symbols) const;
  size_t HashEventTableFields(
      const layouts::LayoutEventTableDefinition &table) const;
  wxImage BuildEventTableImage(
      const wxSize &size, const wxSize &logicalSize, double renderZoom,
      const layouts::LayoutEventTableDefinition &table) const;
  size_t HashTextContent(const layouts::LayoutTextDefinition &text) const;
  size_t HashImageContent(const layouts::LayoutImageDefinition &image) const;
  wxImage BuildTextImage(const wxSize &size, const wxSize &logicalSize,
                         double renderZoom,
                         const layouts::LayoutTextDefinition &text) const;

  enum class FrameDragMode {
    None,
    Move,
    ResizeRight,
    ResizeBottom,
    ResizeCorner
  };

  FrameDragMode HitTestFrame(const wxPoint &pos, const wxRect &frameRect) const;
  wxCursor CursorForMode(FrameDragMode mode) const;

  enum class SelectedElementType {
    None,
    View2D,
    Legend,
    EventTable,
    Text,
    Image
  };

  struct ZOrderedElement {
    SelectedElementType type = SelectedElementType::None;
    int id = -1;
    int zIndex = 0;
    size_t order = 0;
  };

  std::vector<ZOrderedElement> BuildZOrderedElements() const;
  std::pair<int, int> GetZIndexRange() const;

  static constexpr double kLegendContentScale = 0.7;
  static constexpr double kLegendFontScale =
      (2.0 / 3.0) * kLegendContentScale;

  layouts::LayoutDefinition currentLayout;
  double zoom = 1.0;
  wxPoint panOffset{0, 0};
  bool isPanning = false;
  wxPoint lastMousePos{0, 0};
  FrameDragMode dragMode = FrameDragMode::None;
  FrameDragMode hoverMode = FrameDragMode::None;
  wxPoint dragStartPos{0, 0};
  layouts::Layout2DViewFrame dragStartFrame;
  int layoutVersion = 0;
  bool captureInProgress = false;
  SelectedElementType selectedElementType = SelectedElementType::None;
  int selectedElementId = -1;
  wxGLContext *glContext_ = nullptr;
  bool glInitialized_ = false;
  unsigned int textureCopyFbo_ = 0;
  bool renderDirty = true;
  bool renderPending = false;
  wxTimer renderDebounceTimer_;
  std::unordered_map<int, ViewCache> viewCaches_;
  std::unordered_map<int, LegendCache> legendCaches_;
  std::unordered_map<int, EventTableCache> eventTableCaches_;
  std::unordered_map<int, TextCache> textCaches_;
  std::unordered_map<int, ImageCache> imageCaches_;
  std::vector<LegendItem> legendItems_;
  size_t legendDataHash = 0;
  bool pendingFitOnResize = true;

  wxDECLARE_EVENT_TABLE();
};
