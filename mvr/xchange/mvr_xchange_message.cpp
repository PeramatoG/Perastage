#include "mvr_xchange_message.h"
#include "../../core/uuidutils.h"
#include "json.hpp"
#include <limits>
#include <set>

namespace mvr::xchange {
namespace {
constexpr int kMvrVersionMajor = 1;
constexpr int kMvrVersionMinor = 6;
constexpr const char *kProvider = "Perastage";

// Accepts canonical MVR versions and the specification-defined new-member marker.
bool IsSupportedCommitVersion(int major, int minor) {
  return (major == 1 && minor >= 0 && minor <= kMvrVersionMinor) || (major == 0 && minor == 0);
}

// Converts one local commit to the official MVR_COMMIT JSON object shape.
nlohmann::json CommitToJson(const MvrXchangeCommit &commit) {
  nlohmann::json json;
  json["Type"] = "MVR_COMMIT";
  json["verMajor"] = kMvrVersionMajor;
  json["verMinor"] = kMvrVersionMinor;
  json["FileSize"] = commit.declaredFileSizeSpecified ? commit.declaredFileSize : commit.FileSize();
  json["FileUUID"] = CanonicalizeUuid(commit.fileUuid);
  json["StationUUID"] = CanonicalizeUuid(commit.stationUuid);
  json["ForStationsUUID"] = commit.forStationsUuid;
  if (!commit.comment.empty()) json["Comment"] = commit.comment;
  if (!commit.fileName.empty()) json["FileName"] = commit.fileName;
  return json;
}

// Returns a string field from a JSON object or an empty string when absent.
std::string JsonStringValue(const nlohmann::json &json, const char *key) {
  const auto it = json.find(key);
  return it != json.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

// Converts one JSON commit object into a lightweight commit metadata record.
MvrXchangeCommit CommitFromJson(const nlohmann::json &json) {
  MvrXchangeCommit commit;
  commit.fileUuid = CanonicalizeUuid(JsonStringValue(json, "FileUUID"));
  commit.stationUuid = CanonicalizeUuid(JsonStringValue(json, "StationUUID"));
  commit.fileName = JsonStringValue(json, "FileName");
  commit.comment = JsonStringValue(json, "Comment");
  const auto major = json.find("verMajor");
  const auto minor = json.find("verMinor");
  if (major != json.end() && major->is_number_unsigned()) { const auto value = major->get<std::uint64_t>(); if (value <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) commit.verMajor = static_cast<int>(value); else commit.metadataValid = false; }
  else if (major != json.end() && major->is_number_integer()) { const auto value = major->get<std::int64_t>(); if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) commit.verMajor = static_cast<int>(value); else commit.metadataValid = false; }
  else commit.metadataValid = false;
  if (minor != json.end() && minor->is_number_unsigned()) { const auto value = minor->get<std::uint64_t>(); if (value <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) commit.verMinor = static_cast<int>(value); else commit.metadataValid = false; }
  else if (minor != json.end() && minor->is_number_integer()) { const auto value = minor->get<std::int64_t>(); if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) commit.verMinor = static_cast<int>(value); else commit.metadataValid = false; }
  else commit.metadataValid = false;
  const auto fileSize = json.find("FileSize");
  if (fileSize != json.end() && fileSize->is_number_unsigned()) {
    commit.declaredFileSize = fileSize->get<std::uint64_t>();
    commit.declaredFileSizeSpecified = true;
  } else commit.metadataValid = false;
  if (const auto stations = json.find("ForStationsUUID"); stations != json.end() && stations->is_array()) {
    for (const auto &value : *stations) commit.forStationsUuid.push_back(value.is_string() ? CanonicalizeUuid(value.get<std::string>()) : std::string{});
  } else if (stations != json.end()) commit.metadataValid = false;
  return commit;
}

// Appends parsed commit metadata from a JSON array.
void AppendCommitsFromJsonArray(const nlohmann::json &array, std::vector<MvrXchangeCommit> &commits) {
  if (!array.is_array()) return;
  std::set<std::string> identities;
  for (const auto &entry : array) {
    if (!entry.is_object()) { MvrXchangeCommit invalid; invalid.metadataValid = false; commits.push_back(std::move(invalid)); continue; }
    auto commit = CommitFromJson(entry);
    const std::string identity = commit.fileUuid + "\n" + commit.stationUuid;
    if (identities.insert(identity).second) commits.push_back(std::move(commit));
  }
}

// Returns an integer field from a JSON object or zero when absent.
int JsonIntValue(const nlohmann::json &json, const char *key) {
  const auto it = json.find(key);
  if (it == json.end()) return 0;
  if (it->is_number_unsigned()) { const auto value = it->get<std::uint64_t>(); return value <= static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ? static_cast<int>(value) : 0; }
  if (!it->is_number_integer()) return 0;
  const auto value = it->get<std::int64_t>();
  return value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max() ? static_cast<int>(value) : 0;
}

// Returns a boolean field from a JSON object or false when absent.
bool JsonBoolValue(const nlohmann::json &json, const char *key) {
  const auto it = json.find(key);
  return it != json.end() && it->is_boolean() ? it->get<bool>() : false;
}

}

