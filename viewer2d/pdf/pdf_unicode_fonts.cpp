#include "pdf_unicode_fonts.h"
#include "typography/glyph_outline.h"
#include <algorithm>
#include <cmath>
#include "typography/unicode_shaping.h"

namespace layout_pdf_internal {
namespace {
std::string Hex16(uint16_t value) {
  constexpr std::string_view digits = "0123456789ABCDEF";
  std::string result(4, '0');
  for (int i = 3; i >= 0; --i) {
    result[i] = digits[value & 0xf];
    value >>= 4;
  }
  return result;
}

std::string Utf16Hex(uint32_t scalar) {
  if (scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff)) return "FFFD";
  if (scalar <= 0xffff) return Hex16(static_cast<uint16_t>(scalar));
  scalar -= 0x10000;
  return Hex16(0xd800 + (scalar >> 10)) + Hex16(0xdc00 + (scalar & 0x3ff));
}

std::string SequenceHex(std::string_view value) {
  std::string sequence;
  for (const auto unit : typography::DecodeUtf16(value)) sequence += Hex16(unit);
  return sequence;
}

std::string PdfFontName(std::string_view postScript) {
  constexpr std::string_view hex = "0123456789ABCDEF";
  std::string name;
  for (const unsigned char ch : postScript) {
    if (ch > 32 && ch < 127 && std::string_view("#%()/<>[]{}").find(ch) == std::string_view::npos) {
      name += static_cast<char>(ch);
    } else {
      name += '#';
      name += hex[ch >> 4];
      name += hex[ch & 15];
    }
  }
  return name;
}

size_t AppendObject(std::vector<PdfObject> &objects, std::string body) {
  objects.push_back({std::move(body)});
  return objects.size();
}

size_t AppendStream(std::vector<PdfObject> &objects, const std::string &data,
                    const std::string &extra = {}) {
  return AppendObject(objects, "<< /Length " + std::to_string(data.size()) +
      extra + " >>\nstream\n" + data + "\nendstream");
}
}

void BeginActualText(std::ostringstream &out, std::string_view text) {
  out << "/Span << /ActualText <FEFF" << SequenceHex(text) << "> >> BDC\n";
}

PdfUnicodeFonts::PdfUnicodeFonts(std::shared_ptr<const typography::FontCatalog> catalog)
    : catalog_(std::move(catalog)) {}

void PdfUnicodeFonts::AddDiagnostic(const std::string &message) {
  if (!message.empty() && diagnostics_.size() < 128) diagnostics_.insert(message);
}

void PdfUnicodeFonts::AppendGlyph(std::ostringstream &out, const FloatFormatter &fmt,
    const typography::PositionedGlyph &glyph, double x, double y, double size, double radius) {
  const auto resource = catalog_->FindResource(glyph.resourceId);
  if (!resource || !resource->CanEmbedPdf()) {
    AddDiagnostic("A PDF glyph references a font that cannot be embedded.");
    return;
  }
  const auto outline = typography::ResolveGlyphOutline(*catalog_, glyph.resourceId, glyph.glyphId);
  if (!outline || outline->substituted)
    AddDiagnostic("A glyph contour is invalid in " + std::string(resource->postScriptName) +
                  ". Using that face's missing-glyph contour.");
  if (radius > 0) {
    if (!outline || outline->path.empty()) return;
    out << "q\n" << fmt.Format(2 * radius) << " w\n1 J\n1 j\n";
    double currentX = 0, currentY = 0;
    const auto point = [&](double px, double py) {
      out << fmt.Format(x + px * size) << ' ' << fmt.Format(y + py * size);
    };
    for (const auto &p : outline->path) {
      using Kind = typography::GlyphPathElement::Kind;
      switch (p.kind) {
      case Kind::Move: point(p.x, p.y); out << " m\n"; break;
      case Kind::Line: point(p.x, p.y); out << " l\n"; break;
      case Kind::Quadratic:
        point(currentX + (p.cx - currentX) * (2.0 / 3), currentY + (p.cy - currentY) * (2.0 / 3));
        out << ' ';
        point(p.x + (p.cx - p.x) * (2.0 / 3), p.y + (p.cy - p.y) * (2.0 / 3));
        out << ' ';
        point(p.x, p.y); out << " c\n";
        break;
      case Kind::Cubic:
        point(p.cx, p.cy); out << ' '; point(p.dx, p.dy); out << ' ';
        point(p.x, p.y); out << " c\n";
        break;
      case Kind::Close: out << "h\n"; break;
      }
      currentX = p.x;
      currentY = p.y;
    }
    out << "S\nQ\n";
    return;
  }
  auto [it, inserted] = used_.try_emplace(resource->fingerprint);
  auto &face = it->second;
  if (inserted) {
    face.resource = resource;
    face.key = "F" + std::to_string(used_.size() + 2);
  }
  const auto mapping = std::pair{outline && !outline->substituted && glyph.glyphId < resource->metrics.glyphAdvances.size() ? glyph.glyphId : uint16_t{0}, glyph.clusterContinuation ? std::string()
      : glyph.text.empty() ? Utf16Hex(glyph.codepoint) : SequenceHex(glyph.text)};
  auto code = face.codes.find(mapping);
  if (code == face.codes.end()) {
    if (face.glyphs.size() == 65535) {
      AddDiagnostic("A PDF font exceeded the Unicode character mapping limit.");
      return;
    }
    face.glyphs.push_back(mapping);
    code = face.codes.emplace(mapping, static_cast<uint16_t>(face.glyphs.size())).first;
  }
  out << "BT\n/" << face.key << ' ' << fmt.Format(size) << " Tf\n"
      << "1 0 0 1 " << fmt.Format(x) << ' ' << fmt.Format(y)
      << " Tm\n<" << Hex16(code->second) << "> Tj\nET\n";
}

