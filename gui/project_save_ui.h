/* This file is part of Perastage. */
#pragma once
#include "project_save_diagnostic.h"
#include <functional>
#include <string>
#include <wx/string.h>
class wxWindow;
namespace project_save_ui {
struct Result {
  bool saved = false;
  bool threw = false;
  std::string exceptionDetail;
};
// The busy window and disabler are destroyed before this function returns.
Result Run(wxWindow &owner, const std::function<bool()> &save);
wxString FailureMessage(ProjectSaveFailureKind kind);
} // namespace project_save_ui