// Parses a JSON message body into the common fields used by the TCP server.
std::optional<Message> ParseMessage(const std::string &jsonText) {
  const nlohmann::json json = nlohmann::json::parse(jsonText, nullptr, false);
  if (json.is_discarded() || !json.is_object()) return std::nullopt;
  Message msg;
  const auto setStructuralError = [&](const std::string &error) { if (msg.structuralError.empty()) msg.structuralError = error; };
  msg.type = JsonStringValue(json, "Type");
  if (msg.type.empty()) msg.type = JsonStringValue(json, "MessageType");
  if (msg.type.empty()) return std::nullopt;
  const auto fileUuidIt = json.find("FileUUID");
  if (fileUuidIt != json.end() && !fileUuidIt->is_string()) setStructuralError("FileUUID must be a string.");
  msg.fileUuidSpecified = fileUuidIt != json.end() && !JsonStringValue(json, "FileUUID").empty();
  msg.fileUuid = CanonicalizeUuid(JsonStringValue(json, "FileUUID"));
  const auto stationUuid = JsonStringValue(json, "StationUUID");
  const auto fromStationUuid = JsonStringValue(json, "FromStationUUID");
  if (const auto it = json.find("StationUUID"); it != json.end() && !it->is_string()) setStructuralError("StationUUID must be a string.");
  if (const auto it = json.find("FromStationUUID"); it != json.end() && !it->is_string()) setStructuralError("FromStationUUID must be a string.");
  msg.stationUuidSpecified = !stationUuid.empty();
  msg.fromStationUuidSpecified = !fromStationUuid.empty();
  msg.stationUuid = CanonicalizeUuid(stationUuid);
  msg.fromStationUuid = CanonicalizeUuid(fromStationUuid);
  msg.stationName = JsonStringValue(json, "StationName");
  msg.groupName = JsonStringValue(json, "GroupName");
  msg.provider = JsonStringValue(json, "Provider");
  msg.text = JsonStringValue(json, "Message");
  msg.comment = JsonStringValue(json, "Comment");
  for (const char *field : {"StationName", "Provider", "Message", "Comment"})
    if (const auto it = json.find(field); it != json.end() && !it->is_string()) setStructuralError(std::string(field) + " must be a string.");
  for (const char *field : {"verMajor", "verMinor"})
    if (const auto it = json.find(field); it != json.end() && !it->is_number_integer()) setStructuralError(std::string(field) + " must be an integer.");
  msg.verMajor = JsonIntValue(json, "verMajor");
  msg.verMinor = JsonIntValue(json, "verMinor");
  msg.ok = JsonBoolValue(json, "OK");
  msg.okSpecified = json.find("OK") != json.end() && json["OK"].is_boolean();
  if (json.find("OK") != json.end() && !json["OK"].is_boolean()) setStructuralError("OK must be a boolean.");
  if (const auto commitsIt = json.find("Commits"); commitsIt != json.end()) {
    if (!commitsIt->is_array()) { setStructuralError("Commits must be an array."); msg.inventoryPresence = InventoryPresence::PresentEmpty; }
    else {
      AppendCommitsFromJsonArray(*commitsIt, msg.commits);
      msg.inventoryPresence = msg.commits.empty() ? InventoryPresence::PresentEmpty : InventoryPresence::PresentNonEmpty;
    }
  }
  if (const auto filesIt = json.find("Files"); filesIt != json.end() && filesIt->is_array()) {
    msg.filesCount = filesIt->size();
    if (msg.inventoryPresence == InventoryPresence::Absent) {
      AppendCommitsFromJsonArray(*filesIt, msg.commits);
      msg.inventoryPresence = msg.commits.empty() ? InventoryPresence::PresentEmpty : InventoryPresence::PresentNonEmpty;
    }
  }
  else if (json.find("Files") != json.end() && msg.inventoryPresence == InventoryPresence::Absent) setStructuralError("Files must be an array.");
  if (msg.type == "MVR_COMMIT") msg.commits = {CommitFromJson(json)};
  return msg;
}

