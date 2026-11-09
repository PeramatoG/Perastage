/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include "patch/dmx_patch_snapshot.h"
#include "patch/dmx_patch_grid_geometry.h"
#include <wx/scrolwin.h>
class DmxPatchCanvas : public wxScrolledWindow {
public:
  explicit DmxPatchCanvas(wxWindow *parent);
  void SetSnapshot(std::shared_ptr<const dmx_patch::Snapshot> snapshot);
  void SetColumns(int columns);
  std::optional<dmx_patch::Address> AddressAt(const wxPoint &clientPosition) const;
private:
  void Paint(wxPaintEvent &);
  void Hover(wxMouseEvent &);
  void UpdateMetrics();
  wxString Tooltip(int channel) const;
  dmx_patch_ui::GridGeometry geometry_;
  std::shared_ptr<const dmx_patch::Snapshot> snapshot_;
  std::optional<int> hovered_;
};
