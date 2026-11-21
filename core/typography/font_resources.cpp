#include "font_resources.h"
#include "file_import_utils.h"
#include "system_font_paths.h"
#include <algorithm>
#include <array>
#include <mutex>

namespace typography {
namespace {
struct Approval {
  std::string_view family, filename, version, postScript, hash, provenance;
  std::uintmax_t size;
  bool bold, italic = false;
};
constexpr auto kNoto = "Debian fonts-noto-core 20201225-2";
constexpr auto kAdobe = "Adobe Source Sans 3.052R ed1808970eb3c7301c9a523bee26473ba0bb62fa";
constexpr std::array<Approval, 8> kApprovals{{
    {kDefaultFamily, "PerastageSans.ttf", "2.004", "NotoSans-Regular",
     "89c3c497f618fdaa0b2d1e98fef93582f28c71debd2c4a8cdf41f190ced2909d", kNoto, 512672, false},
    {kDefaultFamily, "PerastageSans-Bold.ttf", "2.004", "NotoSans-Bold",
     "e83493c945848ecd4a9ad0f6d19164541a0d3e23a9c952304a00a46e00272ac5", kNoto, 515752, true},
    {"Noto Serif", "NotoSerif-Regular.ttf", "2.004", "NotoSerif-Regular",
     "9d7583b7dc9e812afd32a14280c5cac3160012efe50c8d08938f4fea266ff67f", kNoto, 588876, false},
    {"Noto Serif", "NotoSerif-Bold.ttf", "2.004", "NotoSerif-Bold",
     "0af0ff2be8f84910fb21ec5fe1b6b7395e3073250502a334baf6ca2f860c88fe", kNoto, 611904, true},
    {"Source Sans 3", "SourceSans3-Regular.ttf", "3.052", "SourceSans3-Regular",
     "4644c81b86ec9caaa76b634889968ed3c4f4f52f054855933acc7c2b21e53b0f", kAdobe, 431196, false},
    {"Source Sans 3", "SourceSans3-Bold.ttf", "3.052", "SourceSans3-Bold",
     "9214b9d95e4231c609802815c2646c98174e2102d0d37f88978a7f8e71006e6a", kAdobe, 428176, true},
    {kDefaultFamily, "PerastageSans-Italic.ttf", "2.004", "NotoSans-Italic",
     "5d7618dda819443fd07301c84adcee9fe8960c873ea5e5e1f0f59db384b78082", kNoto, 530120, false, true},
    {kDefaultFamily, "PerastageSans-BoldItalic.ttf", "2.004", "NotoSans-BoldItalic",
     "4315b26e5afb62f70f5ffe0edaa3363e57765ff2af2beda12b6fef197e3e5f5a", kNoto, 534180, true, true},
}};

FontIssue LoadApproved(const std::filesystem::path &path, const Approval &record,
                       FontResource &face, FontSource source) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
  if (ec == std::errc::no_such_file_or_directory) return FontIssue::Missing;
  if (ec) return FontIssue::Inaccessible;
  if (size > 8 * 1024 * 1024) return FontIssue::Unapproved;
  std::ifstream stream(path, std::ios::binary);
  std::string bytes(static_cast<size_t>(size), '\0');
  if (!stream.read(bytes.data(), static_cast<std::streamsize>(size)) ||
      stream.peek() != std::char_traits<char>::eof()) return FontIssue::Inaccessible;
  auto format = CheckStaticTrueType(bytes);
  if (format != FontIssue::None) return format;
  if (FileImportUtils::ComputeBytesSha256(bytes.data(), bytes.size()) != record.hash)
    return FontIssue::Unapproved;
  TtfFontMetrics metrics;
  if (!ParseTtfFontMetrics(std::move(bytes), metrics)) return FontIssue::Invalid;
  metrics.resourceId = record.hash;
  face = {record.family, record.version, record.postScript, record.hash,
          "OFL-1.1", record.provenance, source, record.bold, record.italic, path, std::move(metrics)};
  return FontIssue::None;
}

const TtfFontMetrics &Metrics(const FontCatalog &catalog, std::string_view family, bool bold) {
  static const TtfFontMetrics empty;
  const auto resolved = catalog.Resolve(family, bold);
  return resolved.face ? resolved.face->metrics : empty;
}
}

FontCatalog::FontCatalog(const std::filesystem::path &root)
    : FontCatalog(root, SystemFontRoots()) {}

