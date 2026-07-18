#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace runtime_storage {

std::filesystem::path GetPerastageRuntimeRoot();
std::filesystem::path GetPerastageSessionRoot();
std::filesystem::path GetPerastageOperationRoot();
std::filesystem::path GetPerastageCacheRoot();
void SetRuntimeRootOverrideForTests(const std::filesystem::path &root);
void CleanupStaleRuntimeStorage();

class SceneResourceLease;

class TemporaryWorkspace {
public:
  explicit TemporaryWorkspace(const std::string &kind = "operation");
  ~TemporaryWorkspace();
  TemporaryWorkspace(TemporaryWorkspace &&other) noexcept;
  TemporaryWorkspace &operator=(TemporaryWorkspace &&other) noexcept;
  TemporaryWorkspace(const TemporaryWorkspace &) = delete;
  TemporaryWorkspace &operator=(const TemporaryWorkspace &) = delete;

  const std::filesystem::path &Path() const { return path_; }
  bool IsValid() const { return !path_.empty(); }
  void Cleanup();
  std::shared_ptr<SceneResourceLease> TransferToSceneLease();

private:
  std::filesystem::path path_;
};

class SceneResourceLease {
public:
  explicit SceneResourceLease(std::filesystem::path path = {});
  ~SceneResourceLease();
  SceneResourceLease(const SceneResourceLease &) = delete;
  SceneResourceLease &operator=(const SceneResourceLease &) = delete;
  const std::filesystem::path &Path() const { return path_; }

private:
  std::filesystem::path path_;
};

using SceneResourceLeasePtr = std::shared_ptr<SceneResourceLease>;

bool IsInsideRuntimeRoot(const std::filesystem::path &path);
void RemoveOwnedPath(const std::filesystem::path &path, const std::string &label);

} // namespace runtime_storage
