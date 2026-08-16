#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

struct MvrXchangeCommit {
  std::string fileUuid;
  std::string stationUuid;
  std::string fileName;
  std::string comment;
  std::string timestampUtc;
  std::vector<uint8_t> payload;
  std::uint64_t declaredFileSize = 0;
  bool declaredFileSizeSpecified = false;
  int verMajor = 0;
  int verMinor = 0;
  std::vector<std::string> forStationsUuid;

  std::size_t FileSize() const;
};

class MvrXchangeCommitStore {
public:
  explicit MvrXchangeCommitStore(std::size_t maxCommits = 5);
  void Add(MvrXchangeCommit commit);
  std::optional<MvrXchangeCommit> FindByFileUuid(const std::string &fileUuid) const;
  std::optional<MvrXchangeCommit> Latest() const;
  std::vector<MvrXchangeCommit> List() const;

private:
  std::size_t maxCommits_;
  std::deque<MvrXchangeCommit> commits_;
};
