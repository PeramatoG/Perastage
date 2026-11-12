#pragma once

#include <functional>
#include <string_view>

namespace layouts::composition {

// Document-local points (72 per inch), origin at the frame's top left, y down.
// Resource bounds can use their own units; a plan supplies their point scale.
struct Point { double x = 0; double y = 0; };
struct Rect { double x = 0; double y = 0; double width = 0; double height = 0; };
enum class TextRole { Normal, Bold };
struct TextMeasurement {
  double width = 0;
  double height = 0;
  double baseline = 0; // Distance below the text cell's top.
};

// UTF-8 input, font size in document points, explicit weight. No toolkit state,
// encoding conversion, resources or global services belong in the composer.
using TextMetrics = std::function<TextMeasurement(std::string_view, double, TextRole)>;

} // namespace layouts::composition
