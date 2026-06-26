#include "mvr_xchange_settings.h"
#include <random>
#include <sstream>
#include <wx/config.h>
#include <wx/stdpaths.h>
#include <wx/string.h>

// Generates a random UUID string for MVR-xchange station and file identities.
std::string GenerateMvrXchangeUuid() {
  static constexpr char hex[] = "0123456789abcdef";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(0, 15);
  std::string uuid(36, '0');
  for (int i = 0; i < 36; ++i) uuid[i] = hex[dist(gen)];
  uuid[8] = uuid[13] = uuid[18] = uuid[23] = '-';
  uuid[14] = '4';
  uuid[19] = hex[(dist(gen) & 0x3) | 0x8];
  return uuid;
}

// Loads persistent MVR-xchange settings from the application configuration store.
MvrXchangeSettings LoadMvrXchangeSettings() {
  MvrXchangeSettings settings;
  wxConfig config("Perastage");
  wxString stationName;
  if (config.Read("/MvrXchange/StationName", &stationName)) settings.stationName = stationName.ToStdString();
  wxString groupName;
  if (config.Read("/MvrXchange/GroupName", &groupName)) settings.groupName = groupName.ToStdString();
  wxString stationUuid;
  if (config.Read("/MvrXchange/StationUUID", &stationUuid)) settings.stationUuid = stationUuid.ToStdString();
  long port = 0;
  if (config.Read("/MvrXchange/Port", &port)) settings.port = static_cast<int>(port);
  if (settings.stationUuid.empty()) settings.stationUuid = GenerateMvrXchangeUuid();
  return settings;
}

// Saves persistent MVR-xchange settings to the application configuration store.
void SaveMvrXchangeSettings(const MvrXchangeSettings &settings) {
  wxConfig config("Perastage");
  config.Write("/MvrXchange/StationName", wxString::FromUTF8(settings.stationName));
  config.Write("/MvrXchange/GroupName", wxString::FromUTF8(settings.groupName));
  config.Write("/MvrXchange/StationUUID", wxString::FromUTF8(settings.stationUuid));
  config.Write("/MvrXchange/Port", static_cast<long>(settings.port));
  config.Flush();
}
