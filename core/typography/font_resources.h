#pragma once

#include "system_fonts.h"
#include "font_reference.h"
#include <memory>
#include <string_view>
#include <vector>

namespace typography {
inline constexpr std::string_view kDefaultFamily = "PerastageSans";

enum class FontSource { Bundled, ApprovedSystem, InstalledSystem };

struct FontResource {
  std::string family, version, postScriptName, fingerprint, license, provenance;
  FontSource source = FontSource::Bundled;
  bool bold = false, italic = false;
  std::filesystem::path path;
  TtfFontMetrics metrics;
  std::string subfamily;
  int weight = 400, width = 5;
  FontOutline outline = FontOutline::TrueType;
  bool collection = false;
  EmbeddingPolicy embedding;
  bool CanEmbedPdf() const {
    return source != FontSource::InstalledSystem ||
        (outline == FontOutline::TrueType && !collection && embedding.PermitsPdf());
  }
};

struct FontResolution {
  std::shared_ptr<const FontResource> face;
  FontIssue issue = FontIssue::None;
};

class SystemFontCache;

class FontCatalog {
public:
  explicit FontCatalog(const std::filesystem::path &root);
  FontCatalog(const std::filesystem::path &root,
              const std::vector<std::filesystem::path> &systemRoots);
  FontCatalog(const std::filesystem::path &root,
              const std::vector<std::filesystem::path> &systemRoots,
              std::vector<SystemFontRecord> systemFonts);
  FontResolution Resolve(std::string_view family, bool bold, bool italic = false,
                         FontUsage usage = FontUsage::Local) const;
  std::shared_ptr<const FontResource> FindResource(std::string_view id) const;
  std::vector<std::string> SystemFamilies() const;
  std::string ReferenceForFamily(std::string_view family) const;
  std::vector<std::string_view> Families() const;
  const std::string &Signature() const { return signature_; }
  bool IsComplete() const;

private:
  std::vector<std::shared_ptr<const FontResource>> faces_;
  std::shared_ptr<SystemFontCache> system_;
  std::vector<FontIssue> issues_;
  std::string signature_;
};

enum class FontRefresh { Cached, Incomplete, All };
// Refresh is for startup, project open or an explicit request, outside paint.
std::shared_ptr<const FontCatalog> GetFontCatalog(const std::filesystem::path &root,
                                                  FontRefresh refresh = FontRefresh::Cached);

std::string DescribeFontIssue(const FontCatalog &catalog, std::string_view family,
                              bool bold, FontIssue issue, bool italic = false,
                              FontUsage usage = FontUsage::Local);

std::filesystem::path BundledFontPath(const std::filesystem::path &root, bool bold);

// Compatibility view for the legacy fixture stack and composition clients.
struct FontPair {
  const std::filesystem::path resourceRoot;
  const std::shared_ptr<const FontCatalog> catalog;
  const std::shared_ptr<const FontResource> regularFace, boldFace;
  const TtfFontMetrics &regular, &bold;
  explicit FontPair(const std::filesystem::path &root,
                    std::string_view family = kDefaultFamily);
  explicit FontPair(std::shared_ptr<const FontCatalog> catalog);
};
} // namespace typography
