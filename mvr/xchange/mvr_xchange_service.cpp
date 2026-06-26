#include "mvr_xchange_service.h"
#include "../mvrexporter.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <wx/log.h>
#include <wx/string.h>

namespace {
// Formats the current UTC time for commit metadata.
std::string CurrentUtcTimestamp() {
  auto now = std::chrono::system_clock::now();
  std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}
}

// Starts the TCP publisher and advertises it through the mDNS abstraction.
bool MvrXchangeService::Start(const MvrXchangeSettings &settings) {
  if (IsRunning()) return true;
  settings_ = settings;
  if (!tcpServer_.Start(settings_, [this](const std::string &fileUuid) { return ResolveRequest(fileUuid); }, [this] { return GetLocalCommits(); }, [this](const std::string &msg) { Log(msg); })) {
    Log("MVR-xchange service failed to start.");
    return false;
  }
  if (!mdnsService_.Start(settings_, tcpServer_.Port())) {
    Log("MVR-xchange mDNS advertisement failed: " + mdnsService_.LastError());
    tcpServer_.Stop();
    return false;
  }
  Log("MVR-xchange service started on TCP port " + std::to_string(tcpServer_.Port()) + ".");
  return true;
}

// Stops the TCP publisher and the mDNS advertisement.
void MvrXchangeService::Stop() {
  mdnsService_.Stop();
  tcpServer_.Stop();
  Log("MVR-xchange service stopped.");
}

// Returns whether the publisher is currently running.
bool MvrXchangeService::IsRunning() const { return tcpServer_.IsRunning(); }

// Returns the active TCP listening port.
int MvrXchangeService::Port() const { return tcpServer_.Port(); }

// Exports the current scene, stores it as a bounded commit, and announces it.
bool MvrXchangeService::PublishCurrentScene(const std::string &comment) {
  std::vector<uint8_t> bytes;
  MvrExporter exporter;
  if (!exporter.ExportToBuffer(bytes) || bytes.empty()) {
    Log("MVR-xchange publish failed because the current scene could not be exported.");
    return false;
  }
  MvrXchangeCommit commit;
  commit.fileUuid = GenerateMvrXchangeUuid();
  commit.stationUuid = settings_.stationUuid;
  commit.fileName = "Perastage-" + commit.fileUuid + ".mvr";
  commit.comment = comment;
  commit.timestampUtc = CurrentUtcTimestamp();
  commit.payload = std::move(bytes);
  {
    std::lock_guard lock(mutex_);
    commits_.Add(commit);
  }
  tcpServer_.BroadcastCommit(commit);
  return true;
}

// Returns a snapshot of locally published MVR commits.
std::vector<MvrXchangeCommit> MvrXchangeService::GetLocalCommits() const {
  std::lock_guard lock(mutex_);
  return commits_.List();
}

// Installs a log callback for GUI-safe status forwarding.
void MvrXchangeService::SetLogCallback(LogCallback callback) { logCallback_ = std::move(callback); }

// Resolves a request by FileUUID, treating an empty FileUUID as the latest commit.
std::optional<MvrXchangeCommit> MvrXchangeService::ResolveRequest(const std::string &fileUuid) const {
  std::lock_guard lock(mutex_);
  if (fileUuid.empty() || fileUuid == "latest" || fileUuid == "LATEST") return commits_.Latest();
  return commits_.FindByFileUuid(fileUuid);
}

// Writes a service message to wx logging and the optional callback.
void MvrXchangeService::Log(const std::string &message) const {
  wxLogMessage("%s", wxString::FromUTF8(message));
  if (logCallback_) logCallback_(message);
}
