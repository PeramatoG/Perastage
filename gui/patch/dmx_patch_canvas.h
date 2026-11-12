/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include "patch/dmx_patch_snapshot.h"
#include "patch/dmx_patch_grid_geometry.h"
#include "patch/dmx_patch_grid_blocks.h"
#include <wx/scrolwin.h>
#include <functional>
class DmxPatchCanvas : public wxScrolledWindow {
public:
  using PickHandler = std::function<void(const std::vector<std::string> &, bool, bool)>;
  explicit DmxPatchCanvas(wxWindow *parent);
  void SetPickHandler(PickHandler handler) { pick_ = std::move(handler); }
  std::vector<std::string> FixturesAt(const wxPoint &clientPosition) const;
  void SetSnapshot(std::shared_ptr<const dmx_patch::Snapshot> snapshot);
  void SetHighlights(std::map<std::string, wxColour> highlights);
  const std::map<std::string, wxColour> &Highlights() const { return highlights_; }
  wxSize CellSize() const { return wxSize(geometry_.width, geometry_.height); }
  int ColumnCount() const { return geometry_.columns; }
  std::optional<dmx_patch::Address> AddressAt(const wxPoint &clientPosition) const;
private:
  void Paint(wxPaintEvent &);
  void Hover(wxMouseEvent &);
  void UpdateMetrics();
  void PaintHighlights(wxDC &dc, int firstRow, int lastRow);
  wxString Tooltip(int channel) const;
  PickHandler pick_;
  std::map<std::string, wxColour> highlights_;
  dmx_patch_ui::GridGeometry geometry_;
  std::shared_ptr<const dmx_patch::Snapshot> snapshot_;
  std::optional<int> hovered_;
  std::optional<dmx_patch_ui::GridBlocks> blocks_;
};
