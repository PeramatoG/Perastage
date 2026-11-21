#pragma once

#include "font_metrics.h"
#include <memory>
#include <string_view>
#include <vector>

namespace typography {
inline constexpr std::string_view kDefaultFamily = "PerastageSans";

enum class FontSource { Bundled, ApprovedSystem };

struct FontResource {
  std::string_view family, version, postScriptName, fingerprint, license, provenance;
  FontSource source = FontSource::Bundled;
  bool bold = false, italic = false;
  std::filesystem::path path;
  TtfFontMetrics metrics;
};

struct FontResolution {
  const FontResource *face = nullptr;
  FontIssue issue = FontIssue::None;
};

class FontCatalog {
public:
  explicit FontCatalog(const std::filesystem::path &root);
  FontCatalog(const std::filesystem::path &root,
              const std::vector<std::filesystem::path> &systemRoots);
  FontResolution Resolve(std::string_view family, bool bold, bool italic = false) const;
  const FontResource *FindResource(std::string_view id) const;
  std::vector<std::string_view> Families() const;
  const std::string &Signature() const { return signature_; }
  bool IsComplete() const;

private:
  std::vector<FontResource> faces_;
  std::vector<FontIssue> issues_;
  std::string signature_;
};

// Application assets are immutable for a session. Construct a new catalog to
// re-evaluate resources after an installation or when inspecting another root.
std::shared_ptr<const FontCatalog> GetFontCatalog(const std::filesystem::path &root,
                                                  bool refreshIncomplete = false);

std::string DescribeFontIssue(const FontCatalog &catalog, std::string_view family,
                              bool bold, FontIssue issue, bool italic = false);

std::filesystem::path BundledFontPath(const std::filesystem::path &root, bool bold);

// Compatibility view for the legacy fixture stack and composition clients.
struct FontPair {
  const std::filesystem::path resourceRoot;
  const std::shared_ptr<const FontCatalog> catalog;
  const TtfFontMetrics &regular, &bold;
  explicit FontPair(const std::filesystem::path &root,
                    std::string_view family = kDefaultFamily);
};
} // namespace typography
