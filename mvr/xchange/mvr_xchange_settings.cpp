#include "mvr_xchange_settings.h"
#include "../../core/uuidutils.h"
#include <cctype>
#include <wx/config.h>
#include <wx/string.h>
#include <wx/utils.h>


namespace {
// Returns the local host name used in the default MVR-xchange station label.
std::string LocalHostDisplayName() {
  std::string host = wxGetHostName().ToStdString();
  const std::size_t dot = host.find('.');
  if (dot != std::string::npos) host.resize(dot);
  if (host.empty()) host = "localhost";
  return host;
}

// Builds the default MVR-xchange station name shown to remote clients.
std::string DefaultMvrXchangeStationName() {
  return "Perastage on " + LocalHostDisplayName();
}

// Normalizes a station label for conservative persisted-default repair checks.
std::string NormalizeStationLabel(const std::string &value) {
  std::string normalized;
  for (unsigned char ch : value) {
    if (std::isalnum(ch)) normalized.push_back(static_cast<char>(std::tolower(ch)));
  }
  return normalized;
}

// Removes the default Perastage prefix from a normalized station label.
std::string StripDefaultStationPrefix(std::string value) {
  static const std::string prefix = NormalizeStationLabel("Perastage on");
  if (value.rfind(prefix, 0) == 0) value.erase(0, prefix.size());
  return value;
}

// Checks whether a saved station name looks like a truncated generated default.
bool ShouldUseDefaultStationName(const std::string &stationName, const std::string &defaultStationName, const std::string &hostName) {
  if (stationName.empty() || stationName == "Perastage") return true;
  const std::string normalizedStation = NormalizeStationLabel(stationName);
  const std::string normalizedDefault = NormalizeStationLabel(defaultStationName);
  if (normalizedStation == normalizedDefault) return false;
  const std::string stationHostPart = StripDefaultStationPrefix(normalizedStation);
  const std::string normalizedHost = NormalizeStationLabel(hostName);
  if (stationHostPart.size() < 8 || normalizedHost.empty()) return false;
  return normalizedDefault.find(normalizedStation) != std::string::npos ||
         normalizedHost.find(stationHostPart) != std::string::npos;
}
}

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
  const std::string hostName = LocalHostDisplayName();
  const std::string defaultStationName = DefaultMvrXchangeStationName();
  if (ShouldUseDefaultStationName(settings.stationName, defaultStationName, hostName)) settings.stationName = defaultStationName;
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
