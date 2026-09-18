#pragma once

#include <functional>
#include <string>

#include <wx/string.h>

class ConfigManager;
class wxWindow;

struct GdtfShareDownloadGuiCallbacks {
  std::function<void(const wxString &)> appendConsoleMessage;
  std::function<void(const std::string &)> addFixtureFromGdtfPath;
};

// Runs the complete GDTF Share catalog, search, and download GUI workflow.
void RunGdtfShareDownloadWorkflow(
    wxWindow *parent, ConfigManager &configManager,
    const GdtfShareDownloadGuiCallbacks &callbacks);
