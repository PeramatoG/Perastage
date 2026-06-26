#include "mvr_xchange_message.h"
#include <sstream>

namespace mvr::xchange {

// Escapes a string value for a small JSON message body.
std::string EscapeJson(const std::string &value) {
  std::string out;
  for (char c : value) {
    if (c == '"' || c == '\\') out.push_back('\\');
    if (c == '\n') { out += "\\n"; continue; }
    out.push_back(c);
  }
  return out;
}

// Extracts a top-level JSON string value from simple MVR-xchange messages.
std::optional<std::string> ExtractJsonString(const std::string &json, const std::string &key) {
  const std::string needle = "\"" + key + "\"";
  auto pos = json.find(needle);
  if (pos == std::string::npos) return std::nullopt;
  pos = json.find(':', pos);
  if (pos == std::string::npos) return std::nullopt;
  pos = json.find('"', pos);
  if (pos == std::string::npos) return std::nullopt;
  std::string out;
  bool esc = false;
  for (++pos; pos < json.size(); ++pos) {
    char c = json[pos];
    if (esc) { out.push_back(c == 'n' ? '\n' : c); esc = false; continue; }
    if (c == '\\') { esc = true; continue; }
    if (c == '"') return out;
    out.push_back(c);
  }
  return std::nullopt;
}

// Parses the message type and common fields from a JSON message body.
Message ParseMessage(const std::string &json) {
  Message msg;
  msg.type = ExtractJsonString(json, "MessageType").value_or(ExtractJsonString(json, "Type").value_or(std::string{}));
  msg.fileUuid = ExtractJsonString(json, "FileUUID").value_or(std::string{});
  msg.stationUuid = ExtractJsonString(json, "StationUUID").value_or(std::string{});
  msg.stationName = ExtractJsonString(json, "StationName").value_or(std::string{});
  msg.groupName = ExtractJsonString(json, "GroupName").value_or(std::string{});
  msg.comment = ExtractJsonString(json, "Comment").value_or(std::string{});
  return msg;
}

// Builds the response sent after a compatible station joins the session.
std::string BuildJoinRet(const std::string &stationUuid, const std::string &stationName, const std::string &groupName) {
  return "{\"Type\":\"MVR_JOIN_RET\",\"OK\":true,\"StationUUID\":\"" + EscapeJson(stationUuid) + "\",\"StationName\":\"" + EscapeJson(stationName) + "\",\"GroupName\":\"" + EscapeJson(groupName) + "\"}";
}

// Builds the response sent after a station leaves the session.
std::string BuildLeaveRet(const std::string &stationUuid) {
  return "{\"Type\":\"MVR_LEAVE_RET\",\"OK\":true,\"StationUUID\":\"" + EscapeJson(stationUuid) + "\"}";
}

// Builds a commit announcement for the currently published MVR revision.
std::string BuildCommit(const MvrXchangeCommit &commit) {
  return "{\"Type\":\"MVR_COMMIT\",\"FileUUID\":\"" + EscapeJson(commit.fileUuid) + "\",\"StationUUID\":\"" + EscapeJson(commit.stationUuid) + "\",\"FileName\":\"" + EscapeJson(commit.fileName) + "\",\"FileSize\":" + std::to_string(commit.FileSize()) + ",\"Comment\":\"" + EscapeJson(commit.comment) + "\"}";
}

// Builds an acknowledgement for an incoming commit message.
std::string BuildCommitRet(const std::string &fileUuid, bool ok, const std::string &error) {
  return "{\"Type\":\"MVR_COMMIT_RET\",\"OK\":" + std::string(ok ? "true" : "false") + ",\"FileUUID\":\"" + EscapeJson(fileUuid) + "\",\"Error\":\"" + EscapeJson(error) + "\"}";
}

// Builds the metadata response for a successful MVR payload request.
std::string BuildRequestRet(const MvrXchangeCommit &commit) {
  return "{\"Type\":\"MVR_REQUEST_RET\",\"OK\":true,\"FileUUID\":\"" + EscapeJson(commit.fileUuid) + "\",\"FileName\":\"" + EscapeJson(commit.fileName) + "\",\"FileSize\":" + std::to_string(commit.FileSize()) + "}";
}

// Builds a JSON error response for protocol requests that cannot be fulfilled.
std::string BuildError(const std::string &type, const std::string &error) {
  return "{\"Type\":\"" + EscapeJson(type) + "\",\"OK\":false,\"Error\":\"" + EscapeJson(error) + "\"}";
}

}
