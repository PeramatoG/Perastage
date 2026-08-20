#include "mvr_xchange_service.h"
#include "../mvrexporter.h"
#include "../../core/uuidutils.h"
#include "../../core/logger.h"
#include "mvr_xchange_publication_policy.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <iterator>
#include <sstream>

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

// Checks that an exported MVR archive has the required scene description entry.
bool ContainsGeneralSceneDescription(const std::vector<uint8_t> &bytes) {
  static constexpr char kEntryName[] = "GeneralSceneDescription.xml";
  return std::search(bytes.begin(), bytes.end(), std::begin(kEntryName), std::end(kEntryName) - 1) != bytes.end();
}

// Converts a display name into a filesystem-friendly MVR base name.
std::string SanitizeMvrFileBaseName(std::string name) {
  for (char &ch : name) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (!std::isalnum(value) && ch != '-' && ch != '_' && ch != ' ') ch = '-';
  }
  while (!name.empty() && (name.front() == ' ' || name.front() == '-' || name.front() == '_')) name.erase(name.begin());
  while (!name.empty() && (name.back() == ' ' || name.back() == '-' || name.back() == '_')) name.pop_back();
  return name.empty() ? std::string("Perastage") : name;
}

// Builds the user-facing filename announced with an MVR-xchange commit.
std::string BuildCommitFileName(const std::string &displayName, const std::string &timestampUtc) {
  std::string compactTimestamp = timestampUtc;
  for (char &ch : compactTimestamp) {
    if (ch == ':' || ch == '-' || ch == 'T') ch = '_';
  }
  if (!compactTimestamp.empty() && compactTimestamp.back() == 'Z') compactTimestamp.pop_back();
  return SanitizeMvrFileBaseName(displayName) + "-" + compactTimestamp + ".mvr";
}

// Checks whether a station identity matches grandMA3's MVR-xchange endpoint behavior.
bool IsGrandMa3Station(const MvrXchangeRemoteStation &station) {
  std::string identity = station.provider + " " + station.stationName + " " + station.serviceInstanceName;
  std::transform(identity.begin(), identity.end(), identity.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return identity.find("grandma3") != std::string::npos || identity.find("gma3") != std::string::npos;
}

// Returns a reachable outgoing endpoint, including known compatibility fallbacks.
MvrXchangeRemoteStation ResolveOutgoingEndpoint(MvrXchangeRemoteStation station) {
  static constexpr int kGrandMa3MvrXchangePort = 42424;
  if (!station.ipAddress.empty() && station.port <= 0 && IsGrandMa3Station(station)) station.port = kGrandMa3MvrXchangePort;
  return station;
}
}

