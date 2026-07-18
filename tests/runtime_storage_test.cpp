#include "runtime_storage.h"

#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

// Verifies that a temporary workspace removes its directory on scope exit.
static void TestWorkspaceDestructorCleanup(const fs::path &root) {
  fs::path created;
  {
    runtime_storage::TemporaryWorkspace workspace("test-cleanup");
    assert(workspace.IsValid());
    created = workspace.Path();
    assert(fs::is_directory(created));
  }
  assert(!fs::exists(created));
}

// Verifies that moved workspaces do not double-delete or leak ownership.
static void TestWorkspaceMoveOwnership(const fs::path &root) {
  fs::path created;
  {
    runtime_storage::TemporaryWorkspace first("test-move");
    created = first.Path();
    runtime_storage::TemporaryWorkspace second(std::move(first));
    assert(second.IsValid());
    assert(fs::is_directory(created));
  }
  assert(!fs::exists(created));
}

// Verifies that scene leases keep transferred resources alive until final release.
static void TestSceneLeaseLifetime(const fs::path &root) {
  fs::path created;
  runtime_storage::SceneResourceLeasePtr lease;
  {
    runtime_storage::TemporaryWorkspace workspace("test-lease");
    created = workspace.Path();
    lease = workspace.TransferToSceneLease();
  }
  assert(fs::is_directory(created));
  auto copied = lease;
  lease.reset();
  assert(fs::is_directory(created));
  copied.reset();
  assert(!fs::exists(created));
}

// Verifies that containment checks reject paths outside the runtime root.
static void TestContainment(const fs::path &root) {
  assert(runtime_storage::IsInsideRuntimeRoot(root / "operations" / "x"));
  assert(!runtime_storage::IsInsideRuntimeRoot(root.parent_path() / "outside"));
}

// Runs runtime storage ownership tests with an injected temporary root.
int main() {
  const fs::path root = fs::temp_directory_path() / "perastage_runtime_storage_test_root";
  std::error_code ec;
  fs::remove_all(root, ec);
  runtime_storage::SetRuntimeRootOverrideForTests(root);
  TestWorkspaceDestructorCleanup(root);
  TestWorkspaceMoveOwnership(root);
  TestSceneLeaseLifetime(root);
  TestContainment(root);
  fs::remove_all(root, ec);
  return 0;
}
