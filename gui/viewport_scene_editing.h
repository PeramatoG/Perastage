/* This file is part of Perastage. License: GPL-3.0-or-later. */
#pragma once
#include <string>
class ConfigManager;
class wxWindow;
namespace gui::viewport {
bool ApplyFixturePatch(ConfigManager &config, const std::string &uuid,
                       int fixtureId, const std::string &address,
                       std::string &error);
bool EditFixturePatch(wxWindow *parent, const std::string &uuid);
} // namespace gui::viewport
