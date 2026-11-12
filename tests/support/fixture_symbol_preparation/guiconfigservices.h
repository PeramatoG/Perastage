#pragma once
#include "configmanager.h"

struct TestGuiConfigServices {
  ConfigManager config;
  ConfigManager &LegacyConfigManager() { return config; }
};
TestGuiConfigServices &GetDefaultGuiConfigServices();
