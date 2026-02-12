#pragma once

class MainWindow;
class wxCommandEvent;

class MainWindowPrintController {
public:
  explicit MainWindowPrintController(MainWindow &owner) : owner_(owner) {}
  void OnPrintLayout(wxCommandEvent &event);

private:
  MainWindow &owner_;
};
