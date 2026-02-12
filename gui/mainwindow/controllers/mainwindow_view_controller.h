#pragma once

class MainWindow;
class wxCommandEvent;

class MainWindowViewController {
public:
  explicit MainWindowViewController(MainWindow &owner) : owner_(owner) {}
  void OnToggleConsole(wxCommandEvent &event);
  void OnToggleFixtures(wxCommandEvent &event);
  void OnToggleViewport(wxCommandEvent &event);
  void OnToggleViewport2D(wxCommandEvent &event);
  void OnToggleRender2D(wxCommandEvent &event);
  void OnToggleLayers(wxCommandEvent &event);
  void OnToggleLayouts(wxCommandEvent &event);
  void OnToggleSummary(wxCommandEvent &event);
  void OnToggleRigging(wxCommandEvent &event);

private:
  MainWindow &owner_;
};
