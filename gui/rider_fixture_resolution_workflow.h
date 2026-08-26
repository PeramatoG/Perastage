#pragma once

#include "../core/riderimporter.h"

#include <string>

class ConfigManager;
class wxWindow;

namespace rider_fixture_resolution_gui {

enum class PreflightResult { Proceed, Cancelled, Failed };

PreflightResult RunCreateFromTextPreflight(wxWindow *parent,
                                           ConfigManager &configManager,
                                           const std::string &text,
                                           std::string *filteredTextOut = nullptr,
                                           RiderImporter::ImportPlan *importPlanOut = nullptr);

} // namespace rider_fixture_resolution_gui