// Starts the TCP publisher and advertises it through the mDNS abstraction.
bool MvrXchangeService::Start(const MvrXchangeSettings &settings) {
  if (IsRunning()) return true;
  settings_ = settings;
  settings_.stationUuid = CanonicalizeUuid(settings_.stationUuid);
  if (!tcpServer_.Start(settings_, [this](const std::string &fileUuid) { return ResolveRequest(fileUuid); }, [this] { return GetLocalCommits(); }, [this](const std::string &msg) { Log(msg); }, [this](const MvrXchangeRemoteStation &station) { HandleIncomingJoin(station); }, [this](const std::string &stationUuid) { return HandleIncomingLeave(stationUuid); }, [this](const MvrXchangeCommit &commit) { return HandleIncomingCommit(commit); })) {
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
  Log("MVR-xchange instance: " + mdnsService_.ServiceInstanceName());
  Log("MVR-xchange station UUID: " + settings_.stationUuid);
  Log("MVR-xchange TCP listen: 0.0.0.0:" + std::to_string(tcpServer_.Port()));
  Log("MVR-xchange selected interface: " + mdnsService_.SelectedInterfaceDescription());
  Log("MVR-xchange advertised A record: " + mdnsService_.AdvertisedIpAddress());
  {
    std::lock_guard lock(mutex_);
    stationRegistry_.SetLocalIdentity(settings_.stationUuid, mdnsService_.ServiceInstanceName(), tcpServer_.Port());
  }
  if (!mdnsDiscovery_.Start(settings_, mdnsService_.ServiceInstanceName(), settings_.stationUuid,
                            [this](const std::string &msg) { Log(msg); })) {
    Log("MVR-xchange persistent discovery is unavailable.");
  }
  discoveryStopRequested_ = false;
  discoveryThread_ = std::thread(&MvrXchangeService::DiscoveryLoop, this);
  Log("MVR-xchange active station discovery started.");
  Log("MVR-xchange service started on TCP " + mdnsService_.AdvertisedIpAddress() + ":" + std::to_string(tcpServer_.Port()) + ".");
  return true;
}

// Stops the TCP publisher and the mDNS advertisement.
void MvrXchangeService::Stop() {
  discoveryStopRequested_ = true;
  if (discoveryThread_.joinable()) discoveryThread_.join();
  mdnsDiscovery_.Stop();
  mdnsService_.Stop();
  tcpServer_.Stop();
  Log("MVR-xchange service stopped.");
}

// Returns whether the publisher is currently running.
bool MvrXchangeService::IsRunning() const { return tcpServer_.IsRunning(); }

// Returns the active TCP listening port.
int MvrXchangeService::Port() const { return tcpServer_.Port(); }

// Returns the IP address advertised for incoming MVR-xchange connections.
std::string MvrXchangeService::AdvertisedIpAddress() const { return mdnsService_.AdvertisedIpAddress(); }

// Exports the current scene, stores it as a bounded commit, and announces it.
bool MvrXchangeService::PublishCurrentScene(const std::string &comment, const std::string &fileNameBase) {
  std::vector<uint8_t> bytes;
  MvrExporter exporter;
  if (!exporter.ExportToBuffer(bytes) || bytes.empty()) {
    Log("MVR-xchange publish failed because the current scene could not be exported.");
    return false;
  }
  if (!ContainsGeneralSceneDescription(bytes)) {
    Log("MVR-xchange publish failed because the exported MVR archive does not contain GeneralSceneDescription.xml.");
    return false;
  }
  MvrXchangeCommit commit;
  commit.fileUuid = GenerateMvrXchangeUuid();
  commit.stationUuid = CanonicalizeUuid(settings_.stationUuid);
  commit.comment = comment;
  commit.timestampUtc = CurrentUtcTimestamp();
  commit.fileName = BuildCommitFileName(fileNameBase.empty() ? settings_.stationName : fileNameBase, commit.timestampUtc);
  commit.payload = std::move(bytes);
  std::vector<MvrXchangeRemoteStation> previouslyJoined;
  {
    // Freeze membership and inventory atomically against background discovery JOINs.
    std::lock_guard discoveryLock(discoveryMutex_);
    std::lock_guard lock(mutex_);
    previouslyJoined = mvr::xchange::CapturePublicationDestinations(stationRegistry_);
    commits_.Add(commit);
  }
  ReconcileDiscoveredStations(false);
  SendCommitToJoinedStations(commit, previouslyJoined);
  Log("MVR-xchange published revision FileUUID=" + commit.fileUuid + ", bytes=" + std::to_string(commit.FileSize()) + ", created=" + commit.timestampUtc + ".");
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

// Runs an immediate active discovery pass when requested by the user.
void MvrXchangeService::DiscoverNow() {
  if (IsRunning()) ReconcileDiscoveredStations(true);
}


// Requests an advertised remote MVR commit payload by station and FileUUID.
std::optional<MvrXchangeCommit> MvrXchangeService::RequestRemoteCommit(const std::string &stationUuid, const std::string &fileUuid) {
  const std::string canonicalStationUuid = CanonicalizeUuid(stationUuid);
  const std::string canonicalFileUuid = CanonicalizeUuid(fileUuid);
  const auto stations = GetKnownStations();
  auto stationIt = std::find_if(stations.begin(), stations.end(), [&](const MvrXchangeRemoteStation &station) {
    return !canonicalStationUuid.empty() && station.stationUuid == canonicalStationUuid;
  });
  if (stationIt == stations.end()) {
    Log("MVR-xchange request failed because the remote station is no longer known.");
    return std::nullopt;
  }
  const auto endpoint = ResolveOutgoingEndpoint(*stationIt);
  if (endpoint.ipAddress.empty() || endpoint.port <= 0) {
    Log("MVR-xchange request failed because the remote station has no reachable endpoint.");
    return std::nullopt;
  }
  return tcpClient_.RequestCommit(endpoint, canonicalFileUuid, settings_.stationUuid, [this](const std::string &msg) { Log(msg); });
}

// Installs a log callback for GUI-safe status forwarding.
void MvrXchangeService::SetLogCallback(LogCallback callback) { logCallback_ = std::move(callback); }

// Resolves a request by FileUUID, treating an empty FileUUID as the latest commit.
std::optional<MvrXchangeCommit> MvrXchangeService::ResolveRequest(const std::string &fileUuid) const {
  std::lock_guard lock(mutex_);
  if (fileUuid.empty()) return commits_.Latest();
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
}

// Applies explicit leave state received from a remote station.
std::string MvrXchangeService::HandleIncomingLeave(const std::string &stationUuid) {
  std::lock_guard lock(mutex_);
  return stationRegistry_.MarkLeft(stationUuid) ? std::string{} : "MVR_LEAVE does not belong to a known station.";
}

// Validates targeting and records one incoming remote commit announcement.
std::string MvrXchangeService::HandleIncomingCommit(const MvrXchangeCommit &commit) {
  if (!commit.forStationsUuid.empty() && std::find(commit.forStationsUuid.begin(), commit.forStationsUuid.end(), settings_.stationUuid) == commit.forStationsUuid.end())
    return "MVR_COMMIT is not addressed to this station.";
  std::lock_guard lock(mutex_);
  if (!stationRegistry_.ApplyCommit(commit)) return "MVR_COMMIT does not belong to a joined station.";
  return {};
}

// Sends an outgoing MVR_JOIN to a remote station with a resolved service endpoint.
void MvrXchangeService::TryOutgoingJoin(const MvrXchangeRemoteStation &station) {
  {
    std::lock_guard lock(mutex_);
    if (!stationRegistry_.ShouldInitiateOutgoingJoin(station)) return;
  }
  const auto endpoint = ResolveOutgoingEndpoint(station);
  if (endpoint.ipAddress.empty() || endpoint.port <= 0) return;
  MvrXchangeRemoteStation joinedStation;
  const auto commits = GetLocalCommits();
  if (!tcpClient_.SendJoin(endpoint, settings_, commits, joinedStation, [this](const std::string &msg) { Log(msg); })) return;
  std::lock_guard lock(mutex_);
  stationRegistry_.UpsertOutgoingJoin(joinedStation);
}

// Sends an MVR_COMMIT announcement to all joined remote stations with known endpoints.
void MvrXchangeService::SendCommitToJoinedStations(const MvrXchangeCommit &commit, const std::vector<MvrXchangeRemoteStation> &destinations) {
  int sent = 0;
  int failed = 0;
  int skipped = 0;
  for (const auto &station : destinations) {
    const bool stillJoined = [&] { std::lock_guard lock(mutex_); return stationRegistry_.CanSendCommitTo(station.stationUuid); }();
    if (!stillJoined) { ++skipped; continue; }
    const auto endpoint = ResolveOutgoingEndpoint(station);
    if (endpoint.ipAddress.empty() || endpoint.port <= 0) { ++skipped; continue; }
    if (tcpClient_.SendCommit(endpoint, commit, [this](const std::string &msg) { Log(msg); })) {
      ++sent;
    } else {
      ++failed;
    }
  }
  if (destinations.empty()) {
    const auto stations = GetKnownStations();
    const auto discovered = std::count_if(stations.begin(), stations.end(), [](const auto &station) { return station.discovered; });
    if (discovered > 0) Log("MVR-xchange has " + std::to_string(discovered) + " discovered station(s) but 0 joined station(s); no MVR_COMMIT was sent.");
    else Log("MVR-xchange did not send MVR_COMMIT because there are no joined remote stations.");
  } else {
    Log("MVR-xchange sent MVR_COMMIT to " + std::to_string(sent) + " joined station(s), " + std::to_string(failed) + " failed, " + std::to_string(skipped) + " without advertised endpoints.");
  }
}

// Runs one active mDNS discovery pass and joins newly discovered stations.
void MvrXchangeService::ReconcileDiscoveredStations(bool requestQuery) {
  std::lock_guard discoveryLock(discoveryMutex_);
  if (requestQuery) mdnsDiscovery_.QueryNow();
  const auto stations = mdnsDiscovery_.Snapshot();
  {
    std::lock_guard lock(mutex_);
    stationRegistry_.ReconcileDiscovered(stations);
  }
  for (const auto &station : stations) {
    bool shouldJoin = false;
    {
      std::lock_guard lock(mutex_);
      shouldJoin = stationRegistry_.ShouldInitiateOutgoingJoin(station);
    }
    if (shouldJoin) TryOutgoingJoin(station);
  }
  if (!stations.empty()) LogStationCounts();
}

// Reconciles the persistent discovery cache with station state periodically.
void MvrXchangeService::DiscoveryLoop() {
  while (!discoveryStopRequested_) {
    ReconcileDiscoveredStations(false);
    for (int i = 0; i < 30 && !discoveryStopRequested_; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

// Logs current remote station state counters.
void MvrXchangeService::LogStationCounts() const {
  const auto stations = GetKnownStations();
  const auto discovered = std::count_if(stations.begin(), stations.end(), [](const auto &station) { return station.discovered; });
  const auto incoming = std::count_if(stations.begin(), stations.end(), [](const auto &station) { return station.incomingJoined; });
  const auto outgoing = std::count_if(stations.begin(), stations.end(), [](const auto &station) { return station.outgoingJoined; });
  Log("MVR-xchange remote stations: discovered=" + std::to_string(discovered) + ", incoming joined=" + std::to_string(incoming) + ", outgoing joined=" + std::to_string(outgoing) + ".");
}

// Writes a service message to the application log and optional callback.
void MvrXchangeService::Log(const std::string &message) const {
  Logger::Instance().Log(Logger::Level::Info, message);
  if (logCallback_) logCallback_(message);
}
