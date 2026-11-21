#pragma once

#include "composition.h"
#include <array>
#include <optional>
#include <set>

namespace typography {
struct TextBlock {
  enum class Alignment { Left, Center, Right, Justified };
  struct Run {
    std::string text;
    int fontSize = 12;
    bool bold = false;
    bool italic = false;
    std::string family;
    std::array<uint8_t, 3> color{};
    bool underline = false, strikethrough = false;
  };
  struct Line {
    std::vector<Run> runs;
    std::optional<Alignment> alignment;
    double spaceBefore = 0, spaceAfter = 0, lineSpacing = 1;
    double leftIndent = 0, rightIndent = 0, firstLineIndent = 0;
  };
  Alignment alignment = Alignment::Left;
  int fontSize = 12;
  bool bold = false, italic = false;
  bool wrap = false;
  bool containsUnsupportedObjects = false;
  std::vector<Line> lines;
};

struct TextBlockPlan {
  struct Glyph {
    PositionedGlyph glyph;
    double x, baseline, size;
    std::array<uint8_t, 3> color;
  };
  struct Decoration {
    double x, y, width, height;
    std::array<uint8_t, 3> color;
  };
  std::vector<Glyph> glyphs;
  std::vector<Decoration> decorations;
  std::set<std::string> diagnostics;
};
TextBlockPlan ComposeTextBlock(const TextBlock &, double width, double height,
                              const FontCatalog &);
}
