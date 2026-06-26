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
  if (!tcpServer_.Start(settings_, [this](const std::string &fileUuid) { return ResolveRequest(fileUuid); }, [this] { return GetLocalCommits(); }, [this](const std::string &msg) { Log(msg); }, [this](const MvrXchangeRemoteStation &station) { HandleIncomingJoin(station); })) {
    Log("MVR-xchange service failed to start.");
    return false;
  }
  if (!mdnsService_.Start(settings_, tcpServer_.Port())) {
    Log("MVR-xchange mDNS advertisement failed: " + mdnsService_.LastError());
    tcpServer_.Stop();
    return false;
  }
  Log("MVR-xchange mDNS backend: " + mdnsService_.BackendName());
  Log("MVR-xchange service type: " + mdnsService_.ServiceType());
  Log("MVR-xchange group service: " + mdnsService_.GroupServiceName());
  Log("MVR-xchange station UUID: " + settings_.stationUuid);
  Log("MVR-xchange TCP listen: 0.0.0.0:" + std::to_string(tcpServer_.Port()));
  Log("MVR-xchange selected interface: " + mdnsService_.SelectedInterfaceDescription());
  Log("MVR-xchange advertised A record: " + mdnsService_.AdvertisedIpAddress());
  {
    std::lock_guard lock(mutex_);
    stationRegistry_.SetLocalIdentity(settings_.stationUuid, mdnsService_.ServiceInstanceName(), tcpServer_.Port());
  }
  Log("MVR-xchange station discovery will accept incoming joins and track remote station state.");
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
  SendCommitToJoinedStations(commit);
  return true;
}

// Returns a snapshot of locally published MVR commits.
std::vector<MvrXchangeCommit> MvrXchangeService::GetLocalCommits() const {
  std::lock_guard lock(mutex_);
  return commits_.List();
}

// Returns a snapshot of known remote MVR-xchange stations.
std::vector<MvrXchangeRemoteStation> MvrXchangeService::GetKnownStations() const {
  std::lock_guard lock(mutex_);
  return stationRegistry_.List();
}

// Installs a log callback for GUI-safe status forwarding.
void MvrXchangeService::SetLogCallback(LogCallback callback) { logCallback_ = std::move(callback); }

// Resolves a request by FileUUID, treating an empty FileUUID as the latest commit.
std::optional<MvrXchangeCommit> MvrXchangeService::ResolveRequest(const std::string &fileUuid) const {
  std::lock_guard lock(mutex_);
  if (fileUuid.empty() || fileUuid == "latest" || fileUuid == "LATEST") return commits_.Latest();
  return commits_.FindByFileUuid(fileUuid);
}

// Tracks an incoming MVR_JOIN in the remote station registry.
void MvrXchangeService::HandleIncomingJoin(const MvrXchangeRemoteStation &station) {
  std::size_t discovered = 0;
  std::size_t incoming = 0;
  std::size_t outgoing = 0;
  {
    std::lock_guard lock(mutex_);
    stationRegistry_.UpsertIncomingJoin(station);
    for (const auto &known : stationRegistry_.List()) {
      if (known.discovered) ++discovered;
      if (known.incomingJoined) ++incoming;
      if (known.outgoingJoined) ++outgoing;
    }
  }
  Log("MVR-xchange remote stations: discovered=" + std::to_string(discovered) + ", incoming joined=" + std::to_string(incoming) + ", outgoing joined=" + std::to_string(outgoing) + ".");
  if (station.port > 0) TryOutgoingJoin(station);
}

// Sends an outgoing MVR_JOIN to a remote station with a resolved service endpoint.
void MvrXchangeService::TryOutgoingJoin(const MvrXchangeRemoteStation &station) {
  if (station.ipAddress.empty() || station.port <= 0) return;
  MvrXchangeRemoteStation joinedStation;
  const auto commits = GetLocalCommits();
  if (!tcpClient_.SendJoin(station, settings_, commits, joinedStation, [this](const std::string &msg) { Log(msg); })) return;
  std::lock_guard lock(mutex_);
  stationRegistry_.UpsertDiscovered(joinedStation);
  stationRegistry_.MarkOutgoingJoined(joinedStation.stationUuid, joinedStation.ipAddress, joinedStation.port);
}

// Sends an MVR_COMMIT announcement to all joined remote stations with known endpoints.
void MvrXchangeService::SendCommitToJoinedStations(const MvrXchangeCommit &commit) {
  const auto joined = [&] { std::lock_guard lock(mutex_); return stationRegistry_.JoinedStations(); }();
  int sent = 0;
  int failed = 0;
  for (const auto &station : joined) {
    if (station.ipAddress.empty() || station.port <= 0) continue;
    if (tcpClient_.SendCommit(station, commit, [this](const std::string &msg) { Log(msg); })) ++sent;
    else ++failed;
  }
  if (joined.empty()) Log("MVR-xchange did not send MVR_COMMIT because there are no joined remote stations.");
  else Log("MVR-xchange sent MVR_COMMIT to " + std::to_string(sent) + " joined station(s), " + std::to_string(failed) + " failed.");
}

// Writes a service message to wx logging and the optional callback.
void MvrXchangeService::Log(const std::string &message) const {
  wxLogMessage("%s", wxString::FromUTF8(message));
  if (logCallback_) logCallback_(message);
}
