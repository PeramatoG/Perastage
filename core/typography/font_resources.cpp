#include "font_resources.h"
#include "file_import_utils.h"
#include "system_font_paths.h"
#include "system_font_cache.h"
#include <algorithm>
#include <array>
#include <mutex>

namespace typography {
namespace {
struct Approval {
  std::string_view family, filename, version, postScript, hash, provenance;
  std::uintmax_t size;
  bool bold, italic = false, legacyWrap = false;
};
constexpr auto kNoto = "Debian fonts-noto-core 20201225-2";
constexpr auto kMono = "Debian fonts-noto-mono 20201225-2";
constexpr auto kAdobe = "Adobe Source Sans 3.052R ed1808970eb3c7301c9a523bee26473ba0bb62fa";
constexpr std::array<Approval, 18> kApprovals{{
    {kDefaultFamily, "PerastageSans.ttf", "2.004", "NotoSans-Regular",
     "89c3c497f618fdaa0b2d1e98fef93582f28c71debd2c4a8cdf41f190ced2909d", kNoto, 512672, false, false, true},
    {kDefaultFamily, "PerastageSans-Bold.ttf", "2.004", "NotoSans-Bold",
     "e83493c945848ecd4a9ad0f6d19164541a0d3e23a9c952304a00a46e00272ac5", kNoto, 515752, true},
    {"Noto Serif", "NotoSerif-Regular.ttf", "2.004", "NotoSerif-Regular",
     "9d7583b7dc9e812afd32a14280c5cac3160012efe50c8d08938f4fea266ff67f", kNoto, 588876, false, false, true},
    {"Noto Serif", "NotoSerif-Bold.ttf", "2.004", "NotoSerif-Bold",
     "0af0ff2be8f84910fb21ec5fe1b6b7395e3073250502a334baf6ca2f860c88fe", kNoto, 611904, true},
    {"Source Sans 3", "SourceSans3-Regular.ttf", "3.052", "SourceSans3-Regular",
     "4644c81b86ec9caaa76b634889968ed3c4f4f52f054855933acc7c2b21e53b0f", kAdobe, 431196, false, false, true},
    {"Source Sans 3", "SourceSans3-Bold.ttf", "3.052", "SourceSans3-Bold",
     "9214b9d95e4231c609802815c2646c98174e2102d0d37f88978a7f8e71006e6a", kAdobe, 428176, true},
    {kDefaultFamily, "PerastageSans-Italic.ttf", "2.004", "NotoSans-Italic",
     "5d7618dda819443fd07301c84adcee9fe8960c873ea5e5e1f0f59db384b78082", kNoto, 530120, false, true},
    {kDefaultFamily, "PerastageSans-BoldItalic.ttf", "2.004", "NotoSans-BoldItalic",
     "4315b26e5afb62f70f5ffe0edaa3363e57765ff2af2beda12b6fef197e3e5f5a", kNoto, 534180, true, true},
    {"Noto Sans Mono", "NotoSansMono-Regular.ttf", "2.006", "NotoSansMono-Regular",
     "6b692c4b6d15ccf59f1c1fe8d11cb8a92f51960f3e9f1f523781755a3af7e29f", kMono, 510612, false, false},
    {"Noto Sans Mono", "NotoSansMono-Bold.ttf", "2.006", "NotoSansMono-Bold",
     "b22563d5f03b345a62115e78d8789c9ed99c2cfa8059fad6e7a5c4ced288e21e", kMono, 523412, true, false},
    {"Noto Sans Arabic", "NotoSansArabic-Regular.ttf", "2.005", "NotoSansArabic-Regular",
     "504d7407d86875acf7d04dfaa0fd7524d0b8797723bc4aa18022f29db25b0b6e", kNoto, 244072, false, false},
    {"Noto Sans Arabic", "NotoSansArabic-Bold.ttf", "2.005", "NotoSansArabic-Bold",
     "ded6fc7359ca36d15d7aab9ef0c066e21ce48b26a069994d6602fa2cb9a1b952", kNoto, 252940, true, false},
    {"Noto Sans Hebrew", "NotoSansHebrew-Regular.ttf", "3.000", "NotoSansHebrew-Regular",
     "436900d5ad77d33e4234247f3076eaecd25b92b9bea0519514f684140e3566a7", kNoto, 26892, false, false},
    {"Noto Sans Hebrew", "NotoSansHebrew-Bold.ttf", "3.000", "NotoSansHebrew-Bold",
     "d3b01343f71e8a93fffb84c345a1aecbaf4694b98d0eaace916cb9eac9e6dcd3", kNoto, 27156, true, false},
    {"Noto Serif", "NotoSerif-Italic.ttf", "2.004", "NotoSerif-Italic",
     "bc25600aa27cd409e1e5b3d86340df3a329bb860fcfbe57a03a95070b229e1b0", kNoto, 637472, false, true},
    {"Noto Serif", "NotoSerif-BoldItalic.ttf", "2.004", "NotoSerif-BoldItalic",
     "78a6d685e5690b7dcda0f336b689193afc03b6bab5cf18fc1a9ecc3aa0600059", kNoto, 656716, true, true},
    {"Source Sans 3", "SourceSans3-It.ttf", "3.052", "SourceSans3-It",
     "192afd78f0f54a3c69eaf02d43f4d9a821e9d6110e41d3d25d61a7385cd580e4", kAdobe, 315812, false, true},
    {"Source Sans 3", "SourceSans3-BoldIt.ttf", "3.052", "SourceSans3-BoldIt",
     "7978291fc1bf314db887e0366853b33c5cf2e964c7b95cfb9ce403a6ec46a842", kAdobe, 318700, true, true},
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
  face.family = record.family;
  face.version = record.version;
  face.postScriptName = record.postScript;
  face.fingerprint = record.hash;
  face.license = "OFL-1.1";
  face.provenance = record.provenance;
  face.source = source;
  face.bold = record.bold;
  face.italic = record.italic;
  face.weight = record.bold ? 700 : 400;
  face.path = path;
  face.metrics = std::move(metrics);
  face.embedding = ClassifyEmbedding(4, 0);
  return FontIssue::None;
}

const TtfFontMetrics &Metrics(const std::shared_ptr<const FontResource> &face) {
  static const TtfFontMetrics empty;
  return face ? face->metrics : empty;
}
}

FontCatalog::FontCatalog(const std::filesystem::path &root)
    : FontCatalog(root, SystemFontRoots(), DiscoverSystemFonts()) {}

FontCatalog::FontCatalog(const std::filesystem::path &root,
                         const std::vector<std::filesystem::path> &systemRoots)
    : FontCatalog(root, systemRoots, {}) {}

FontCatalog::FontCatalog(const std::filesystem::path &root,
                         const std::vector<std::filesystem::path> &systemRoots,
                         std::vector<SystemFontRecord> systemFonts) {
  if (!systemFonts.empty()) system_ = std::make_shared<SystemFontCache>(std::move(systemFonts));
  faces_.resize(kApprovals.size());
  auto fontDirectory = root / "fonts";
#ifdef __APPLE__
  // Installed bundles can carry other resources under Contents/Resources/resources.
  std::error_code ec;
  if (!std::filesystem::is_directory(fontDirectory, ec) && root.filename() == "resources" &&
      root.parent_path().filename() == "Resources")
    fontDirectory = root.parent_path() / "fonts";
#endif
  const auto tryLoad = [&](size_t index, const std::filesystem::path &path, FontSource source) {
    auto face = std::make_shared<FontResource>();
    const auto issue = LoadApproved(path, kApprovals[index], *face, source);
    if (issue == FontIssue::None) faces_[index] = std::move(face);
    return issue;
  };
  for (size_t i = 0; i < kApprovals.size(); ++i)
    issues_.push_back(tryLoad(i, fontDirectory / kApprovals[i].filename, FontSource::Bundled));
  // Check conventional names and a common Linux package location before walking.
  for (const auto &directory : systemRoots) {
    if (IsComplete()) break;
    for (size_t i = 0; i < kApprovals.size(); ++i) {
      if (issues_[i] == FontIssue::None) continue;
      const auto &record = kApprovals[i];
      const auto name = std::string(record.postScript) + ".ttf";
      for (const auto &candidate : {directory / record.filename, directory / name,
                                    directory / "truetype/noto" / name}) {
        if (tryLoad(i, candidate, FontSource::ApprovedSystem) == FontIssue::None) {
          issues_[i] = FontIssue::None;
          break;
        }
      }
    }
  }
  size_t remainingEntries = 8192;
  for (const auto &directory : systemRoots) {
    if (IsComplete() || !remainingEntries) break;
    std::error_code ec;
    std::filesystem::recursive_directory_iterator files(
        directory, std::filesystem::directory_options::skip_permission_denied, ec), end;
    for (; !ec && files != end && remainingEntries && !IsComplete(); files.increment(ec), --remainingEntries) {
      if (files.depth() >= 6) files.disable_recursion_pending();
      std::error_code entryError;
      if (!files->is_regular_file(entryError)) continue;
      const auto size = files->file_size(entryError);
      if (entryError) continue;
      for (std::size_t i = 0; i < kApprovals.size(); ++i) {
        if (issues_[i] != FontIssue::None && size == kApprovals[i].size &&
            tryLoad(i, files->path(), FontSource::ApprovedSystem)
                == FontIssue::None)
          issues_[i] = FontIssue::None;
      }
    }
  }
  for (const auto &face : faces_)
    if (face) signature_ += face->fingerprint;
  if (system_) signature_ += "|" + system_->Signature();
}

FontResolution FontCatalog::Resolve(std::string_view family, bool bold, bool italic, FontUsage usage) const {
  auto reference = DecodeFontReference(family);
  if (reference.family.empty() || reference.family == "Noto Sans") reference.family = kDefaultFamily;
  family = reference.family;
  const auto find = [&](std::string_view requested, bool weight, bool slant) {
    const auto it = std::find_if(kApprovals.begin(), kApprovals.end(), [&](const auto &record) {
      return record.family == requested && record.bold == weight && record.italic == slant;
    });
    return static_cast<size_t>(it - kApprovals.begin());
  };
  const auto requested = find(family, bold, italic);
  const auto upright = find(family, bold, false);
  if (requested < faces_.size() && issues_[requested] == FontIssue::None)
    return {faces_[requested], FontIssue::None};

  auto issue = requested < faces_.size() ? issues_[requested] :
      upright < faces_.size() ? FontIssue::FaceUnavailable : FontIssue::Missing;
  const bool bundledFamily = std::any_of(kApprovals.begin(), kApprovals.end(),
      [&](const Approval &record) { return record.family == family; });
  if (!bundledFamily && system_) {
    auto installed = system_->Resolve(reference, bold, italic);
    if (!installed.face) issue = installed.issue;
    else if (usage == FontUsage::Local || installed.face->CanEmbedPdf()) return installed;
    else issue = installed.face->embedding.PermitsPdf() ? FontIssue::PdfFormatUnsupported :
                                                          FontIssue::EmbeddingUnverified;
  }
  for (const auto weight : {bold, !bold}) {
    for (const auto slant : {italic, false}) {
      const auto fallback = find(kDefaultFamily, weight, slant);
      if (fallback < faces_.size() && issues_[fallback] == FontIssue::None)
        return {faces_[fallback], issue};
    }
  }
  return {nullptr, issue};
}

std::shared_ptr<const FontResource> FontCatalog::FindResource(std::string_view id) const {
  if (id.empty()) return {};
  for (const auto &face : faces_)
    if (face && face->fingerprint == id) return face;
  return system_ ? system_->FindResource(id) : nullptr;
}

std::vector<std::string> FontCatalog::SystemFamilies() const {
  auto result = system_ ? system_->Families() : std::vector<std::string>{};
  std::erase_if(result, [](const auto &family) {
    return family == "Noto Sans" || std::any_of(kApprovals.begin(), kApprovals.end(),
        [&](const Approval &record) { return record.family == family; });
  });
  return result;
}

std::vector<std::string_view> FontCatalog::Families() const {
  std::vector<std::string_view> families;
  for (size_t i = 0; i < kApprovals.size(); ++i) {
    const auto &record = kApprovals[i];
    if (record.bold || record.italic || record.family == "Noto Sans Arabic" ||
        record.family == "Noto Sans Hebrew" || issues_[i] != FontIssue::None) continue;
    if (Resolve(record.family, true).issue == FontIssue::None)
      families.push_back(record.family);
  }
  return families;
}

std::string FontCatalog::ReferenceForFamily(std::string_view family) const {
  FontReference reference{family};
  std::array<std::shared_ptr<const FontResource>, 4> faces;
  for (size_t variant = 0; variant < faces.size(); ++variant) {
    const auto selected = Resolve(family, variant & 1, variant & 2);
    if (selected.issue != FontIssue::None || !selected.face ||
        selected.face->source != FontSource::InstalledSystem) continue;
    faces[variant] = selected.face;
    reference.postScriptNames[variant] = faces[variant]->postScriptName;
    reference.fingerprints[variant] = faces[variant]->fingerprint;
  }
  return std::any_of(faces.begin(), faces.end(), [](const auto &face) { return bool(face); })
      ? EncodeFontReference(reference) : std::string(family);
}

bool FontCatalog::LegacyWrapDefault(std::string_view family) {
  if (family == "Noto Sans") family = kDefaultFamily;
  return std::any_of(kApprovals.begin(), kApprovals.end(), [&](const Approval &record) {
    return record.family == family && record.legacyWrap;
  });
}

bool FontCatalog::IsComplete() const {
  return std::all_of(issues_.begin(), issues_.end(), [](auto issue) {
    return issue == FontIssue::None;
  });
}

std::shared_ptr<const FontCatalog> GetFontCatalog(const std::filesystem::path &root,
                                                  FontRefresh refresh) {
  static std::mutex mutex;
  static std::filesystem::path loadedRoot;
  static std::shared_ptr<const FontCatalog> catalog;
  static std::mutex initialize;
  const auto cached = [&]() -> std::shared_ptr<const FontCatalog> {
    std::lock_guard lock(mutex);
    if (root != loadedRoot || !catalog || refresh == FontRefresh::All ||
        (refresh == FontRefresh::Incomplete && !catalog->IsComplete())) return {};
    return catalog;
  };
  if (auto current = cached()) return current;
  const std::lock_guard initLock(initialize);
  if (auto current = cached()) return current;
  auto next = refresh == FontRefresh::All ?
      std::make_shared<const FontCatalog>(root, SystemFontRoots(), DiscoverSystemFonts(true)) :
      std::make_shared<const FontCatalog>(root);
  std::lock_guard lock(mutex);
  catalog = next;
  loadedRoot = root;
  return next;
}

std::string DescribeFontIssue(const FontCatalog &catalog, std::string_view family,
                              bool bold, FontIssue issue, bool italic, FontUsage usage) {
  std::string detail;
  switch (issue) {
  case FontIssue::None: return {};
  case FontIssue::Missing: detail = "font unavailable"; break;
  case FontIssue::Unapproved: detail = "font provenance is unverified"; break;
  case FontIssue::UnsupportedFormat: detail = "font format is unsupported"; break;
  case FontIssue::FaceUnavailable: detail = "requested face is unavailable"; break;
  case FontIssue::Invalid: detail = "font file is invalid"; break;
  case FontIssue::EmbeddingUnverified: {
    const auto local = catalog.Resolve(family, bold, italic);
    detail = local.face && local.face->embedding.right == EmbeddingRight::PreviewPrint
        ? "font requires a read-only document for embedding" : "font embedding is restricted or unverifiable";
    break;
  }
  case FontIssue::Inaccessible: detail = "font file is inaccessible"; break;
  case FontIssue::PdfFormatUnsupported: detail = "font format cannot be embedded in PDF"; break;
  case FontIssue::CompatibleFace: detail = "the saved font identity is unavailable"; break;
  }
  const auto resolved = catalog.Resolve(family, bold, italic, usage);
  if (resolved.face)
    detail += ". Using " + std::string(resolved.face->family) +
              (resolved.face->bold ? " Bold" : resolved.face->italic ? "" : " Regular") +
              (resolved.face->italic ? " Italic" : "");
  return std::string(family.empty() ? kDefaultFamily : DecodeFontReference(family).family) +
         (bold ? " Bold" : italic ? "" : " Regular") +
         (italic ? " Italic" : "") + ": " + detail + ".";
}

std::filesystem::path BundledFontPath(const std::filesystem::path &root, bool bold) {
  return root / "fonts" / kApprovals[bold].filename;
}

FontPair::FontPair(const std::filesystem::path &root, std::string_view family)
    : resourceRoot(root), catalog(GetFontCatalog(root)),
      regularFace(catalog->Resolve(family, false).face), boldFace(catalog->Resolve(family, true).face),
      regular(Metrics(regularFace)), bold(Metrics(boldFace)) {}

FontPair::FontPair(std::shared_ptr<const FontCatalog> snapshot)
    : catalog(std::move(snapshot)), regularFace(catalog->Resolve({}, false).face),
      boldFace(catalog->Resolve({}, true).face), regular(Metrics(regularFace)), bold(Metrics(boldFace)) {}
} // namespace typography
