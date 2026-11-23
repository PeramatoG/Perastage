#include "system_fonts.h"
#include "font_resources.h"
#include "file_import_utils.h"
#include <algorithm>
#include <fstream>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include FT_TRUETYPE_TABLES_H
#include FT_ADVANCES_H
#include FT_SFNT_NAMES_H
#include <unicode/unistr.h>

namespace typography {
EmbeddingPolicy ClassifyEmbedding(unsigned version, unsigned flags) {
  // OS/2 versions 0 and 1 predate the subsetting and outline flags.
  // Versions 0 through 2 use the least restrictive of multiple permission bits.
  EmbeddingPolicy result;
  const auto permissions = flags & 0x000e;
  if (version < 2) flags &= 0x000fU;
  constexpr auto allowed = 0x030eU;
  if (version > 5 || (flags & ~allowed) ||
      (version >= 3 && permissions && (permissions & (permissions - 1)))) return result;
  result.noSubsetting = (flags & 0x0100) != 0;
  result.bitmapOnly = (flags & 0x0200) != 0;
  if (permissions & 0x0008) result.right = EmbeddingRight::Editable;
  else if (permissions & 0x0004) result.right = EmbeddingRight::PreviewPrint;
  else if (permissions & 0x0002) result.right = EmbeddingRight::Restricted;
  else result.right = EmbeddingRight::Installable;
  return result;
}

namespace {
struct Library {
  FT_Library ptr = nullptr;
  Library() { FT_Init_FreeType(&ptr); }
  ~Library() { if (ptr) FT_Done_FreeType(ptr); }
};

bool HasTable(FT_Face face, FT_ULong tag) {
  FT_ULong size = 0;
  return !FT_Load_Sfnt_Table(face, tag, 0, nullptr, &size) && size;
}

std::string Metadata(FT_Face face, unsigned id) {
  FT_SfntName name{};
  const auto count = FT_Get_Sfnt_Name_Count(face);
  for (FT_UInt i = 0; i < count; ++i) {
    if (FT_Get_Sfnt_Name(face, i, &name) || name.name_id != id ||
        (name.platform_id != 0 && name.platform_id != 3) ||
        name.string_len > 8192 || name.string_len % 2) continue;
    icu::UnicodeString utf16;
    for (FT_UInt j = 0; j < name.string_len; j += 2)
      utf16.append(static_cast<char16_t>((unsigned(name.string[j]) << 8) | name.string[j + 1]));
    std::string text;
    utf16.toUTF8String(text);
    return text;
  }
  return {};
}

bool ReadExternalMetrics(FT_Face face, TtfFontMetrics &metrics) {
  if (FT_Select_Charmap(face, FT_ENCODING_UNICODE)) return false;
  const auto *horizontal = static_cast<const TT_HoriHeader *>(FT_Get_Sfnt_Table(face, ft_sfnt_hhea));
  const auto *os2 = static_cast<const TT_OS2 *>(FT_Get_Sfnt_Table(face, ft_sfnt_os2));
  const auto *post = static_cast<const TT_Postscript *>(FT_Get_Sfnt_Table(face, ft_sfnt_post));
  metrics.unitsPerEm = face->units_per_EM;
  metrics.ascent = horizontal ? horizontal->Ascender : face->ascender;
  metrics.descent = horizontal ? horizontal->Descender : face->descender;
  metrics.lineGap = horizontal ? horizontal->Line_Gap : face->height - face->ascender + face->descender;
  metrics.capHeight = os2 && os2->version >= 2 && os2->sCapHeight ? os2->sCapHeight : metrics.ascent;
  metrics.italicAngle = post ? post->italicAngle / 65536.0 : 0;
  metrics.xMin = face->bbox.xMin;
  metrics.yMin = face->bbox.yMin;
  metrics.xMax = face->bbox.xMax;
  metrics.yMax = face->bbox.yMax;
  metrics.glyphAdvances.resize(face->num_glyphs);
  for (FT_UInt glyph = 0; glyph < metrics.glyphAdvances.size(); ++glyph) {
    FT_Fixed advance = 0;
    if (FT_Get_Advance(face, glyph, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP, &advance))
      return false;
    metrics.glyphAdvances[glyph] = advance;
  }
  FT_UInt glyph = 0;
  for (auto cp = FT_Get_First_Char(face, &glyph); glyph && cp <= 0x10ffff;
       cp = FT_Get_Next_Char(face, cp, &glyph)) {
    if (glyph < metrics.glyphAdvances.size())
      metrics.coverage.emplace_back(cp, static_cast<uint16_t>(glyph));
  }
  return !metrics.coverage.empty();
}
}

FontIssue LoadSystemFont(const SystemFontRecord &record, FontResource &out) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(record.path, ec)) return FontIssue::Inaccessible;
  const auto size = std::filesystem::file_size(record.path, ec);
  if (ec) return FontIssue::Inaccessible;
  if (size < 12) return FontIssue::Invalid;
  if (size > 32 * 1024 * 1024 || record.index > kFindFaceByPostScriptName) return FontIssue::UnsupportedFormat;
  std::ifstream stream(record.path, std::ios::binary);
  std::string data(static_cast<size_t>(size), '\0');
  if (!stream.read(data.data(), static_cast<std::streamsize>(size)) ||
      stream.peek() != std::char_traits<char>::eof()) return FontIssue::Inaccessible;

  const bool collection = data.compare(0, 4, "ttcf") == 0;
  const bool openType = data.compare(0, 4, "OTTO") == 0;
  const bool trueType = data.compare(0, 4, "\0\1\0\0", 4) == 0;
  if (!collection && !openType && !trueType) return FontIssue::UnsupportedFormat;
  thread_local Library library;
  using Face = std::unique_ptr<std::remove_pointer_t<FT_Face>, decltype(&FT_Done_Face)>;
  const auto open = [&](unsigned index) {
    FT_Face raw = nullptr;
    if (library.ptr)
      FT_New_Memory_Face(library.ptr, reinterpret_cast<const FT_Byte *>(data.data()), data.size(), index, &raw);
    return Face(raw, FT_Done_Face);
  };
  auto index = record.index == kFindFaceByPostScriptName ? 0u : record.index;
  auto face = open(index);
  if (!face) return FontIssue::Invalid;
  if (record.index == kFindFaceByPostScriptName) {
    const auto count = std::min<FT_Long>(face->num_faces, 1024);
    for (; index < count; ++index) {
      if (index) face = open(index);
      const auto *name = face ? FT_Get_Postscript_Name(face.get()) : nullptr;
      if (name && record.postScriptName == name) break;
    }
    if (index >= count) return FontIssue::FaceUnavailable;
  }
  const auto raw = face.get();
  const std::string_view format = FT_Get_Font_Format(raw) ? FT_Get_Font_Format(raw) : "";
  if (!FT_IS_SFNT(raw) || !FT_IS_SCALABLE(raw) || FT_HAS_MULTIPLE_MASTERS(raw) ||
      HasTable(raw, FT_MAKE_TAG('f','v','a','r')) || FT_HAS_COLOR(raw) ||
      (format != "TrueType" && format != "CFF"))
    return FontIssue::UnsupportedFormat;
  if (!raw->units_per_EM || raw->num_glyphs <= 0 || raw->num_glyphs > 65535)
    return FontIssue::Invalid;

  const auto *ps = FT_Get_Postscript_Name(raw);
  if (!ps || !*ps || (!record.postScriptName.empty() && record.postScriptName != ps))
    return FontIssue::FaceUnavailable;
  auto &metrics = out.metrics;
  if (!ReadExternalMetrics(raw, metrics)) return FontIssue::Invalid;
  const auto *os2 = static_cast<const TT_OS2 *>(FT_Get_Sfnt_Table(raw, ft_sfnt_os2));
  out.family = record.family;
  out.subfamily = record.subfamily;
  out.postScriptName = ps;
  out.weight = os2 ? os2->usWeightClass : record.weight;
  out.width = os2 ? os2->usWidthClass : record.width;
  out.bold = out.weight >= 600 || (raw->style_flags & FT_STYLE_FLAG_BOLD);
  out.italic = (raw->style_flags & FT_STYLE_FLAG_ITALIC) || record.italic;
  out.source = FontSource::InstalledSystem;
  out.outline = format == "CFF" ? FontOutline::Cff : FontOutline::TrueType;
  out.collection = collection;
  out.embedding = os2 && os2->version != 0xffff ? ClassifyEmbedding(os2->version, os2->fsType) : EmbeddingPolicy{};
  out.version = Metadata(raw, 5);
  out.license = Metadata(raw, 13);
  out.provenance = Metadata(raw, 14);
  out.path = record.path;
  out.fingerprint = FileImportUtils::ComputeBytesSha256(data.data(), data.size());
  if (collection) out.fingerprint += ":" + std::to_string(index);
  metrics.resourceId = out.fingerprint;
  metrics.faceIndex = index;
  metrics.data = std::move(data);
  metrics.valid = true;
  return FontIssue::None;
}
}
