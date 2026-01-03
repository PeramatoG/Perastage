#pragma once

#include <wx/dialog.h>

class Viewer2DPanel;
class Viewer2DRenderPanel;
class LayerPanel;
class wxSlider;
class wxStaticText;

class Layout2DViewDialog : public wxDialog {
public:
  explicit Layout2DViewDialog(wxWindow *parent);

  Viewer2DPanel *GetViewerPanel() const { return viewerPanel; }
  Viewer2DRenderPanel *GetRenderPanel() const { return renderPanel; }

private:
  void OnOk(wxCommandEvent &event);
  void OnCancel(wxCommandEvent &event);
  void OnShow(wxShowEvent &event);
  void OnScaleChanged(wxCommandEvent &event);
  void UpdateScaleLabel();

  Viewer2DPanel *viewerPanel = nullptr;
  Viewer2DRenderPanel *renderPanel = nullptr;
  LayerPanel *layerPanel = nullptr;
  wxSlider *scaleSlider = nullptr;
  wxStaticText *scaleValueLabel = nullptr;
};
