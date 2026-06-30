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
  std::string provider;
  std::string text;
  std::string comment;
  int verMajor = 0;
  int verMinor = 0;
  bool ok = false;
  bool fileUuidSpecified = false;
  bool stationUuidSpecified = false;
  std::size_t filesCount = 0;
  std::vector<MvrXchangeCommit> commits;
};

std::optional<Message> ParseMessage(const std::string &json);
std::string ValidateMessage(const Message &message);
std::string BuildJoin(const std::string &stationUuid, const std::string &stationName, const std::vector<MvrXchangeCommit> &commits);
std::string BuildJoinRet(const std::string &stationUuid, const std::string &stationName, const std::vector<MvrXchangeCommit> &commits);
std::string BuildLeaveRet();
std::string BuildCommit(const MvrXchangeCommit &commit);
std::string BuildCommitRet(bool ok, const std::string &message = {});
std::string BuildRequest(const std::string &fileUuid, const std::string &fromStationUuid = {});
std::string BuildRequestError(const std::string &message);

}
