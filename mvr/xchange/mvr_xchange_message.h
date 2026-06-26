#pragma once
#include "mvr_xchange_commit.h"
#include <optional>
#include <string>

namespace mvr::xchange {

struct Message {
  std::string type;
  std::string fileUuid;
  std::string stationUuid;
  std::string stationName;
  std::string groupName;
  std::string comment;
};

std::string EscapeJson(const std::string &value);
std::optional<std::string> ExtractJsonString(const std::string &json, const std::string &key);
Message ParseMessage(const std::string &json);
std::string BuildJoinRet(const std::string &stationUuid, const std::string &stationName, const std::string &groupName);
std::string BuildLeaveRet(const std::string &stationUuid);
std::string BuildCommit(const MvrXchangeCommit &commit);
std::string BuildCommitRet(const std::string &fileUuid, bool ok, const std::string &error = {});
std::string BuildRequestRet(const MvrXchangeCommit &commit);
std::string BuildError(const std::string &type, const std::string &error);

}
