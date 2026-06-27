#pragma once
#include <string>

struct MvrXchangeSettings {
  std::string stationName = "Perastage";
  std::string groupName = "Default";
  std::string stationUuid;
  int port = 0;
  std::string selectedInterfaceId;
};

MvrXchangeSettings LoadMvrXchangeSettings();
void SaveMvrXchangeSettings(const MvrXchangeSettings &settings);
std::string GenerateMvrXchangeUuid();
