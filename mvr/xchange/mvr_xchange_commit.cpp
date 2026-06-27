#include "mvr_xchange_commit.h"
#include "../../core/uuidutils.h"

// Returns the byte size of the MVR payload attached to this commit.
std::size_t MvrXchangeCommit::FileSize() const { return payload.size(); }

// Creates a bounded in-memory store for recently published MVR commits.
MvrXchangeCommitStore::MvrXchangeCommitStore(std::size_t maxCommits)
    : maxCommits_(maxCommits == 0 ? 1 : maxCommits) {}

// Adds a commit and evicts the oldest entry when the bounded history is full.
void MvrXchangeCommitStore::Add(MvrXchangeCommit commit) {
  commit.fileUuid = CanonicalizeUuid(commit.fileUuid);
  commit.stationUuid = CanonicalizeUuid(commit.stationUuid);
  commits_.push_back(std::move(commit));
  while (commits_.size() > maxCommits_)
    commits_.pop_front();
}

// Finds a commit by its MVR-xchange FileUUID.
std::optional<MvrXchangeCommit>
MvrXchangeCommitStore::FindByFileUuid(const std::string &fileUuid) const {
  const std::string canonicalFileUuid = CanonicalizeUuid(fileUuid);
  for (const auto &commit : commits_) {
    if (commit.fileUuid == canonicalFileUuid)
      return commit;
  }
  return std::nullopt;
}

// Returns the most recent published commit when one is available.
std::optional<MvrXchangeCommit> MvrXchangeCommitStore::Latest() const {
  if (commits_.empty())
    return std::nullopt;
  return commits_.back();
}

// Returns a snapshot of the stored commit history.
std::vector<MvrXchangeCommit> MvrXchangeCommitStore::List() const {
  return {commits_.begin(), commits_.end()};
}
