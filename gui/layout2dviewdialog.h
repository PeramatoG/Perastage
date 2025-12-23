#pragma once

#include <wx/dialog.h>

class Viewer2DPanel;
class Viewer2DRenderPanel;

class Layout2DViewDialog : public wxDialog {
public:
  explicit Layout2DViewDialog(wxWindow *parent);

  Viewer2DPanel *GetViewerPanel() const { return viewerPanel; }
  Viewer2DRenderPanel *GetRenderPanel() const { return renderPanel; }

private:
  void OnOk(wxCommandEvent &event);
  void OnCancel(wxCommandEvent &event);

  Viewer2DPanel *viewerPanel = nullptr;
  Viewer2DRenderPanel *renderPanel = nullptr;
};
