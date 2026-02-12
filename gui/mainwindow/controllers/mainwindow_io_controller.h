#pragma once

class MainWindow;
class wxCommandEvent;

class MainWindowIoController {
public:
  explicit MainWindowIoController(MainWindow &owner) : owner_(owner) {}
  void OnImportMVR(wxCommandEvent &event);

private:
  MainWindow &owner_;
};
