#pragma once
#include "mvr_xchange_commit.h"
#include <optional>
#include <string>
#include <vector>

namespace mvr::xchange {

enum class InventoryPresence { Absent, PresentEmpty, PresentNonEmpty };

struct Message {
  std::string type;
  std::string fileUuid;
  std::string stationUuid;
  std::string fromStationUuid;
  std::vector<std::string> fromStationUuids;
  std::string stationName;
  std::string groupName;
  std::string provider;
  std::string text;
  std::string comment;
  int verMajor = 0;
  int verMinor = 0;
  bool ok = false;
  bool okSpecified = false;
  bool fileUuidSpecified = false;
  bool stationUuidSpecified = false;
  bool fromStationUuidSpecified = false;
  bool fromStationUuidsSpecified = false;
  std::string structuralError;
  std::size_t filesCount = 0;
  InventoryPresence inventoryPresence = InventoryPresence::Absent;
  std::vector<MvrXchangeCommit> commits;
};

std::optional<Message> ParseMessage(const std::string &json);
std::string ValidateMessage(const Message &message);
std::string BuildJoin(const std::string &stationUuid, const std::string &stationName, const std::vector<MvrXchangeCommit> &commits);
std::string BuildJoinRet(const std::string &stationUuid, const std::string &stationName, const std::vector<MvrXchangeCommit> &commits);
std::string BuildJoinRet(const std::string &stationUuid, const std::string &stationName, bool ok, const std::string &message);
std::string BuildLeave(const std::string &stationUuid);
std::string BuildLeaveRet(bool ok = true, const std::string &message = {});
std::string BuildCommit(const MvrXchangeCommit &commit);
std::string BuildCommitRet(bool ok, const std::string &message = {});
std::string BuildRequest(const std::string &fileUuid, const std::vector<std::string> &fromStationUuids = {});
std::string BuildRequestError(const std::string &message);
std::string BuildTypedErrorResponse(const std::string &requestType, const std::string &stationUuid, const std::string &stationName, const std::string &message);

}
