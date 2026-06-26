#include "mvr_xchange_mdns_service.h"
#include <wx/log.h>
#include <wx/string.h>

// Starts the isolated mDNS advertisement placeholder for the official MVR-xchange service name.
bool MvrXchangeMdnsService::Start(const MvrXchangeSettings &settings, int port) {
  running_ = true;
  lastError_.clear();
  wxLogMessage("MVR-xchange mDNS advertisement requested for _mvrxchange._tcp on port %d (%s).", port, wxString::FromUTF8(settings.stationName));
  return true;
}

// Stops the mDNS advertisement backend.
void MvrXchangeMdnsService::Stop() { running_ = false; }

// Returns whether the advertisement backend is currently active.
bool MvrXchangeMdnsService::IsRunning() const { return running_; }

// Returns the latest advertisement backend error text.
std::string MvrXchangeMdnsService::LastError() const { return lastError_; }