void PdfUnicodeFonts::AppendRun(std::ostringstream &out, const FloatFormatter &fmt,
    const typography::Composition &run, double x, double y) {
  if (!run.unsupportedBytes.empty()) AddDiagnostic("Some text characters are unavailable in the approved fonts.");
  for (const auto &line : run.lines) {
    AppendTextClusters(out, line.first, line.first + line.count,
        [&](size_t i) -> const auto & { return run.glyphs[i].text; },
        [&](size_t i) { return run.glyphs[i].clusterContinuation; },
        [&](size_t a, size_t b) { return !run.glyphs[a].rightToLeft && !run.glyphs[b].rightToLeft; },
        [&](size_t i) {
          const auto &glyph = run.glyphs[i];
          AppendGlyph(out, fmt, glyph, x + glyph.x, y + glyph.baseline, run.style.size);
        });
  }
}

void PdfUnicodeFonts::WriteResources(std::vector<PdfObject> &objects, std::ostringstream &out) const {
  for (const auto &[id, face] : used_) {
    const auto &font = face.resource->metrics;
    const auto name = PdfFontName(face.resource->postScriptName);
    const auto metric = [&](int units) { return std::lround(units * 1000.0 / font.unitsPerEm); };
    const auto length = " /Length1 " + std::to_string(font.data.size());
    std::string compressed, error;
    const auto file = PdfDeflater::Compress(font.data, compressed, error)
        ? AppendStream(objects, compressed, length + " /Filter /FlateDecode")
        : AppendStream(objects, font.data, length);
    std::ostringstream descriptor;
    descriptor << "<< /Type /FontDescriptor /FontName /" << name
               << " /Flags " << (4 + (face.resource->italic ? 64 : 0) + (face.resource->family == "Noto Serif" ? 2 : 0))
               << " /ItalicAngle " << FloatFormatter(3).Format(font.italicAngle)
               << " /StemV " << (face.resource->bold ? 120 : 80)
               << " /FontBBox [" << metric(font.xMin) << ' ' << metric(font.yMin) << ' '
               << metric(font.xMax) << ' ' << metric(font.yMax) << "] /Ascent "
               << metric(font.ascent) << " /Descent " << metric(font.descent)
               << " /CapHeight " << metric(font.capHeight) << " /FontFile2 " << file << " 0 R >>";
    const auto descriptorId = AppendObject(objects, descriptor.str());

    std::string gidMap(2, '\0'); // CID zero is .notdef.
    std::ostringstream widths;
    widths.imbue(std::locale::classic());
    FloatFormatter fmt(6);
    for (const auto &[gid, scalar] : face.glyphs) {
      gidMap.push_back(static_cast<char>(gid >> 8));
      gidMap.push_back(static_cast<char>(gid & 0xff));
      widths << fmt.Format(font.glyphAdvances[gid] * 1000.0 / font.unitsPerEm) << ' ';
    }
    const auto gidId = AppendStream(objects, gidMap);
    const std::string identity = " /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >>";
    std::ostringstream descendant;
    descendant << "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /" << name << identity
               << " /FontDescriptor " << descriptorId << " 0 R /CIDToGIDMap " << gidId
               << " 0 R /W [1 [" << widths.str() << "]] >>";
    const auto descendantId = AppendObject(objects, descendant.str());

    std::ostringstream cmap;
    cmap << "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n"
         << "/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
         << "/CMapName /Perastage-UCS def\n/CMapType 2 def\n"
         << "1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n";
    std::vector<size_t> mapped;
    for (size_t i = 0; i < face.glyphs.size(); ++i)
      if (!face.glyphs[i].second.empty()) mapped.push_back(i);
    for (size_t first = 0; first < mapped.size(); first += 100) {
      const auto count = std::min<size_t>(100, mapped.size() - first);
      cmap << count << " beginbfchar\n";
      for (size_t j = first; j < first + count; ++j) {
        const auto i = mapped[j];
        cmap << '<' << Hex16(static_cast<uint16_t>(i + 1)) << "> <"
             << face.glyphs[i].second << ">\n";
      }
      cmap << "endbfchar\n";
    }
    cmap << "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n";
    const auto unicodeId = AppendStream(objects, cmap.str());
    std::ostringstream type0;
    type0 << "<< /Type /Font /Subtype /Type0 /BaseFont /" << name
          << " /Encoding /Identity-H /DescendantFonts [" << descendantId
          << " 0 R] /ToUnicode " << unicodeId << " 0 R >>";
    out << " /" << face.key << ' ' << AppendObject(objects, type0.str()) << " 0 R";
  }
}
}
