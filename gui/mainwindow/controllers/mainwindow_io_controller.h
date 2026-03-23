#pragma once

#include <string>

class MainWindow;
class wxCommandEvent;

class MainWindowIoController {
public:
  explicit MainWindowIoController(MainWindow &owner) : owner_(owner) {}
  void OnImportMVR(wxCommandEvent &event);
  bool OpenPathFromCommandLine(const std::string &pathUtf8);

private:
  bool ImportMvrFromPath(const std::string &pathUtf8);
  MainWindow &owner_;
};
