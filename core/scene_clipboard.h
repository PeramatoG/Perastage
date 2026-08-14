#pragma once

#include "configservices.h"
#include "scene_grouping.h"

#include <optional>
#include <unordered_map>
#include <variant>

namespace scene_clipboard {

using SceneValue = std::variant<Fixture, Truss, Support, SceneObject>;

struct Item {
  MvrNodeType type = MvrNodeType::SceneObject;
  std::string sourceUuid;
  SceneValue value;
  Matrix sourceWorldTransform{};
  std::optional<std::string> fixtureLabelOverride;
};

struct Payload {
  std::uint64_t projectEpoch = 0;
  std::vector<Item> items;
  bool Empty() const { return items.empty(); }
};

struct MutationResult {
  bool changed = false;
  std::vector<scene_grouping::SceneTransformTarget> nodes;
  std::unordered_map<std::string, std::string> uuidRemap;
};

// Owns a value-based, project-scoped clipboard for supported scene instances.
class Service {
public:
  bool Capture(const MvrScene &scene, const SelectionState &selection,
               std::uint64_t projectEpoch,
               const std::unordered_map<std::string, std::string> &labelOverrides = {});
  MutationResult Paste(MvrScene &scene, std::uint64_t projectEpoch,
                       std::unordered_map<std::string, std::string> *labelOverrides = nullptr) const;
  MutationResult Cut(MvrScene &scene, SelectionState &selection,
                     std::uint64_t projectEpoch,
                     std::unordered_map<std::string, std::string> *labelOverrides = nullptr);
  bool CanPaste(std::uint64_t projectEpoch) const;
  const Payload &GetPayload() const { return payload_; }
  void Clear();

private:
  Payload payload_;
};

// Restores provisional scene edits exactly unless explicitly committed.
class MutationTransaction {
public:
  MutationTransaction(MvrScene &scene, SelectionState &selection,
                      ProjectSession &session,
                      std::unordered_map<std::string, std::string> *metadata = nullptr);
  ~MutationTransaction();
  void Commit();
  void Cancel();

private:
  MvrScene &scene_;
  SelectionState &selection_;
  ProjectSession &session_;
  std::unordered_map<std::string, std::string> *metadata_;
  MvrScene sceneBefore_;
  SelectionState selectionBefore_;
  ProjectSession::DirtyState dirtyBefore_;
  std::unordered_map<std::string, std::string> metadataBefore_;
  bool active_ = true;
};

} // namespace scene_clipboard
