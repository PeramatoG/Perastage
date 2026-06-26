#pragma once
#include "mvr_xchange_commit.h"
#include <optional>
#include <string>
#include <vector>

namespace mvr::xchange {

struct Message {
  std::string type;
  std::string fileUuid;
  std::string stationUuid;
  std::string stationName;
  std::string groupName;
  std::string comment;
};

std::optional<Message> ParseMessage(const std::string &json);
std::string BuildJoinRet(const std::string &stationUuid, const std::string &stationName, const std::vector<MvrXchangeCommit> &commits);
std::string BuildLeaveRet();
std::string BuildCommit(const MvrXchangeCommit &commit);
std::string BuildCommitRet(bool ok, const std::string &message = {});
std::string BuildRequestError(const std::string &message);

}
