#pragma once

#include <wx/dialog.h>
#include "LayoutCollection.h"

class Viewer2DPanel;
class Viewer2DRenderPanel;
class LayerPanel;
class SummaryPanel;
class ConfigManager;
class wxSlider;
class wxStaticText;

class Layout2DViewDialog : public wxDialog {
public:
  explicit Layout2DViewDialog(wxWindow *parent,
                              ConfigManager *visibilityConfig = nullptr,
                              ConfigManager *colorConfig = nullptr);

  void SetViewportDefinition(const layouts::Layout2DViewDefinition &view) { viewportDraft = view; }
  void ApplyViewportProperties(layouts::Layout2DViewDefinition &view) const;
  Viewer2DPanel *GetViewerPanel() const { return viewerPanel; }
  Viewer2DRenderPanel *GetRenderPanel() const { return renderPanel; }

private:
  void OnOk(wxCommandEvent &event);
  void OnViewportProperties(wxCommandEvent &event);
  void OnCancel(wxCommandEvent &event);
  void OnShow(wxShowEvent &event);
  void OnCharHook(wxKeyEvent &event);
  void OnScaleChanged(wxCommandEvent &event);
  void UpdateScaleLabel();
  void SyncPhysicalCenter();
  void UpdateEditorScaleMode();

  Viewer2DPanel *viewerPanel = nullptr;
  Viewer2DRenderPanel *renderPanel = nullptr;
  LayerPanel *layerPanel = nullptr;
  SummaryPanel *summaryPanel = nullptr;
  wxSlider *scaleSlider = nullptr;
  wxStaticText *scaleValueLabel = nullptr;
  wxStaticText *fixedScaleNotice = nullptr;
  ConfigManager *config = nullptr;
  std::optional<layouts::Layout2DViewCameraState> freeEditorCamera;
  float freeEditorOverlayScale = 1;
  std::optional<layouts::Layout2DViewDefinition> initialEditorView;
  bool initialShowSyncDone = false;
  std::optional<layouts::Layout2DViewDefinition> viewportDraft;
};
