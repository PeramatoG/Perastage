#pragma once

#include <string>
#include <unordered_set>

class ConfigManager;
class wxWindow;

namespace HoistLoadRecalculationPrompt {

bool PromptAndApply(ConfigManager &cfg, wxWindow *parent,
                    const std::unordered_set<std::string> &positionNames,
                    bool reloadHoistTable = true);

} // namespace HoistLoadRecalculationPrompt
