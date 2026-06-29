#include "mvr_xchange_settings.h"
#include "../../core/uuidutils.h"
#include <wx/config.h>
#include <wx/stdpaths.h>
#include <wx/string.h>

// Generates a canonical UUID string for MVR-xchange station and file identities.
std::string GenerateMvrXchangeUuid() { return CanonicalizeUuid(GenerateUuid()); }

// Loads persistent MVR-xchange settings from the application configuration store.
MvrXchangeSettings LoadMvrXchangeSettings() {
  MvrXchangeSettings settings;
  wxConfig config("Perastage");
  wxString stationName;
  if (config.Read("/MvrXchange/StationName", &stationName)) settings.stationName = stationName.ToStdString();
  wxString groupName;
  if (config.Read("/MvrXchange/GroupName", &groupName)) settings.groupName = groupName.ToStdString();
  wxString stationUuid;
  if (config.Read("/MvrXchange/StationUUID", &stationUuid)) settings.stationUuid = CanonicalizeUuid(stationUuid.ToStdString());
  long port = 0;
  if (config.Read("/MvrXchange/Port", &port)) settings.port = static_cast<int>(port);
  wxString selectedInterfaceId;
  if (config.Read("/MvrXchange/SelectedInterfaceId", &selectedInterfaceId)) settings.selectedInterfaceId = selectedInterfaceId.ToStdString();
  if (settings.stationUuid.empty()) settings.stationUuid = GenerateMvrXchangeUuid();
  return settings;
}

// Saves persistent MVR-xchange settings to the application configuration store.
void SaveMvrXchangeSettings(const MvrXchangeSettings &settings) {
  wxConfig config("Perastage");
  config.Write("/MvrXchange/StationName", wxString::FromUTF8(settings.stationName));
  config.Write("/MvrXchange/GroupName", wxString::FromUTF8(settings.groupName));
  config.Write("/MvrXchange/StationUUID", wxString::FromUTF8(CanonicalizeUuid(settings.stationUuid)));
  config.Write("/MvrXchange/Port", static_cast<long>(settings.port));
  config.Write("/MvrXchange/SelectedInterfaceId", wxString::FromUTF8(settings.selectedInterfaceId));
  config.Flush();
}
