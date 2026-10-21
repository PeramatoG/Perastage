#include "glyph_outline.h"
#include <stdexcept>

namespace typography {
namespace {
struct Reader {
  const std::string &data;
  size_t pos, end;
  void Skip(size_t count) {
    if (pos > end || count > end - pos || end > data.size())
      throw std::runtime_error("Invalid TrueType glyph contour bounds");
    pos += count;
  }
  unsigned Byte() { Skip(1); return static_cast<unsigned char>(data[pos - 1]); }
  unsigned Word() { const auto high = Byte(); return (high << 8) | Byte(); }
  int SignedWord() { return static_cast<int16_t>(Word()); }
  uint32_t Dword() { const auto high = Word(); return (high << 16) | Word(); }
  double Fixed() { return SignedWord() / 16384.0; }
};
struct Point { double x = 0, y = 0; bool on = false; };

void AppendContour(std::vector<GlyphPathElement> &path, std::vector<Point> points) {
  if (points.empty()) return;
  double area = 0;
  for (size_t i = 0; i < points.size(); ++i) {
    const auto &p = points[i], &next = points[(i + 1) % points.size()];
    area += p.x * next.y - next.x * p.y;
  }
  Point start;
  if (points.front().on) {
    start = points.front();
    points.erase(points.begin());
  } else if (points.back().on) {
    start = points.back();
    points.pop_back();
  } else {
    start = {(points.front().x + points.back().x) / 2,
             (points.front().y + points.back().y) / 2, true};
  }
  path.push_back({GlyphPathElement::Kind::Move, start.x, start.y});
  points.push_back(start);
  for (size_t i = 0; i < points.size();) {
    const auto p = points[i++];
    if (p.on) {
      path.push_back({GlyphPathElement::Kind::Line, p.x, p.y});
    } else {
      const auto next = points.at(i);
      const auto end = next.on ? next : Point{(p.x + next.x) / 2, (p.y + next.y) / 2, true};
      path.push_back({GlyphPathElement::Kind::Quadratic, end.x, end.y, p.x, p.y});
      if (next.on) ++i;
    }
  }
  path.push_back({GlyphPathElement::Kind::Close, 0, 0, 0, 0, area > 0});
}

class OutlineReader {
public:
  explicit OutlineReader(const TtfFontMetrics &font) : font_(font) {
    uint32_t head = 0, length = 0;
    if (!font.valid || font.unitsPerEm <= 0 ||
        !FindTable(font.data, 0x68656164, head, length) || length < 54 ||
        !FindTable(font.data, 0x6c6f6361, loca_, locaLength_) ||
        !FindTable(font.data, 0x676c7966, glyf_, glyfLength_))
      throw std::runtime_error("Bundled font has no TrueType contours");
    Reader r{font.data, head + 50, font.data.size()};
    longOffsets_ = r.Word() != 0;
  }
  std::vector<GlyphPathElement> Read(unsigned glyph, unsigned depth = 0) {
    if (depth > 12) throw std::runtime_error("Recursive TrueType glyph component");
    const unsigned stride = longOffsets_ ? 4 : 2;
    if ((static_cast<size_t>(glyph) + 2) * stride > locaLength_)
      throw std::runtime_error("Invalid TrueType glyph index");
    Reader locations{font_.data, loca_ + glyph * stride, font_.data.size()};
    const auto first = longOffsets_ ? locations.Dword() : locations.Word() * 2;
    const auto last = longOffsets_ ? locations.Dword() : locations.Word() * 2;
    if (first > last || last > glyfLength_)
      throw std::runtime_error("Invalid TrueType glyph location");
    if (first == last) return {};
    Reader r{font_.data, glyf_ + first, glyf_ + last};
    const int contours = r.SignedWord();
    r.Skip(8);
    if (contours >= 0) return ReadSimple(r, contours);
    std::vector<GlyphPathElement> result;
    unsigned flags;
    do {
      flags = r.Word();
      const unsigned component = r.Word();
      const int x = flags & 1 ? r.SignedWord() : static_cast<int8_t>(r.Byte());
      const int y = flags & 1 ? r.SignedWord() : static_cast<int8_t>(r.Byte());
      if (!(flags & 2))
        throw std::runtime_error("Unsupported TrueType point-anchored component");
      double a = 1, b = 0, c = 0, d = 1;
      if (flags & 8) a = d = r.Fixed();
      else if (flags & 64) { a = r.Fixed(); d = r.Fixed(); }
      else if (flags & 128) { a = r.Fixed(); b = r.Fixed(); c = r.Fixed(); d = r.Fixed(); }
      const double dx = flags & 0x800 ? a * x + c * y : x;
      const double dy = flags & 0x800 ? b * x + d * y : y;
      for (auto p : Read(component, depth + 1)) {
        const auto transform = [&](double &px, double &py) {
          const double tx = a * px + c * py + dx;
          py = b * px + d * py + dy;
          px = tx;
        };
        if (p.kind != GlyphPathElement::Kind::Close) transform(p.x, p.y);
        if (p.kind == GlyphPathElement::Kind::Quadratic) transform(p.cx, p.cy);
        result.push_back(p);
      }
    } while (flags & 32);
    return result;
  }
private:
  std::vector<GlyphPathElement> ReadSimple(Reader &r, unsigned contours) {
    std::vector<unsigned> ends;
    for (unsigned i = 0; i < contours; ++i) {
      const auto end = r.Word();
      if (!ends.empty() && end <= ends.back()) throw std::runtime_error("Invalid contour endpoints");
      ends.push_back(end);
    }
    const auto instructions = r.Word();
    r.Skip(instructions);
    const unsigned count = ends.empty() ? 0 : ends.back() + 1;
    std::vector<unsigned> flags;
    while (flags.size() < count) {
      const auto flag = r.Byte();
      const unsigned repeat = flag & 8 ? r.Byte() : 0;
      if (repeat + 1 > count - flags.size()) throw std::runtime_error("Invalid repeated glyph flags");
      flags.insert(flags.end(), repeat + 1, flag);
    }
    std::vector<Point> points(count);
    for (int axis = 0; axis < 2; ++axis) {
      int coordinate = 0;
      for (unsigned i = 0; i < count; ++i) {
        const unsigned shortFlag = axis == 0 ? 2 : 4, sameFlag = axis == 0 ? 16 : 32;
        const int delta = flags[i] & shortFlag ? static_cast<int>(r.Byte()) * (flags[i] & sameFlag ? 1 : -1)
                         : flags[i] & sameFlag ? 0 : r.SignedWord();
        coordinate += delta;
        if (axis == 0) points[i].x = coordinate; else points[i].y = coordinate;
        points[i].on = (flags[i] & 1) != 0;
      }
    }
    std::vector<GlyphPathElement> result;
    unsigned first = 0;
    for (auto last : ends) {
      AppendContour(result, {points.begin() + first, points.begin() + last + 1});
      first = last + 1;
    }
    return result;
  }
  const TtfFontMetrics &font_;
  uint32_t loca_ = 0, locaLength_ = 0, glyf_ = 0, glyfLength_ = 0;
  bool longOffsets_ = false;
};
} // namespace

std::shared_ptr<const GlyphOutline> ReadGlyphOutline(const TtfFontMetrics &font,
                                                    unsigned char winAnsi) {
  auto outline = std::make_shared<GlyphOutline>();
  outline->path = OutlineReader(font).Read(font.glyphIndices[winAnsi]);
  for (auto &p : outline->path) {
    p.x /= font.unitsPerEm; p.y /= font.unitsPerEm;
    p.cx /= font.unitsPerEm; p.cy /= font.unitsPerEm;
  }
  return outline;
}
} // namespace typography
