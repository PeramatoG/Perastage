#include "runtime_storage.h"

#include "logger.h"

#include <chrono>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

namespace runtime_storage {
namespace fs = std::filesystem;
namespace {
fs::path g_overrideRoot;
fs::path g_sessionRoot;

// Builds a process-local unique token for runtime directory names.
std::string UniqueToken() {
  static std::mt19937_64 rng{std::random_device{}()};
  std::ostringstream ss;
  ss << std::chrono::steady_clock::now().time_since_epoch().count() << '-'
     << std::hex << rng();
  return ss.str();
}

// Creates a directory and its parents without throwing.
bool CreateDirectories(const fs::path &path) {
  std::error_code ec;
  fs::create_directories(path, ec);
  return !ec && fs::is_directory(path, ec);
}

// Returns the canonical runtime root for containment checks.
fs::path RuntimeRootForChecks() {
  std::error_code ec;
  fs::path root = fs::weakly_canonical(GetPerastageRuntimeRoot(), ec);
  return ec ? GetPerastageRuntimeRoot() : root;
}
} // namespace

// Returns the Perastage-owned runtime root under the system temporary folder.
fs::path GetPerastageRuntimeRoot() {
  if (!g_overrideRoot.empty())
    return g_overrideRoot;
  return fs::temp_directory_path() / "Perastage";
}

// Returns the current process session root for session-lifetime artifacts.
fs::path GetPerastageSessionRoot() {
  if (!g_sessionRoot.empty())
    return g_sessionRoot;
  g_sessionRoot = GetPerastageRuntimeRoot() / "sessions" / ("session-" + UniqueToken());
  CreateDirectories(g_sessionRoot);
  std::ofstream marker(g_sessionRoot / "perastage-session.marker");
  marker << "Perastage runtime session\n";
  return g_sessionRoot;
}

// Returns the shared operation root for operation-scoped workspaces.
fs::path GetPerastageOperationRoot() {
  fs::path root = GetPerastageRuntimeRoot() / "operations";
  CreateDirectories(root);
  return root;
}

// Returns the versioned session cache root for regenerable runtime caches.
fs::path GetPerastageCacheRoot() {
  fs::path root = GetPerastageSessionRoot() / "cache" / "v1";
  CreateDirectories(root);
  return root;
}

// Overrides the runtime root in tests and resets the process session.
void SetRuntimeRootOverrideForTests(const fs::path &root) {
  g_overrideRoot = root;
  g_sessionRoot.clear();
}

// Removes old Perastage session directories only when they contain the marker file.
void CleanupStaleRuntimeStorage() {
  const fs::path sessions = GetPerastageRuntimeRoot() / "sessions";
  std::error_code ec;
  if (!fs::is_directory(sessions, ec))
    return;
  size_t removed = 0;
  for (const auto &entry : fs::directory_iterator(sessions, ec)) {
    if (ec || !entry.is_directory(ec))
      continue;
    if (entry.path() == g_sessionRoot)
      continue;
    if (!fs::is_regular_file(entry.path() / "perastage-session.marker", ec))
      continue;
    fs::remove_all(entry.path(), ec);
    if (!ec)
      ++removed;
    ec.clear();
  }
  Logger::Instance().Log(Logger::Level::Info,
                         "Runtime storage stale-session cleanup removed " +
                             std::to_string(removed) + " session(s).");
}

// Returns true when a path resolves below the Perastage runtime root.
bool IsInsideRuntimeRoot(const fs::path &path) {
  std::error_code ec;
  const fs::path root = RuntimeRootForChecks();
  fs::path target = fs::weakly_canonical(path, ec);
  if (ec)
    target = fs::absolute(path, ec);
  const auto rootText = root.lexically_normal().string();
  const auto targetText = target.lexically_normal().string();
  std::string rootPrefix = rootText;
  rootPrefix.push_back(static_cast<char>(fs::path::preferred_separator));
  return targetText == rootText || targetText.rfind(rootPrefix, 0) == 0;
}

// Removes an owned runtime path after validating containment.
void RemoveOwnedPath(const fs::path &path, const std::string &label) {
  if (path.empty())
    return;
  if (!IsInsideRuntimeRoot(path)) {
    Logger::Instance().Log(Logger::Level::Warn,
                           "Refusing to remove non-runtime " + label + ": " + path.string());
    return;
  }
  std::error_code ec;
  const auto count = fs::remove_all(path, ec);
  Logger::Instance().Log(ec ? Logger::Level::Warn : Logger::Level::Info,
                         (ec ? "Failed removing " : "Removed ") + label + ": " +
                             path.string() + " entries=" + std::to_string(count));
}

// Creates a unique operation-scoped workspace.
TemporaryWorkspace::TemporaryWorkspace(const std::string &kind) {
  const fs::path root = GetPerastageOperationRoot();
  for (int i = 0; i < 64; ++i) {
    fs::path candidate = root / (kind + '-' + UniqueToken());
    std::error_code ec;
    if (fs::create_directory(candidate, ec) && !ec) {
      path_ = candidate;
      Logger::Instance().Log(Logger::Level::Info, "Created runtime workspace: " + path_.string());
      break;
    }
  }
}

// Removes the owned temporary workspace.
TemporaryWorkspace::~TemporaryWorkspace() { Cleanup(); }

// Moves temporary workspace ownership.
TemporaryWorkspace::TemporaryWorkspace(TemporaryWorkspace &&other) noexcept : path_(std::move(other.path_)) { other.path_.clear(); }

// Replaces this workspace with another owned workspace.
TemporaryWorkspace &TemporaryWorkspace::operator=(TemporaryWorkspace &&other) noexcept {
  if (this != &other) {
    Cleanup();
    path_ = std::move(other.path_);
    other.path_.clear();
  }
  return *this;
}

// Removes the workspace immediately when still owned.
void TemporaryWorkspace::Cleanup() {
  RemoveOwnedPath(path_, "temporary workspace");
  path_.clear();
}

// Transfers operation workspace ownership to a scene/session lease.
std::shared_ptr<SceneResourceLease> TemporaryWorkspace::TransferToSceneLease() {
  auto lease = std::make_shared<SceneResourceLease>(path_);
  path_.clear();
  return lease;
}

// Creates a lease for scene/session runtime resources.
SceneResourceLease::SceneResourceLease(fs::path path) : path_(std::move(path)) {}

// Releases scene/session runtime resources when the final shared owner disappears.
SceneResourceLease::~SceneResourceLease() { RemoveOwnedPath(path_, "scene resource workspace"); }

} // namespace runtime_storage
