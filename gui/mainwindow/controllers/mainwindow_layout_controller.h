#pragma once

class MainWindow;
class wxCommandEvent;

class MainWindowLayoutController {
public:
  explicit MainWindowLayoutController(MainWindow &owner) : owner_(owner) {}
  void OnLayoutViewEdit(wxCommandEvent &event);
  void OnLayoutAdd2DView(wxCommandEvent &event);
  void OnLayoutAddLegend(wxCommandEvent &event);
  void OnLayoutAddEventTable(wxCommandEvent &event);
  void OnLayoutAddText(wxCommandEvent &event);
  void OnLayoutAddImage(wxCommandEvent &event);
  void OnLayout2DViewOk(wxCommandEvent &event);
  void OnLayout2DViewCancel(wxCommandEvent &event);
  void OnLayoutSelected(wxCommandEvent &event);
  void OnLayoutRenderReady(wxCommandEvent &event);

private:
  MainWindow &owner_;
};
