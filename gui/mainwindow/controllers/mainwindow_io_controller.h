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
  static constexpr const char *kMvrOpenAction = "importing an MVR file";
  static constexpr const char *kMvrOpenTitle = "Import MVR";

  bool ImportMvrFromPath(const std::string &pathUtf8);
  bool ImportMvrWithOfficialPolicy(const std::string &pathUtf8);
  MainWindow &owner_;
};