FontCatalog::FontCatalog(const std::filesystem::path &root,
                         const std::vector<std::filesystem::path> &systemRoots) {
  faces_.resize(kApprovals.size());
  auto fontDirectory = root / "fonts";
#ifdef __APPLE__
  // Installed bundles can carry other resources under Contents/Resources/resources.
  std::error_code ec;
  if (!std::filesystem::is_directory(fontDirectory, ec) && root.filename() == "resources" &&
      root.parent_path().filename() == "Resources")
    fontDirectory = root.parent_path() / "fonts";
#endif
  for (std::size_t i = 0; i < kApprovals.size(); ++i)
    issues_.push_back(LoadApproved(fontDirectory / kApprovals[i].filename,
                                  kApprovals[i], faces_[i], FontSource::Bundled));
  for (const auto &directory : systemRoots) {
    if (IsComplete()) break;
    std::error_code ec;
    std::filesystem::recursive_directory_iterator files(
        directory, std::filesystem::directory_options::skip_permission_denied, ec), end;
    for (; !ec && files != end && !IsComplete(); files.increment(ec)) {
      std::error_code entryError;
      if (!files->is_regular_file(entryError)) continue;
      const auto size = files->file_size(entryError);
      if (entryError) continue;
      for (std::size_t i = 0; i < kApprovals.size(); ++i) {
        if (issues_[i] != FontIssue::None && size == kApprovals[i].size &&
            LoadApproved(files->path(), kApprovals[i], faces_[i], FontSource::ApprovedSystem)
                == FontIssue::None)
          issues_[i] = FontIssue::None;
      }
    }
  }
  for (const auto &face : faces_)
    if (!face.fingerprint.empty()) signature_ += face.fingerprint;
}

FontResolution FontCatalog::Resolve(std::string_view family, bool bold, bool italic) const {
  if (family.empty() || family == "Noto Sans") family = kDefaultFamily;
  const auto find = [&](std::string_view requested, bool weight, bool slant) {
    const auto it = std::find_if(kApprovals.begin(), kApprovals.end(), [&](const auto &record) {
      return record.family == requested && record.bold == weight && record.italic == slant;
    });
    return static_cast<size_t>(it - kApprovals.begin());
  };
  const auto requested = find(family, bold, italic);
  const auto upright = find(family, bold, false);
  if (requested < faces_.size() && issues_[requested] == FontIssue::None)
    return {&faces_[requested], FontIssue::None};

  const auto issue = requested < faces_.size() ? issues_[requested] :
      upright < faces_.size() ? FontIssue::FaceUnavailable : FontIssue::Unapproved;
  for (const auto weight : {bold, !bold}) {
    for (const auto slant : {italic, false}) {
      const auto fallback = find(kDefaultFamily, weight, slant);
      if (fallback < faces_.size() && issues_[fallback] == FontIssue::None)
        return {&faces_[fallback], issue};
    }
  }
  return {nullptr, issue};
}

const FontResource *FontCatalog::FindResource(std::string_view id) const {
  if (id.empty()) return nullptr;
  for (const auto &face : faces_)
    if (face.fingerprint == id) return &face;
  return nullptr;
}

std::vector<std::string_view> FontCatalog::Families() const {
  std::vector<std::string_view> families;
  for (std::size_t i = 0; i < faces_.size(); i += 2)
    if (!kApprovals[i].italic && issues_[i] == FontIssue::None && issues_[i + 1] == FontIssue::None)
      families.push_back(faces_[i].family);
  return families;
}

bool FontCatalog::IsComplete() const {
  return std::all_of(issues_.begin(), issues_.end(), [](auto issue) {
    return issue == FontIssue::None;
  });
}

std::shared_ptr<const FontCatalog> GetFontCatalog(const std::filesystem::path &root,
                                                  bool refreshIncomplete) {
  static std::mutex mutex;
  static std::filesystem::path loadedRoot;
  static std::shared_ptr<const FontCatalog> catalog;
  std::lock_guard lock(mutex);
  if (!catalog || root != loadedRoot || (refreshIncomplete && !catalog->IsComplete())) {
    catalog = std::make_shared<const FontCatalog>(root);
    loadedRoot = root;
  }
  return catalog;
}

std::string DescribeFontIssue(const FontCatalog &catalog, std::string_view family,
                              bool bold, FontIssue issue, bool italic) {
  std::string detail;
  switch (issue) {
  case FontIssue::None: return {};
  case FontIssue::Missing: detail = "font unavailable"; break;
  case FontIssue::Unapproved: detail = "font provenance is unverified"; break;
  case FontIssue::UnsupportedFormat: detail = "font format is unsupported"; break;
  case FontIssue::FaceUnavailable: detail = "requested face is unavailable"; break;
  case FontIssue::Invalid: detail = "font file is invalid"; break;
  case FontIssue::EmbeddingUnverified: detail = "font embedding is restricted or unverifiable"; break;
  case FontIssue::Inaccessible: detail = "font file is inaccessible"; break;
  }
  const auto resolved = catalog.Resolve(family, bold, italic);
  if (resolved.face)
    detail += ". Using " + std::string(resolved.face->family) +
              (resolved.face->bold ? " Bold" : resolved.face->italic ? "" : " Regular") +
              (resolved.face->italic ? " Italic" : "");
  return std::string(family.empty() ? kDefaultFamily : family) +
         (bold ? " Bold" : italic ? "" : " Regular") +
         (italic ? " Italic" : "") + ": " + detail + ".";
}

std::filesystem::path BundledFontPath(const std::filesystem::path &root, bool bold) {
  return root / "fonts" / kApprovals[bold].filename;
}

FontPair::FontPair(const std::filesystem::path &root, std::string_view family)
    : resourceRoot(root), catalog(GetFontCatalog(root)),
      regular(Metrics(*catalog, family, false)), bold(Metrics(*catalog, family, true)) {}
} // namespace typography
