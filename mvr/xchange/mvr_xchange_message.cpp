#include "mvr_xchange_message.h"
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
  json["FileUUID"] = commit.fileUuid;
  json["StationUUID"] = commit.stationUuid;
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
}

// Parses a JSON message body into the common fields used by the TCP server.
std::optional<Message> ParseMessage(const std::string &jsonText) {
  const nlohmann::json json = nlohmann::json::parse(jsonText, nullptr, false);
  if (json.is_discarded() || !json.is_object()) return std::nullopt;
  Message msg;
  msg.type = JsonStringValue(json, "Type");
  if (msg.type.empty()) msg.type = JsonStringValue(json, "MessageType");
  if (msg.type.empty()) return std::nullopt;
  msg.fileUuid = JsonStringValue(json, "FileUUID");
  msg.stationUuid = JsonStringValue(json, "StationUUID");
  if (msg.stationUuid.empty()) msg.stationUuid = JsonStringValue(json, "FromStationUUID");
  msg.stationName = JsonStringValue(json, "StationName");
  msg.groupName = JsonStringValue(json, "GroupName");
  msg.comment = JsonStringValue(json, "Comment");
  return msg;
}

// Builds the official MVR_JOIN_RET response with local commit metadata.
std::string BuildJoinRet(const std::string &stationUuid, const std::string &stationName, const std::vector<MvrXchangeCommit> &commits) {
  nlohmann::json json;
  json["Type"] = "MVR_JOIN_RET";
  json["OK"] = true;
  json["Message"] = "";
  json["Provider"] = kProvider;
  json["StationName"] = stationName;
  json["verMajor"] = kMvrVersionMajor;
  json["verMinor"] = kMvrVersionMinor;
  json["StationUUID"] = stationUuid;
  json["Commits"] = nlohmann::json::array();
  for (const auto &commit : commits) json["Commits"].push_back(CommitToJson(commit));
  json["Files"] = json["Commits"];
  return json.dump();
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
