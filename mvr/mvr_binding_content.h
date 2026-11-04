#pragma once
#include <string_view>
namespace mvr {
// XML whitespace is exactly space, tab, carriage return and line feed.
inline bool HasBindingContent(const char *value) {
  return value && std::string_view(value).find_first_not_of(" \t\r\n") !=
                      std::string_view::npos;
}
} // namespace mvr
