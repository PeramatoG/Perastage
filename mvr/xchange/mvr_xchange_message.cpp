#include "mvr_xchange_message.h"
#include "../../core/uuidutils.h"
#include "json.hpp"

namespace mvr::xchange {
namespace {
constexpr int kMvrVersionMajor = 1;
constexpr int kMvrVersionMinor = 6;
constexpr const char *kProvider = "Perastage";

// Converts one local commit to the official MVR_COMMIT JSON object shape.
nlohmann::json CommitToJson(const MvrXchangeCommit &commit) {
  nlohmann::json json;
  json["Type"] = "MVR_COMMIT";
  json["verMajor"] = kMvrVersionMajor;
  json["verMinor"] = kMvrVersionMinor;
  json["FileSize"] = commit.FileSize();
  json["FileUUID"] = CanonicalizeUuid(commit.fileUuid);
  json["StationUUID"] = CanonicalizeUuid(commit.stationUuid);
  json["ForStationsUUID"] = nlohmann::json::array();
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
  const auto fileSize = json.find("FileSize");
  if (fileSize != json.end() && fileSize->is_number_unsigned()) commit.payload.resize(fileSize->get<std::size_t>());
  return commit;
}

// Appends parsed commit metadata from a JSON array.
void AppendCommitsFromJsonArray(const nlohmann::json &array, std::vector<MvrXchangeCommit> &commits) {
  if (!array.is_array()) return;
  for (const auto &entry : array) {
    if (entry.is_object()) commits.push_back(CommitFromJson(entry));
  }
}

// Returns an integer field from a JSON object or zero when absent.
int JsonIntValue(const nlohmann::json &json, const char *key) {
  const auto it = json.find(key);
  return it != json.end() && it->is_number_integer() ? it->get<int>() : 0;
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
  msg.type = JsonStringValue(json, "Type");
  if (msg.type.empty()) msg.type = JsonStringValue(json, "MessageType");
  if (msg.type.empty()) return std::nullopt;
  const auto fileUuidIt = json.find("FileUUID");
  msg.fileUuidSpecified = fileUuidIt != json.end() && !JsonStringValue(json, "FileUUID").empty();
  msg.fileUuid = CanonicalizeUuid(JsonStringValue(json, "FileUUID"));
  const auto stationUuid = JsonStringValue(json, "StationUUID");
  const auto fromStationUuid = JsonStringValue(json, "FromStationUUID");
  msg.stationUuidSpecified = !stationUuid.empty() || !fromStationUuid.empty();
  msg.stationUuid = CanonicalizeUuid(stationUuid);
  if (msg.stationUuid.empty()) msg.stationUuid = CanonicalizeUuid(fromStationUuid);
  msg.stationName = JsonStringValue(json, "StationName");
  msg.groupName = JsonStringValue(json, "GroupName");
  msg.provider = JsonStringValue(json, "Provider");
  msg.text = JsonStringValue(json, "Message");
  msg.comment = JsonStringValue(json, "Comment");
  msg.verMajor = JsonIntValue(json, "verMajor");
  msg.verMinor = JsonIntValue(json, "verMinor");
  msg.ok = JsonBoolValue(json, "OK");
  if (const auto commitsIt = json.find("Commits"); commitsIt != json.end()) AppendCommitsFromJsonArray(*commitsIt, msg.commits);
  if (const auto filesIt = json.find("Files"); filesIt != json.end() && filesIt->is_array()) {
    msg.filesCount = filesIt->size();
    if (msg.commits.empty()) AppendCommitsFromJsonArray(*filesIt, msg.commits);
  }
  return msg;
}

// Validates required fields for official MVR-xchange message types handled by Perastage.
std::string ValidateMessage(const Message &message) {
  if (message.type == "MVR_JOIN" || message.type == "MVR_JOIN_RET") {
    if (message.stationUuid.empty()) return message.type + " is missing a valid StationUUID.";
    if (message.stationName.empty()) return message.type + " is missing StationName.";
    if (message.provider.empty()) return message.type + " is missing Provider.";
    for (const auto &commit : message.commits) {
      if (commit.fileUuid.empty()) return message.type + " contains a commit with an invalid FileUUID.";
      if (commit.stationUuid.empty()) return message.type + " contains a commit with an invalid StationUUID.";
    }
    return {};
  }
  if (message.type == "MVR_LEAVE") {
    if (message.stationUuid.empty()) return "MVR_LEAVE is missing a valid StationUUID.";
    return {};
  }
  if (message.type == "MVR_COMMIT") {
    if (message.fileUuid.empty()) return "MVR_COMMIT is missing a valid FileUUID.";
    if (message.stationUuid.empty()) return "MVR_COMMIT is missing a valid StationUUID.";
    return {};
  }
  if (message.type == "MVR_REQUEST") {
    if (message.fileUuidSpecified && message.fileUuid.empty()) return "MVR_REQUEST contains an invalid FileUUID.";
    return {};
  }
  if (message.type == "MVR_REQUEST_RET" || message.type == "MVR_COMMIT_RET" || message.type == "MVR_LEAVE_RET") return {};
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
  json["Files"] = json["Commits"];
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

// Builds the official MVR_LEAVE_RET acknowledgement.
std::string BuildLeaveRet() {
  return nlohmann::json{{"Type", "MVR_LEAVE_RET"}, {"OK", true}, {"Message", ""}}.dump();
}

// Builds an official MVR_COMMIT announcement for a local published revision.
std::string BuildCommit(const MvrXchangeCommit &commit) { return CommitToJson(commit).dump(); }

// Builds the official MVR_COMMIT_RET acknowledgement.
std::string BuildCommitRet(bool ok, const std::string &message) {
  return nlohmann::json{{"Type", "MVR_COMMIT_RET"}, {"OK", ok}, {"Message", message}}.dump();
}

// Builds the official MVR_REQUEST_RET error response.
std::string BuildRequestError(const std::string &message) {
  return nlohmann::json{{"Type", "MVR_REQUEST_RET"}, {"OK", false}, {"Message", message}}.dump();
}

}
