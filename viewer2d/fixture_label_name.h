#pragma once
#include <string>
#include <string_view>

namespace viewer2d {
// Legacy fixture names separate each pair of space-delimited words with a line break.
// This is a content rule, independent of font metrics, zoom and rendering backend.
inline std::string WrapFixtureLabelName(std::string_view name) {
  std::string result;
  size_t words = 0, position = 0;
  while ((position = name.find_first_not_of(' ', position)) != std::string_view::npos) {
    const auto end = name.find(' ', position);
    if (words) result += words % 2 == 0 ? '\n' : ' ';
    result += name.substr(position, end == std::string_view::npos ? end : end - position);
    ++words;
    if (end == std::string_view::npos) break;
    position = end;
  }
  return result;
}
} // namespace viewer2d