// Validates required fields for official MVR-xchange message types handled by Perastage.
std::string ValidateMessage(const Message &message) {
  if (!message.structuralError.empty()) return message.structuralError;
  if (message.type == "MVR_JOIN" || message.type == "MVR_JOIN_RET") {
    if (message.stationUuid.empty()) return message.type + " is missing a valid StationUUID.";
    if (message.stationName.empty()) return message.type + " is missing StationName.";
    if (message.provider.empty()) return message.type + " is missing Provider.";
    if (message.verMajor != 1 || message.verMinor < 0 || message.verMinor > 6) return message.type + " contains an unsupported protocol version.";
    for (const auto &commit : message.commits) {
      if (!commit.metadataValid) return message.type + " contains malformed commit metadata.";
      if (commit.fileUuid.empty()) return message.type + " contains a commit with an invalid FileUUID.";
      if (commit.stationUuid.empty()) return message.type + " contains a commit with an invalid StationUUID.";
      if (!commit.declaredFileSizeSpecified) return message.type + " contains a commit without FileSize.";
      if (!IsSupportedCommitVersion(commit.verMajor, commit.verMinor)) return message.type + " contains a commit with an unsupported protocol version.";
      for (const auto &uuid : commit.forStationsUuid) if (uuid.empty()) return message.type + " contains a commit with an invalid ForStationsUUID value.";
    }
    return {};
  }
  if (message.type == "MVR_LEAVE") {
    if (message.fromStationUuidSpecified && message.fromStationUuid.empty()) return "MVR_LEAVE contains an invalid FromStationUUID.";
    if (!message.fromStationUuidSpecified && message.stationUuidSpecified && message.stationUuid.empty()) return "MVR_LEAVE contains an invalid legacy StationUUID.";
    if (message.fromStationUuid.empty() && message.stationUuid.empty()) return "MVR_LEAVE is missing a valid FromStationUUID.";
    return {};
  }
  if (message.type == "MVR_COMMIT") {
    if (message.fileUuid.empty()) return "MVR_COMMIT is missing a valid FileUUID.";
    if (message.stationUuid.empty()) return "MVR_COMMIT is missing a valid StationUUID.";
    if (!IsSupportedCommitVersion(message.verMajor, message.verMinor)) return "MVR_COMMIT contains an unsupported protocol version.";
    if (message.commits.empty() || !message.commits.front().declaredFileSizeSpecified) return "MVR_COMMIT is missing FileSize.";
    for (const auto &uuid : message.commits.front().forStationsUuid) if (uuid.empty()) return "MVR_COMMIT contains an invalid ForStationsUUID value.";
    return {};
  }
  if (message.type == "MVR_REQUEST") {
    if (message.fileUuidSpecified && message.fileUuid.empty()) return "MVR_REQUEST contains an invalid FileUUID.";
    if (message.fromStationUuidSpecified && message.fromStationUuid.empty()) return "MVR_REQUEST contains an invalid FromStationUUID.";
    return {};
  }
  if (message.type == "MVR_REQUEST_RET" || message.type == "MVR_COMMIT_RET" || message.type == "MVR_LEAVE_RET") {
    if (!message.okSpecified) return message.type + " is missing OK.";
    if (!message.ok && message.text.empty()) return message.type + " is missing an error Message.";
    return {};
  }
  return "Unsupported MVR-xchange message type.";
}

