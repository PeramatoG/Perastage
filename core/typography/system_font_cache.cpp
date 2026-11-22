#include "system_font_cache.h"
#include "file_import_utils.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <tuple>

namespace typography {
namespace {
size_t ByteCount(const FontResource &resource) {
  return resource.metrics.data.size() +
      resource.metrics.coverage.size() * sizeof(resource.metrics.coverage.front()) +
      resource.metrics.glyphAdvances.size() * sizeof(int);
}
}

SystemFontCache::FileStamp SystemFontCache::Stamp(const std::filesystem::path &path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error) return {};
  const auto time = std::filesystem::last_write_time(path, error);
  return {size, time, !error};
}

SystemFontCache::SystemFontCache(std::vector<SystemFontRecord> records)
    : records_(std::move(records)), loadLocks_(records_.size()) {
  std::ostringstream identity;
  std::map<std::filesystem::path, FileStamp> stats;
  for (size_t i = 0; i < records_.size(); ++i) {
    const auto &record = records_[i];
    families_[record.family].push_back(i);
    if (!record.postScriptName.empty()) postScriptNames_[record.postScriptName].push_back(i);
    const auto [stat, inserted] = stats.try_emplace(record.path);
    if (inserted) stat->second = Stamp(record.path);
    stamps_.push_back(stat->second);
    const auto path = record.path.generic_u8string();
    identity << path.size() << ':'
             << std::string_view(reinterpret_cast<const char *>(path.data()), path.size())
             << ':' << stat->second.size << ':' << stat->second.time.time_since_epoch().count()
             << ':' << record.index << ':' << record.family << ':' << record.postScriptName
             << ':' << record.weight << ':' << record.width << ':' << record.italic << '\n';
  }
  const auto data = identity.str();
  signature_ = FileImportUtils::ComputeBytesSha256(data.data(), data.size());
}

FontResolution SystemFontCache::Load(size_t record) const {
  const std::lock_guard loadLock(loadLocks_[record]);
  {
    const std::lock_guard lock(mutex_);
    if (const auto it = cache_.find(record); it != cache_.end()) {
      it->second.sequence = ++sequence_;
      return it->second.resolution;
    }
  }
  auto face = std::make_shared<FontResource>();
  FontIssue issue = FontIssue::Inaccessible;
  if (stamps_[record].valid && Stamp(records_[record].path) == stamps_[record]) {
    issue = LoadSystemFont(records_[record], *face);
    if (Stamp(records_[record].path) != stamps_[record]) issue = FontIssue::Inaccessible;
  }
  FontResolution resolution{issue == FontIssue::None ? face : nullptr, issue};
  const size_t bytes = resolution.face ? ByteCount(*face) : 0;
  const std::lock_guard lock(mutex_);
  constexpr size_t budget = 64 * 1024 * 1024;
  while (!cache_.empty() && (bytes_ + bytes > budget || cache_.size() >= 128)) {
    const auto oldest = std::min_element(cache_.begin(), cache_.end(), [](const auto &a, const auto &b) {
      return a.second.sequence < b.second.sequence;
    });
    bytes_ -= oldest->second.bytes;
    cache_.erase(oldest);
  }
  if (resolution.face) {
    if (fingerprints_.size() >= records_.size() * 2) fingerprints_.clear();
    fingerprints_.try_emplace(face->fingerprint, record);
  }
  cache_.emplace(record, Entry{resolution, bytes, ++sequence_});
  bytes_ += bytes;
  return resolution;
}

FontResolution SystemFontCache::Resolve(const FontReference &reference, bool bold, bool italic) const {
  const auto variant = FontReference::Variant(bold, italic);
  const auto savedFingerprint = reference.fingerprints[variant];
  const auto ps = postScriptNames_.find(reference.postScriptNames[variant]);
  if (ps != postScriptNames_.end()) {
    FontResolution sameName;
    for (const auto index : ps->second) {
      auto resolved = Load(index);
      if (!resolved.face || resolved.face->bold != bold || resolved.face->italic != italic) continue;
      if (resolved.face->fingerprint == savedFingerprint) return resolved;
      if (!sameName.face && resolved.face->family == reference.family) sameName = std::move(resolved);
    }
    if (sameName.face) return {sameName.face, FontIssue::CompatibleFace};
  }
  const auto family = families_.find(reference.family);
  if (family == families_.end()) return {nullptr, FontIssue::Missing};
  auto candidates = family->second;
  const auto rank = [&](size_t index) {
    const auto &face = records_[index];
    return std::tuple(face.italic != italic, std::abs(face.weight - (bold ? 700 : 400)),
                      std::abs(face.width - 5), index);
  };
  std::sort(candidates.begin(), candidates.end(), [&](size_t a, size_t b) { return rank(a) < rank(b); });
  FontIssue issue = FontIssue::FaceUnavailable;
  std::shared_ptr<const FontResource> nearest;
  for (const auto index : candidates) {
    auto resolved = Load(index);
    if (!resolved.face) {
      if (issue == FontIssue::FaceUnavailable) issue = resolved.issue;
      continue;
    }
    if (resolved.face->italic == italic && resolved.face->bold == bold) {
      if (!savedFingerprint.empty()) resolved.issue = FontIssue::CompatibleFace;
      return resolved;
    }
    if (!nearest) nearest = std::move(resolved.face);
  }
  return nearest ? FontResolution{nearest, FontIssue::FaceUnavailable} : FontResolution{nullptr, issue};
}

std::shared_ptr<const FontResource> SystemFontCache::FindResource(std::string_view id) const {
  size_t index = 0;
  {
    const std::lock_guard lock(mutex_);
    const auto it = fingerprints_.find(id);
    if (it == fingerprints_.end()) return {};
    index = it->second;
  }
  const auto resolved = Load(index);
  return resolved.face && resolved.face->fingerprint == id ? resolved.face : nullptr;
}

std::vector<std::string> SystemFontCache::Families() const {
  std::vector<std::string> result;
  result.reserve(families_.size());
  for (const auto &[name, faces] : families_) result.push_back(name);
  return result;
}
}
