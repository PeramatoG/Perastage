#pragma once

#if defined(_MSVC_LANG)
#define PERASTAGE_CXX_STD _MSVC_LANG
#else
#define PERASTAGE_CXX_STD __cplusplus
#endif

#if PERASTAGE_CXX_STD >= 201703L
#include <optional>
namespace perastage {
template <typename T>
using Optional = std::optional<T>;
using std::nullopt;
} // namespace perastage
#elif __has_include(<experimental/optional>)
#include <experimental/optional>
namespace perastage {
template <typename T>
using Optional = std::experimental::optional<T>;
constexpr auto nullopt = std::experimental::nullopt;
} // namespace perastage
#else
#error "Perastage requires <optional> or <experimental/optional> support."
#endif

#undef PERASTAGE_CXX_STD
