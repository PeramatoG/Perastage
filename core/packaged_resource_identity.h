#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace packaged_resources {
// Stable package namespace + logical resource identities for extracted assets.
// Registration is owned by the workspace/scene lease, never by a GUI widget.
struct Identity;
inline std::mutex mutex;
inline std::unordered_map<std::string, std::weak_ptr<const Identity>> identities;
inline std::string Key(const std::filesystem::path &path) {
  std::error_code ec;
  const auto key = std::filesystem::absolute(path, ec).lexically_normal().generic_u8string();
  return ec ? std::string{} : std::string(key.begin(), key.end());
}
struct Identity {
  std::string path, logicalIdentity;
  std::uintmax_t size;
  std::filesystem::file_time_type time;
  ~Identity() {
    std::lock_guard lock(mutex);
    auto it = identities.find(path);
    if (it != identities.end() && it->second.expired()) identities.erase(it);
  }
};
inline std::shared_ptr<const Identity> Register(const std::filesystem::path &path,
                                               std::string logicalIdentity) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) return {};
  const auto time = std::filesystem::last_write_time(path, ec);
  if (ec) return {};
  auto entry = std::make_shared<Identity>();
  entry->path = Key(path); entry->logicalIdentity = std::move(logicalIdentity);
  entry->size = size; entry->time = time;
  std::lock_guard lock(mutex);
  identities[entry->path] = entry;
  return entry;
}
inline std::optional<std::string> Lookup(const std::filesystem::path &path) {
  std::shared_ptr<const Identity> entry;
  {
    std::lock_guard lock(mutex);
    const auto it = identities.find(Key(path));
    if (it == identities.end()) return std::nullopt;
    entry = it->second.lock();
  }
  if (!entry) return std::nullopt;
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size != entry->size) return std::nullopt;
  const auto time = std::filesystem::last_write_time(path, ec);
  if (ec || time != entry->time) return std::nullopt;
  return "packaged-resource:" + entry->logicalIdentity;
}
} // namespace packaged_resources
