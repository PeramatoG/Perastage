#pragma once

#include <string>

class ConfigManager;
class wxWindow;

namespace rider_fixture_resolution_gui {

enum class PreflightResult { Proceed, Cancelled, Failed };

PreflightResult RunCreateFromTextPreflight(wxWindow *parent,
                                           ConfigManager &configManager,
                                           const std::string &text);

} // namespace rider_fixture_resolution_gui
