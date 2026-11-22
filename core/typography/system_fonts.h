#pragma once

#include "font_metrics.h"

namespace typography {
enum class FontOutline { TrueType, Cff };
enum class EmbeddingRight { Unknown, Restricted, Installable, PreviewPrint, Editable };

struct EmbeddingPolicy {
  EmbeddingRight right = EmbeddingRight::Unknown;
  bool noSubsetting = false;
  bool bitmapOnly = false;
  bool PermitsPdf() const {
    return !bitmapOnly && (right == EmbeddingRight::Installable ||
        right == EmbeddingRight::Editable);
  }
};

EmbeddingPolicy ClassifyEmbedding(unsigned version, unsigned flags);

inline constexpr unsigned kFindFaceByPostScriptName = 0x10000;

struct SystemFontRecord {
  std::filesystem::path path;
  unsigned index = 0;
  std::string family, subfamily, postScriptName;
  int weight = 400, width = 5;
  bool italic = false;
  bool operator==(const SystemFontRecord &) const = default;
};

struct FontResource;
std::vector<SystemFontRecord> DiscoverSystemFonts(bool refresh = false);
FontIssue LoadSystemFont(const SystemFontRecord &, FontResource &);
}