// Builds a JOIN-style message with local station identity and commit metadata.
std::string BuildJoinMessage(const char *type, const std::string &stationUuid, const std::string &stationName, const std::vector<MvrXchangeCommit> &commits) {
  nlohmann::json json;
  json["Type"] = type;
  if (std::string(type) == "MVR_JOIN_RET") {
    json["OK"] = true;
    json["Message"] = "";
  }
  json["Provider"] = kProvider;
  json["StationName"] = stationName;
  json["verMajor"] = kMvrVersionMajor;
  json["verMinor"] = kMvrVersionMinor;
  json["StationUUID"] = CanonicalizeUuid(stationUuid);
  json["Commits"] = nlohmann::json::array();
  for (const auto &commit : commits) json["Commits"].push_back(CommitToJson(commit));
  return json.dump();
}

// Builds the official MVR_JOIN request with local commit metadata.
std::string BuildJoin(const std::string &stationUuid, const std::string &stationName, const std::vector<MvrXchangeCommit> &commits) {
  return BuildJoinMessage("MVR_JOIN", stationUuid, stationName, commits);
}

// Builds the official MVR_JOIN_RET response with local commit metadata.
std::string BuildJoinRet(const std::string &stationUuid, const std::string &stationName, const std::vector<MvrXchangeCommit> &commits) {
  return BuildJoinMessage("MVR_JOIN_RET", stationUuid, stationName, commits);
}

// Builds a typed MVR_JOIN_RET error response.
std::string BuildJoinRet(const std::string &stationUuid, const std::string &stationName, bool ok, const std::string &message) {
  auto json = nlohmann::json::parse(BuildJoinMessage("MVR_JOIN_RET", stationUuid, stationName, {}));
  json["OK"] = ok;
  json["Message"] = message;
  return json.dump();
}

// Builds the official MVR_LEAVE request for the local station.
std::string BuildLeave(const std::string &stationUuid) {
  return nlohmann::json{{"Type", "MVR_LEAVE"}, {"FromStationUUID", CanonicalizeUuid(stationUuid)}}.dump();
}

// Builds the official MVR_LEAVE_RET acknowledgement.
std::string BuildLeaveRet(bool ok, const std::string &message) {
  return nlohmann::json{{"Type", "MVR_LEAVE_RET"}, {"OK", ok}, {"Message", message}}.dump();
}

// Builds an official MVR_COMMIT announcement for a local published revision.
std::string BuildCommit(const MvrXchangeCommit &commit) { return CommitToJson(commit).dump(); }

// Builds the official MVR_COMMIT_RET acknowledgement.
std::string BuildCommitRet(bool ok, const std::string &message) {
  return nlohmann::json{{"Type", "MVR_COMMIT_RET"}, {"OK", ok}, {"Message", message}}.dump();
}

// Builds the official MVR_REQUEST message for one advertised file.
std::string BuildRequest(const std::string &fileUuid, const std::string &fromStationUuid) {
  return nlohmann::json{{"Type", "MVR_REQUEST"}, {"FileUUID", CanonicalizeUuid(fileUuid)}, {"FromStationUUID", CanonicalizeUuid(fromStationUuid)}}.dump();
}

// Builds the official MVR_REQUEST_RET error response.
std::string BuildRequestError(const std::string &message) {
  return nlohmann::json{{"Type", "MVR_REQUEST_RET"}, {"OK", false}, {"Message", message}}.dump();
}

// Builds the RET error corresponding to a recognizable request type.
std::string BuildTypedErrorResponse(const std::string &requestType, const std::string &stationUuid, const std::string &stationName, const std::string &message) {
  if (requestType == "MVR_JOIN") return BuildJoinRet(stationUuid, stationName, false, message);
  if (requestType == "MVR_COMMIT") return BuildCommitRet(false, message);
  if (requestType == "MVR_LEAVE") return BuildLeaveRet(false, message);
  return BuildRequestError(message);
}

}
